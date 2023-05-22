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

void AudioTaskMain(std::unique_ptr<Pipeline> pipeline, IAudioSink* sink) {
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

  while (1) {
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
      std::size_t sent =
          xStreamBufferSend(sink->buffer(), sink_stream.data().data(),
                            sink_stream.data().size_bytes(), 0);
      if (sent > 0) {
        ESP_LOGI(
            kTag, "sunk %u bytes out of %u (%d %%)", sent,
            sink_stream.info().bytes_in_stream,
            (int)(((float)sent / (float)sink_stream.info().bytes_in_stream) *
                  100));
      }
      sink_stream.consume(sent);
    }

    out_region.Unmap();
  }
}

auto StartPipeline(Pipeline* pipeline, IAudioSink* sink) -> void {
  ESP_LOGI(kTag, "starting audio pipeline task");
  tasks::StartPersistent<tasks::Type::kAudio>(
      [=]() { AudioTaskMain(std::unique_ptr<Pipeline>(pipeline), sink); });
}

}  // namespace task

}  // namespace audio
