#include "audio_task.hpp"

#include <stdlib.h>

#include <cstdint>

#include "esp_heap_caps.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "freertos/stream_buffer.h"

#include "audio_element.hpp"
#include "include/audio_element.hpp"

namespace audio {

static const TickType_t kCommandWaitTicks = 1;
static const TickType_t kIdleTaskDelay = 1;

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
    ProcessResult result;

    if (!xQueueReceive(commands, &command, kCommandWaitTicks)) {
      result = element->ProcessIdle();
      if (result == IAudioElement::ERROR) {
	break;
      }
      if (result == IAudioElement::OUTPUT_FULL) {
	vTaskDelay(kIdleTaskDelay);
      }
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
        result = element->ProcessData(frame_buffer, command.read_size);
	if (result == IAudioElement::ERROR) {
	  break;
	}
	if (result == IAudioElement::OUTPUT_FULL) {
	  // TODO: Do we care about this? could just park indefinitely.
	}
      }

      continue;
    }

    if (command.type == IAudioElement::ELEMENT) {
      assert(command.data != NULL);
      if (command.sequence_number == current_sequence_number) {
        result = element->ProcessElementCommand(command.data);
	if (result == IAudioElement::ERROR) {
	  break;
	}
	if (result == IAudioElement::OUTPUT_FULL) {
	  // TODO: what does this mean lol
	}
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
