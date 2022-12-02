#include "audio_task.hpp"

#include <stdlib.h>

#include <cstdint>

#include "cbor.h"
#include "esp_heap_caps.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "audio_element.hpp"
#include "chunk.hpp"
#include "stream_info.hpp"
#include "stream_message.hpp"
#include "tasks.hpp"

namespace audio {

static const TickType_t kCommandWaitTicks = 1;
static const TickType_t kIdleTaskDelay = 1;
static const size_t kChunkBufferSize = kMaxChunkSize * 1.5;

auto StartAudioTask(const std::string& name,
                    std::shared_ptr<IAudioElement>& element) -> void {
  AudioTaskArgs* args = new AudioTaskArgs{.element = element};
  xTaskCreate(&AudioTaskMain, name.c_str(), element->StackSizeBytes(), args,
              kTaskPriorityAudio, NULL);
}

void AudioTaskMain(void* args) {
  {
    AudioTaskArgs* real_args = reinterpret_cast<AudioTaskArgs*>(args);
    std::shared_ptr<IAudioElement> element = std::move(real_args->element);
    delete real_args;

    ChunkReader chunk_reader = ChunkReader(element->InputBuffer());

    while (1) {
      cpp::result<size_t, AudioProcessingError> process_res;

      // If this element has an input stream, then our top priority is
      // processing any chunks from it. Try doing this first, then fall back to
      // the other cases.
      bool has_received_message = false;
      if (element->InputBuffer() != nullptr) {
        ChunkReadResult chunk_res = chunk_reader.ReadChunkFromStream(
            [&](uint8_t* data, std::size_t length) -> std::optional<size_t> {
              process_res = element->ProcessChunk(data, length);
              if (process_res.has_value()) {
                return process_res.value();
              } else {
                return {};
              }
            },
            element->IdleTimeout());

        if (chunk_res == CHUNK_PROCESSING_ERROR ||
            chunk_res == CHUNK_DECODING_ERROR) {
          break;  // TODO.
        } else if (chunk_res == CHUNK_STREAM_ENDED) {
          has_received_message = true;
        }
      }

      if (has_received_message) {
        auto [buffer, length] = chunk_reader.GetLastMessage();
        MessageType msg = ReadMessageType(buffer, length);
        if (msg == TYPE_STREAM_INFO) {
          auto parse_res =
              ReadMessage<StreamInfo>(&StreamInfo::Parse, buffer, length);
          if (parse_res.has_error()) {
            break;  // TODO.
          }
          auto info_res = element->ProcessStreamInfo(parse_res.value());
          if (info_res.has_error()) {
            break;  // TODO.
          }
        }
      }

      // TODO: Do any out of band reading, such a a pause command, here.

      // Chunk reading must have timed out, or we don't have an input stream.
      // Signal the element to do any of its idle tasks.
      auto process_error = element->ProcessIdle();
      if (process_error.has_error()) {
        break;  // TODO.
      }
    }
  }
  vTaskDelete(NULL);
}

}  // namespace audio
