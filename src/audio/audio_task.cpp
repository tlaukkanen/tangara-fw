#include "audio_task.hpp"

#include <stdlib.h>

#include <cstdint>

#include "esp_heap_caps.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "audio_element.hpp"

namespace audio {

static const TickType_t kCommandWaitTicks = 1;

void audio_task(void* args) {
  AudioTaskArgs* real_args = reinterpret_cast<AudioTaskArgs*>(args);
  std::shared_ptr<IAudioElement> element = real_args->element;
  delete real_args;

  QueueHandle_t commands = element->InputCommandQueue();
  StreamBufferHandle_t stream = element->InputBuffer();

  // TODO: think about overflow.
  uint8_t current_sequence_number;
  uint8_t* frame_buffer =
      (uint8_t*)heap_caps_malloc(kFrameSize, MALLOC_CAP_SPIRAM);

  while (1) {
    IAudioElement::Command command;
    if (!xQueueReceive(commands, &command, kCommandWaitTicks)) {
      element->ProcessIdle();
      continue;
    };

    if (command.type == IAudioElement::SEQUENCE_NUMBER) {
      if (command.sequence_number > current_sequence_number) {
        current_sequence_number = command.sequence_number;
      }

      continue;
    }

    if (command.type == IAudioElement::READ) {
      assert(command.read_size <= kFrameSize);
      assert(stream != NULL);
      xStreamBufferReceive(stream, &frame_buffer, command.read_size, 0);

      if (command.sequence_number == current_sequence_number) {
        element->ProcessData(frame_buffer, command.read_size);
      }

      continue;
    }

    if (command.type == IAudioElement::ELEMENT) {
      assert(command.data != NULL);
      if (command.sequence_number == current_sequence_number) {
        element->ProcessElementCommand(command.data);
      } else {
        element->SkipElementCommand(command.data);
      }
    }

    if (command.type == IAudioElement::QUIT) {
      break;
    }
  }

  element = nullptr;
  free(frame_buffer);

  xTaskDelete(NULL);
}

}  // namespace audio
