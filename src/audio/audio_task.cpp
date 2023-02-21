#include "audio_task.hpp"

#include <stdlib.h>

#include <cstdint>
#include <deque>
#include <memory>

#include "arena.hpp"
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
                    std::optional<BaseType_t> core_id,
                    std::shared_ptr<IAudioElement> element)
    -> std::unique_ptr<AudioElementHandle> {
  auto task_handle = std::make_unique<TaskHandle_t>();

  // Newly created task will free this.
  AudioTaskArgs* args = new AudioTaskArgs{.element = element};

  ESP_LOGI(kTag, "starting audio task %s", name.c_str());
  if (core_id) {
    xTaskCreatePinnedToCore(&AudioTaskMain, name.c_str(),
                            element->StackSizeBytes(), args, kTaskPriorityAudio,
                            task_handle.get(), *core_id);
  } else {
    xTaskCreate(&AudioTaskMain, name.c_str(), element->StackSizeBytes(), args,
                kTaskPriorityAudio, task_handle.get());
  }

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

      if (has_work_to_do) {
        ESP_LOGD(kTag, "checking for events");
      } else {
        ESP_LOGD(kTag, "waiting for events");
      }

      // If we have no new events to process and the element has nothing left to
      // do, then just delay forever waiting for a new event.
      TickType_t ticks_to_wait = has_work_to_do ? 0 : portMAX_DELAY;

      StreamEvent* new_event = nullptr;
      bool has_event =
          xQueueReceive(element->InputEventQueue(), &new_event, ticks_to_wait);

      if (has_event) {
        if (new_event->tag == StreamEvent::UNINITIALISED) {
          ESP_LOGE(kTag, "discarding invalid event!!");
        } else if (new_event->tag == StreamEvent::CHUNK_NOTIFICATION) {
          ESP_LOGD(kTag, "marking chunk as used");
          element->OnChunkProcessed();
          delete new_event;
        } else if (new_event->tag == StreamEvent::LOG_STATUS) {
          element->ProcessLogStatus();
          if (element->OutputEventQueue() != nullptr) {
            xQueueSendToFront(element->OutputEventQueue(), &new_event, 0);
          } else {
            delete new_event;
          }
        } else {
          // This isn't an event that needs to be actioned immediately. Add it
          // to our work queue.
          pending_events.emplace_back(new_event);
          ESP_LOGD(kTag, "deferring event");
        }
        // Loop again, so that we service all incoming events before doing our
        // possibly expensive processing.
        continue;
      }

      if (element->HasUnflushedOutput()) {
        ESP_LOGD(kTag, "flushing output");
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
        ESP_LOGD(kTag, "processing input events");
        auto process_res = element->Process();
        if (!process_res.has_error() || process_res.error() != OUT_OF_DATA) {
          // TODO: log!
        }
        continue;
      }

      // The element ran out of data, so now it's time to let it process more
      // input.
      while (!pending_events.empty()) {
        std::unique_ptr<StreamEvent> event;
        pending_events.front().swap(event);
        pending_events.pop_front();
        ESP_LOGD(kTag, "processing event, tag %i", event->tag);

        if (event->tag == StreamEvent::STREAM_INFO) {
          ESP_LOGD(kTag, "processing stream info");
          auto process_res = element->ProcessStreamInfo(*event->stream_info);
          if (process_res.has_error()) {
            // TODO(jacqueline)
            ESP_LOGE(kTag, "failed to process stream info");
          }
        } else if (event->tag == StreamEvent::ARENA_CHUNK) {
          ESP_LOGD(kTag, "processing arena data");
          memory::ArenaRef ref(event->arena_chunk);
          auto callback =
              StreamEvent::CreateChunkNotification(element->InputEventQueue());
          if (!xQueueSend(event->source, &callback, 0)) {
            ESP_LOGW(kTag, "failed to send chunk notif");
            continue;
          }

          // TODO(jacqueline): Consider giving the element a full ArenaRef here,
          // so that it can hang on to it and potentially save an alloc+copy.
          auto process_chunk_res =
              element->ProcessChunk({ref.ptr.start, ref.ptr.used_size});
          if (process_chunk_res.has_error()) {
            // TODO(jacqueline)
            ESP_LOGE(kTag, "failed to process chunk");
            continue;
          }

          // TODO: think about whether to do the whole queue
          break;
        }
      }
    }
  }
  vTaskDelete(NULL);
}

}  // namespace audio
