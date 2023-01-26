#include "audio_task.hpp"

#include <stdlib.h>

#include <cstdint>
#include <deque>
#include <memory>

#include "audio_element_handle.hpp"
#include "cbor.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "span.hpp"

#include "audio_element.hpp"
#include "chunk.hpp"
#include "stream_event.hpp"
#include "stream_info.hpp"
#include "stream_message.hpp"
#include "sys/_stdint.h"
#include "tasks.hpp"

namespace audio {

static const char* kTag = "task";

auto StartAudioTask(const std::string& name,
                    std::shared_ptr<IAudioElement> element)
    -> std::unique_ptr<AudioElementHandle> {
  auto task_handle = std::make_unique<TaskHandle_t>();

  // Newly created task will free this.
  AudioTaskArgs* args = new AudioTaskArgs{.element = element};

  ESP_LOGI(kTag, "starting audio task %s", name.c_str());
  xTaskCreate(&AudioTaskMain, name.c_str(), element->StackSizeBytes(), args,
              kTaskPriorityAudio, task_handle.get());

  return std::make_unique<AudioElementHandle>(std::move(task_handle), element);
}

void AudioTaskMain(void* args) {
  // Nest the body within an additional scope to ensure that destructors are
  // called before the task quits.
  {
    AudioTaskArgs* real_args = reinterpret_cast<AudioTaskArgs*>(args);
    std::shared_ptr<IAudioElement> element = std::move(real_args->element);
    delete real_args;

    // Queue of events that we have received on our input queue, but not yet
    // processed.
    std::deque<std::unique_ptr<StreamEvent>> pending_events;

    while (element->ElementState() != STATE_QUIT) {
      // First, we pull events from our input queue into pending_events. This
      // keeps us responsive to any events that need to be handled immediately.
      // Then we check if there's any events to flush downstream.
      // Then we pass anything requiring processing to the element.

      bool has_work_to_do =
          (!pending_events.empty() || element->HasUnflushedOutput() ||
           element->HasUnprocessedInput()) &&
          !element->IsOverBuffered();

      // If we have no new events to process and the element has nothing left to
      // do, then just delay forever waiting for a new event.
      TickType_t ticks_to_wait = has_work_to_do ? 0 : portMAX_DELAY;

      StreamEvent* event_ptr = nullptr;
      bool has_event =
          xQueueReceive(element->InputEventQueue(), &event_ptr, ticks_to_wait);

      if (has_event && event_ptr != nullptr) {
        std::unique_ptr<StreamEvent> event(event_ptr);
        if (event->tag == StreamEvent::CHUNK_NOTIFICATION) {
          element->OnChunkProcessed();
        } else {
          // This isn't an event that needs to be actioned immediately. Add it
          // to our work queue.
          pending_events.push_back(std::move(event));
        }
        // Loop again, so that we service all incoming events before doing our
        // possibly expensive processing.
        continue;
      }

      // We have no new events. Next, see if there's anything that needs to be
      // flushed.
      if (element->HasUnflushedOutput() && !element->FlushBufferedOutput()) {
        // We had things to flush, but couldn't send it all. This probably
        // implies that the downstream element is having issues servicing its
        // input queue, so hold off for a moment before retrying.
        ESP_LOGW(kTag, "failed to flush buffered output");
        vTaskDelay(pdMS_TO_TICKS(100));
        continue;
      }

      if (element->HasUnprocessedInput()) {
        auto process_res = element->Process();
        if (!process_res.has_error() || process_res.error() != OUT_OF_DATA) {
          // TODO: log!
        }
        continue;
      }

      // The element ran out of data, so now it's time to let it process more
      // input.
      while (!pending_events.empty()) {
        auto event = std::move(pending_events.front());
        pending_events.pop_front();

        if (event->tag == StreamEvent::STREAM_INFO) {
          auto process_res = element->ProcessStreamInfo(*event->stream_info);
          if (process_res.has_error()) {
            // TODO(jacqueline)
            ESP_LOGE(kTag, "failed to process stream info");
          }
        } else if (event->tag == StreamEvent::CHUNK_DATA) {
          StreamEvent* callback = new StreamEvent();
          callback->source = element->InputEventQueue();
          callback->tag = StreamEvent::CHUNK_NOTIFICATION;
          if (!xQueueSend(event->source, callback, 0)) {
            // TODO: log? crash? hmm.
            pending_events.push_front(std::move(event));
            continue;
          }

          auto process_chunk_res =
              element->ProcessChunk(event->chunk_data.bytes);
          if (process_chunk_res.has_error()) {
            // TODO(jacqueline)
            ESP_LOGE(kTag, "failed to process chunk");
            continue;
          }
        }
      }
    }
  }
  vTaskDelete(NULL);
}

}  // namespace audio
