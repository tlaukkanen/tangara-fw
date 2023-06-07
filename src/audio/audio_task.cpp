/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio_task.hpp"

#include <stdlib.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <variant>

#include "audio_events.hpp"
#include "audio_fsm.hpp"
#include "audio_sink.hpp"
#include "cbor.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "event_queue.hpp"
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

// The default amount of time to wait between pipeline iterations for a single
// song.
static constexpr uint_fast16_t kDefaultDelayTicks = pdMS_TO_TICKS(5);
static constexpr uint_fast16_t kMaxDelayTicks = pdMS_TO_TICKS(10);
static constexpr uint_fast16_t kMinDelayTicks = pdMS_TO_TICKS(1);

void AudioTaskMain(std::unique_ptr<Pipeline> pipeline, IAudioSink* sink) {
  // The stream format for bytes currently in the sink buffer.
  std::optional<StreamInfo::Format> output_format;

  // How long to wait between pipeline iterations. This is reset for each song,
  // and readjusted on the fly to maintain a reasonable amount playback buffer.
  // Buffering too much will mean we process samples inefficiently, wasting CPU
  // time, whilst buffering too little will affect the quality of the output.
  uint_fast16_t delay_ticks = kDefaultDelayTicks;

  std::vector<Pipeline*> all_elements = pipeline->GetIterationOrder();

  bool previously_had_work = false;
  events::EventQueue& event_queue = events::EventQueue::GetInstance();
  while (1) {
    // First, see if we actually have any pipeline work to do in this iteration.
    bool has_work = false;
    // We always have work to do if there's still bytes to be sunk.
    has_work = all_elements.back()->OutStream().info->bytes_in_stream > 0;
    if (!has_work) {
      for (Pipeline* p : all_elements) {
        has_work = p->OutputElement()->NeedsToProcess();
        if (has_work) {
          break;
        }
      }
    }

    if (previously_had_work && !has_work) {
      events::Dispatch<AudioPipelineIdle, AudioState>({});
    }
    previously_had_work = has_work;

    // See if there's any new events.
    event_queue.ServiceAudio(has_work ? delay_ticks : portMAX_DELAY);

    if (!has_work) {
      // See if we've been given work by this event.
      for (Pipeline* p : all_elements) {
        has_work = p->OutputElement()->NeedsToProcess();
        if (has_work) {
          delay_ticks = kDefaultDelayTicks;
          break;
        }
      }
      if (!has_work) {
        continue;
      }
    }

    // We have work to do! Allow each element in the pipeline to process one
    // chunk. We iterate from input nodes first, so this should result in
    // samples in the output buffer.

    for (int i = 0; i < all_elements.size(); i++) {
      std::vector<RawStream> raw_in_streams;
      all_elements.at(i)->InStreams(&raw_in_streams);
      RawStream raw_out_stream = all_elements.at(i)->OutStream();

      // Crop the input and output streams to the ranges that are safe to
      // touch. For the input streams, this is the region that contains
      // data. For the output stream, this is the region that does *not*
      // already contain data.
      std::vector<InputStream> in_streams;
      std::for_each(raw_in_streams.begin(), raw_in_streams.end(),
                    [&](RawStream& s) { in_streams.emplace_back(&s); });
      OutputStream out_stream(&raw_out_stream);

      all_elements.at(i)->OutputElement()->Process(in_streams, &out_stream);
    }

    RawStream raw_sink_stream = all_elements.back()->OutStream();
    InputStream sink_stream(&raw_sink_stream);

    if (sink_stream.info().bytes_in_stream == 0) {
      // No new bytes to sink, so skip sinking completely.
      ESP_LOGI(kTag, "no bytes to sink");
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
      } else {
        ESP_LOGI(kTag, "waiting to reconfigure");
        continue;
      }
    }

    // We've reconfigured the sink, or it was already configured correctly.
    // Send through some data.
    std::size_t bytes_sunk =
        xStreamBufferSend(sink->buffer(), sink_stream.data().data(),
                          sink_stream.data().size_bytes(), 0);

    // Adjust how long we wait for the next iteration if we're getting too far
    // ahead or behind.
    float sunk_percent = static_cast<float>(bytes_sunk) /
                         static_cast<float>(sink_stream.info().bytes_in_stream);

    if (sunk_percent > 0.66f) {
      // We're sinking a lot of the output buffer per iteration, so we need to
      // be running faster.
      delay_ticks--;
    } else if (sunk_percent < 0.33f) {
      // We're not sinking much of the output buffer per iteration, so we can
      // slow down to save some cycles.
      delay_ticks++;
    }
    delay_ticks = std::clamp(delay_ticks, kMinDelayTicks, kMaxDelayTicks);

    // Finally, actually mark the bytes we sunk as consumed.
    if (bytes_sunk > 0) {
      sink_stream.consume(bytes_sunk);
    }
  }
}

auto StartPipeline(Pipeline* pipeline, IAudioSink* sink) -> void {
  ESP_LOGI(kTag, "starting audio pipeline task");
  tasks::StartPersistent<tasks::Type::kAudio>(
      [=]() { AudioTaskMain(std::unique_ptr<Pipeline>(pipeline), sink); });
}

}  // namespace task

}  // namespace audio
