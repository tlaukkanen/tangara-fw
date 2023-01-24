#include "audio_task.hpp"

#include <stdlib.h>

#include <cstdint>
#include <memory>

#include "audio_element_handle.hpp"
#include "cbor.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"
#include "span.hpp"

#include "audio_element.hpp"
#include "chunk.hpp"
#include "stream_info.hpp"
#include "stream_message.hpp"
#include "tasks.hpp"

namespace audio {

  static const char *kTag = "task";

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

    ChunkReader chunk_reader = ChunkReader(element->InputBuffer());

    while (element->ElementState() != STATE_QUIT) {
      if (element->ElementState() == STATE_PAUSE) {
        // TODO: park with a condition variable or something?
        vTaskDelay(1000);
        continue;
      }

      cpp::result<size_t, AudioProcessingError> process_res;

      // If this element has an input stream, then our top priority is
      // processing any chunks from it. Try doing this first, then fall back to
      // the other cases.
      bool has_received_message = false;
      ChunkReadResult chunk_res = chunk_reader.ReadChunkFromStream(
          [&](cpp::span<std::byte> data) -> std::optional<size_t> {
            process_res = element->ProcessChunk(data);
            if (process_res.has_value()) {
              return process_res.value();
            } else {
              return {};
            }
          },
          0);

      if (chunk_res == CHUNK_PROCESSING_ERROR ||
          chunk_res == CHUNK_DECODING_ERROR) {
        ESP_LOGE(kTag, "failed to process chunk");
        break;  // TODO.
      } else if (chunk_res == CHUNK_STREAM_ENDED) {
        has_received_message = true;
      }

      if (has_received_message) {
        auto message = chunk_reader.GetLastMessage();
        MessageType type = ReadMessageType(message);
        if (type == TYPE_STREAM_INFO) {
          auto parse_res = ReadMessage<StreamInfo>(&StreamInfo::Parse, message);
          if (parse_res.has_error()) {
            ESP_LOGE(kTag, "failed to parse stream info");
            break;  // TODO.
          }
          auto info_res = element->ProcessStreamInfo(parse_res.value());
          if (info_res.has_error()) {
            ESP_LOGE(kTag, "failed to process stream info");
            break;  // TODO.
          }
        }
      }

      // Chunk reading must have timed out, or we don't have an input stream.
      ElementState state = element->ElementState();
      if (state == STATE_PAUSE) {
        element->PrepareForPause();

        vTaskSuspend(NULL);

        // Zzzzzz...

        // When we wake up, skip straight to the start of the loop again.
        continue;
      } else if (state == STATE_QUIT) {
        break;
      }

      // Signal the element to do any of its idle tasks.
      auto process_error = element->ProcessIdle();
      if (process_error.has_error()) {
        auto err = process_error.error();
        if (err == OUT_OF_DATA) {
          // If we ran out of data, then place ourselves into the pause state.
          // We will be woken up when there's something to do.
          element->ElementState(STATE_PAUSE);
          continue;
        } else {
          ESP_LOGE(kTag, "failed to process idle");
          break;  // TODO.
        }
      }
    }
  }
  vTaskDelete(NULL);
}

}  // namespace audio
