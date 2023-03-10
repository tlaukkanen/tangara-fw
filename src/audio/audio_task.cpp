#include "audio_task.hpp"

#include <stdlib.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>

#include "cbor.h"
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
static const uint8_t kAudioCore = 0;

auto Start(Pipeline* pipeline) -> Handle* {
  auto input_queue = xQueueCreate(8, 1);

  // Newly created task will free this.
  AudioTaskArgs* args = new AudioTaskArgs{
      .pipeline = pipeline,
      .input = input_queue,
  };

  ESP_LOGI(kTag, "starting audio task");
  xTaskCreatePinnedToCore(&AudioTaskMain, "pipeline", kStackSize, args,
                          kTaskPriorityAudio, NULL, kAudioCore);

  return new Handle(input_queue);
}

void AudioTaskMain(void* args) {
  // Nest the body within an additional scope to ensure that destructors are
  // called before the task quits.
  {
    AudioTaskArgs* real_args = reinterpret_cast<AudioTaskArgs*>(args);
    std::unique_ptr<Pipeline> pipeline(real_args->pipeline);
    QueueHandle_t input;
    StreamBufferHandle_t output;
    delete real_args;

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
                  [](const MappableRegion<kBufferSize>& region) {
                    assert(region.is_valid);
                  });
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
      // TODO: full event here?
      Command cmd;
      bool has_cmd = xQueueReceive(input, &cmd, 0);
      if (has_cmd) {
        switch (cmd) {
          case PLAY:
            playing = true;
            break;
          case PAUSE:
            playing = false;
            break;
          case QUIT:
            quit = true;
            break;
        }
      }
      if (quit) {
        break;
      }

      if (playing) {
        for (int i = 0; i < elements.size(); i++) {
          std::vector<MutableStream> in_streams;
          elements.at(i)->InStreams(&in_regions, &in_streams);
          MutableStream out_stream = elements.at(i)->OutStream(&out_region);

          // Crop the input and output streams to the ranges that are safe to
          // touch. For the input streams, this is the region that contains
          // data. For the output stream, this is the region that does *not*
          // already contain data.
          std::vector<Stream> cropped_in_streams;
          std::for_each(in_streams.begin(), in_streams.end(),
                        [&](MutableStream& s) {
                          cropped_in_streams.emplace_back(
                              s.info, s.data.first(s.info->bytes_in_stream));
                        });

          elements.at(i)->OutputElement()->Process(&cropped_in_streams,
                                                   &out_stream);

          for (int stream = 0; stream < in_streams.size(); stream++) {
            MutableStream& orig_stream = in_streams.at(stream);
            Stream& cropped_stream = cropped_in_streams.at(stream);
            std::move(cropped_stream.data.begin(), cropped_stream.data.end(),
                      orig_stream.data.begin());
            orig_stream.info->bytes_in_stream =
                cropped_stream.data.size_bytes();
          }
        }
      }
    }
  }
  vTaskDelete(NULL);
}

}  // namespace task

}  // namespace audio
