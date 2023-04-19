#include "audio_task.hpp"

#include <stdlib.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <variant>

#include "audio_sink.hpp"
#include "cbor.h"
#include "dac.hpp"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "pipeline.hpp"
#include "span.hpp"

#include "arena.hpp"
#include "audio_element.hpp"
#include "chunk.hpp"
#include "stream_event.hpp"
#include "stream_info.hpp"
#include "stream_message.hpp"
#include "sys/_stdint.h"
#include "tasks.hpp"

namespace audio {

namespace task {

static const char* kTag = "task";
static const std::size_t kStackSize = 24 * 1024;
static const std::size_t kDrainStackSize = 1024;
static const uint8_t kAudioCore = 0;

auto StartPipeline(Pipeline* pipeline, IAudioSink* sink) -> void {
  // Newly created task will free this.
  AudioTaskArgs* args = new AudioTaskArgs{.pipeline = pipeline, .sink = sink};

  ESP_LOGI(kTag, "starting audio pipeline task");
  xTaskCreatePinnedToCore(&AudioTaskMain, "pipeline", kStackSize, args,
                          kTaskPriorityAudio, NULL, kAudioCore);
}

auto StartDrain(IAudioSink* sink) -> void {
  auto command = new std::atomic<Command>(PLAY);
  // Newly created task will free this.
  AudioDrainArgs* drain_args = new AudioDrainArgs{
      .sink = sink,
      .command = command,
  };

  ESP_LOGI(kTag, "starting audio drain task");
  xTaskCreatePinnedToCore(&AudioDrainMain, "drain", kDrainStackSize, drain_args,
                          kTaskPriorityAudio, NULL, kAudioCore);
}

void AudioTaskMain(void* args) {
  // Nest the body within an additional scope to ensure that destructors are
  // called before the task quits.
  {
    AudioTaskArgs* real_args = reinterpret_cast<AudioTaskArgs*>(args);
    std::unique_ptr<Pipeline> pipeline(real_args->pipeline);
    IAudioSink* sink = real_args->sink;
    delete real_args;

    std::optional<StreamInfo::Format> output_format;

    std::vector<Pipeline*> elements = pipeline->GetIterationOrder();
    std::size_t max_inputs =
        (*std::max_element(elements.begin(), elements.end(),
                           [](Pipeline const* first, Pipeline const* second) {
                             return first->NumInputs() < second->NumInputs();
                           }))
            ->NumInputs();

    // We need to be able to simultaneously map all of an element's inputs, plus
    // its output. So preallocate that many ranges.
    std::vector<MappableRegion<kPipelineBufferSize>> in_regions(max_inputs);
    MappableRegion<kPipelineBufferSize> out_region;
    std::for_each(in_regions.begin(), in_regions.end(),
                  [](const auto& region) { assert(region.is_valid); });
    assert(out_region.is_valid);

    // Each element has exactly one output buffer.
    std::vector<HimemAlloc<kPipelineBufferSize>> buffers(elements.size());
    std::vector<StreamInfo> buffer_infos(buffers.size());
    std::for_each(buffers.begin(), buffers.end(),
                  [](const HimemAlloc<kPipelineBufferSize>& alloc) {
                    assert(alloc.is_valid);
                  });

    bool playing = true;
    bool quit = false;
    while (!quit) {
      if (playing) {
        for (int i = 0; i < elements.size(); i++) {
          std::vector<RawStream> raw_in_streams;
          elements.at(i)->InStreams(&in_regions, &raw_in_streams);
          RawStream raw_out_stream = elements.at(i)->OutStream(&out_region);

          // Crop the input and output streams to the ranges that are safe to
          // touch. For the input streams, this is the region that contains
          // data. For the output stream, this is the region that does *not*
          // already contain data.
          std::vector<InputStream> in_streams;
          std::for_each(raw_in_streams.begin(), raw_in_streams.end(),
                        [&](RawStream& s) { in_streams.emplace_back(&s); });
          OutputStream out_stream(&raw_out_stream);

          elements.at(i)->OutputElement()->Process(in_streams, &out_stream);

          std::for_each(in_regions.begin(), in_regions.end(),
                        [](auto&& r) { r.Unmap(); });
          out_region.Unmap();
        }

        RawStream raw_sink_stream = elements.front()->OutStream(&out_region);
        InputStream sink_stream(&raw_sink_stream);

        if (sink_stream.info().bytes_in_stream == 0) {
          out_region.Unmap();
          vTaskDelay(pdMS_TO_TICKS(100));
          continue;
        }

        if (!output_format || output_format != sink_stream.info().format) {
          // The format of the stream within the sink stream has changed. We
          // need to reconfigure the sink, but shouldn't do so until we've fully
          // drained the current buffer.
          if (xStreamBufferIsEmpty(sink->buffer())) {
            ESP_LOGI(kTag, "reconfiguring dac");
            output_format = sink_stream.info().format;
            sink->Configure(*output_format);
          }
        }

        // We've reconfigured the sink, or it was already configured correctly.
        // Send through some data.
        if (output_format == sink_stream.info().format &&
            !std::holds_alternative<std::monostate>(*output_format)) {
          // TODO: tune the delay on this, as it's currently the only way to
          // throttle this task's CPU time. Maybe also hold off on the pipeline
          // if the buffer is already close to full?
          std::size_t sent = xStreamBufferSend(
              sink->buffer(), sink_stream.data().data(),
              sink_stream.data().size_bytes(), pdMS_TO_TICKS(10));
          sink_stream.consume(sent);
        }

        out_region.Unmap();
      }
    }
  }
  vTaskDelete(NULL);
}

static std::byte sDrainBuf[8 * 1024];

void AudioDrainMain(void* args) {
  {
    AudioDrainArgs* real_args = reinterpret_cast<AudioDrainArgs*>(args);
    IAudioSink* sink = real_args->sink;
    std::atomic<Command>* command = real_args->command;
    delete real_args;

    // TODO(jacqueline): implement PAUSE without busy-waiting.
    while (*command != QUIT) {
      std::size_t len = xStreamBufferReceive(sink->buffer(), sDrainBuf,
                                             sizeof(sDrainBuf), portMAX_DELAY);
      if (len > 0) {
        sink->Send({sDrainBuf, len});
      }
    }
  }
  vTaskDelete(NULL);
}

}  // namespace task

}  // namespace audio
