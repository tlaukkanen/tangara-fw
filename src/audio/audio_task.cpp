#include "audio_task.hpp"

#include <stdlib.h>

#include <cstdint>

#include "esp-idf/components/cbor/tinycbor/src/cbor.h"
#include "esp_heap_caps.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "audio_element.hpp"
#include "include/audio_element.hpp"
#include "stream_message.hpp"

namespace audio {

static const TickType_t kCommandWaitTicks = 1;
static const TickType_t kIdleTaskDelay = 1;

void audio_task(void* args) {
  AudioTaskArgs* real_args = reinterpret_cast<AudioTaskArgs*>(args);
  std::shared_ptr<IAudioElement> element = real_args->element;
  delete real_args;

  MessageBufferHandle_t *stream = element->InputBuffer();

  uint8_t* message_buffer =
      (uint8_t*)heap_caps_malloc(kFrameSize, MALLOC_CAP_SPIRAM);

  while (1) {
    BaseType_t rtos_res;
    IAudioElement::ProcessResult result;


    size_t message_size = 0;
    if (message_buffer != nullptr) {
      // TODO: tune delay.
      message_size = xMessageBufferReceive(stream, &message_buffer, kFrameSize, portMAX_DELAY);
    }

    if (message_size == 0) {
      element->ProcessIdle();
      continue;
    }

    // We got a valid message. Check what kind it is so that we know how to
    // process it.
    CborParser parser;
    CborValue value;
    cbor_parser_init(message_buffer, message_size, &parser, &value);

    MessageType message_type;
    if (!cbor_value_is_integer(&value) || !cbor_value_get_integer(&value, &message_type)) {
      // We weren't able to parse the message type. This is really bad, so just
      // abort.
      break; // TODO.
    }

    if (message_type == STREAM_INFO) {
      errs = StreamInfo::Create(message_buffer, message_size).map(element->ProcessStreamInfo);
      if (errs.has_error) {
        // TODO;
      }
    } else if (message_type == CHUNK_HEADER) {
    } else {
      // TODO.
    }

    cbor_value_
    if (!xQueueReceive(commands, &command, wait_time)) {
      if (bytes_in_stream > 0) {
        size_t read_length = std::min(kMaxFrameSize - leftover_data, bytes_in_stream);
        xStreamBufferReceive(stream, &frame_buffer + leftover_data, read_length, 0);

        uint8_t *data_in = frame_buffer;
        result = element->ProcessData(&data_in, read_length);
        if (result == IAudioElement::ERROR) {
          break;
        }

        if (result == IAudioElement::LEFTOVER_DATA) {
          leftover_data = frame_buffer + read_length - data_in;
          memmove(frame_buffer, data_in, leftover_data);
        } else {
          leftover_data = 0;
        }
      } else {
        result = element->ProcessIdle();
        if (result == IAudioElement::ERROR) {
          break;
        }
        if (result == IAudioElement::OUTPUT_FULL) {
          vTaskDelay(kIdleTaskDelay);
        }
      }
    } else {
      if (command.type == IAudioElement::SEQUENCE_NUMBER) {
        if (command.sequence_number > current_sequence_number) {
          current_sequence_number = command.sequence_number;
          bytes_in_stream = 0;
        }
      } else if (command.type == IAudioElement::READ) {
        assert(command.read_size <= kFrameSize);
        assert(stream != NULL);

        if (command.sequence_number == current_sequence_number) {
          bytes_in_stream += command.read_size;
        } else {
          // This data is for a different stream, so just discard it.
          xStreamBufferReceive(stream, &frame_buffer, command.read_size, 0);
        }
      } else if (command.type == IAudioElement::ELEMENT) {
        assert(command.data != NULL);
        if (command.sequence_number == current_sequence_number) {
          if (bytes_in_stream > 0) {
            // We're not ready to handle this yet, so put it back.
            xQueueSendToFront(commands, &command, kMaxWaitTicks);
          } else {
            result = element->ProcessElementCommand(command.data);
            if (result == IAudioElement::ERROR) {
              break;
            }
            if (result == IAudioElement::OUTPUT_FULL) {
              // TODO: what does this mean lol
            }
          }
        } else {
          element->SkipElementCommand(command.data);
        }
      } else if (command.type == IAudioElement::QUIT) {
        break;
      }
    }
  }

  element = nullptr;
  free(frame_buffer);

  xTaskDelete(NULL);
}

}  // namespace audio
