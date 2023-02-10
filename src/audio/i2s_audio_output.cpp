#include "i2s_audio_output.hpp"

#include <algorithm>

#include "esp_err.h"
#include "freertos/portmacro.h"

#include "audio_element.hpp"
#include "dac.hpp"
#include "freertos/projdefs.h"
#include "gpio_expander.hpp"
#include "result.hpp"

static const TickType_t kIdleTimeBeforeMute = pdMS_TO_TICKS(1000);
static const char* kTag = "I2SOUT";

namespace audio {

static const std::size_t kDmaQueueLength = 8;

auto I2SAudioOutput::create(drivers::GpioExpander* expander)
    -> cpp::result<std::shared_ptr<I2SAudioOutput>, Error> {
  // First, we need to perform initial configuration of the DAC chip.
  auto dac_result = drivers::AudioDac::create(expander);
  if (dac_result.has_error()) {
    ESP_LOGE(kTag, "failed to init dac: %d", dac_result.error());
    return cpp::fail(DAC_CONFIG);
  }
  std::unique_ptr<drivers::AudioDac> dac = std::move(dac_result.value());

  // Soft mute immediately, in order to minimise any clicks and pops caused by
  // the initial output element and pipeline configuration.
  // dac->WriteVolume(255);
  dac->WriteVolume(120);  // for testing

  return std::make_shared<I2SAudioOutput>(expander, std::move(dac));
}

I2SAudioOutput::I2SAudioOutput(drivers::GpioExpander* expander,
                               std::unique_ptr<drivers::AudioDac> dac)
    : expander_(expander),
      dac_(std::move(dac)),
      volume_(255),
      is_soft_muted_(false),
      chunk_reader_(),
      latest_chunk_(),
      dma_size_(),
      dma_queue_(nullptr) {}

I2SAudioOutput::~I2SAudioOutput() {
  if (dma_queue_ != nullptr) {
    ClearDmaQueue();
  }
  // TODO: power down the DAC.
}

auto I2SAudioOutput::HasUnprocessedInput() -> bool {
  if (dma_queue_ == nullptr || !dma_size_) {
    return false;
  }
  return latest_chunk_.size() >= *dma_size_;
}

auto I2SAudioOutput::ProcessStreamInfo(const StreamInfo& info)
    -> cpp::result<void, AudioProcessingError> {
  // TODO(jacqueline): probs do something with the channel hey

  if (!info.bits_per_sample || !info.sample_rate) {
    ESP_LOGE(kTag, "audio stream missing bits or sample rate");
    return cpp::fail(UNSUPPORTED_STREAM);
  }

  if (!info.chunk_size) {
    ESP_LOGE(kTag, "audio stream missing chunk size");
    return cpp::fail(UNSUPPORTED_STREAM);
  }
  chunk_reader_.emplace(*info.chunk_size);

  ESP_LOGI(kTag, "incoming audio stream: %u bpp @ %u Hz", *info.bits_per_sample,
           *info.sample_rate);

  drivers::AudioDac::BitsPerSample bps;
  switch (*info.bits_per_sample) {
    case 16:
      bps = drivers::AudioDac::BPS_16;
      break;
    case 24:
      bps = drivers::AudioDac::BPS_24;
      break;
    case 32:
      bps = drivers::AudioDac::BPS_32;
      break;
    default:
      ESP_LOGE(kTag, "dropping stream with unknown bps");
      return cpp::fail(UNSUPPORTED_STREAM);
  }

  drivers::AudioDac::SampleRate sample_rate;
  switch (*info.sample_rate) {
    case 44100:
      sample_rate = drivers::AudioDac::SAMPLE_RATE_44_1;
      break;
    case 48000:
      sample_rate = drivers::AudioDac::SAMPLE_RATE_48;
      break;
    default:
      ESP_LOGE(kTag, "dropping stream with unknown rate");
      return cpp::fail(UNSUPPORTED_STREAM);
  }

  QueueHandle_t new_dma_queue =
      xQueueCreate(kDmaQueueLength, sizeof(std::byte*));

  dma_size_ = dac_->Reconfigure(bps, sample_rate, new_dma_queue);

  if (dma_queue_ != nullptr) {
    ClearDmaQueue();
  }
  dma_queue_ = new_dma_queue;

  return {};
}

auto I2SAudioOutput::ProcessChunk(const cpp::span<std::byte>& chunk)
    -> cpp::result<std::size_t, AudioProcessingError> {
  ESP_LOGI(kTag, "received new samples");
  latest_chunk_ = chunk_reader_->HandleNewData(chunk);
  return 0;
}

auto I2SAudioOutput::ProcessEndOfStream() -> void {
  if (chunk_reader_ && dma_size_) {
    auto leftovers = chunk_reader_->GetLeftovers();
    if (leftovers.size() > 0 && leftovers.size() < *dma_size_) {
      std::byte* dest = static_cast<std::byte*>(malloc(*dma_size_));
      cpp::span dest_span(dest, *dma_size_);

      std::copy(leftovers.begin(), leftovers.end(), dest_span.begin());
      std::fill(dest_span.begin() + leftovers.size(), dest_span.end(), static_cast<std::byte>(0));

      xQueueSend(dma_queue_, &dest, portMAX_DELAY);
    }
  }

  SendOrBufferEvent(
      std::unique_ptr<StreamEvent>(
        StreamEvent::CreateEndOfStream(input_events_)));

  chunk_reader_.reset();
  dma_size_.reset();
}

auto I2SAudioOutput::Process() -> cpp::result<void, AudioProcessingError> {
  std::size_t spaces_available = uxQueueSpacesAvailable(dma_queue_);
  if (spaces_available == 0) {
    // TODO: think about this more. can this just be the output event queue?
    vTaskDelay(pdMS_TO_TICKS(100));
    return {};
  }

  // Fill the queue as much as possible, since we need to be able to stream
  // FAST.
  while (latest_chunk_.size() >= *dma_size_ && spaces_available > 0) {
    // TODO: small memory arena for this?
    std::byte* dest = static_cast<std::byte*>(malloc(*dma_size_));
    cpp::span dest_span(dest, *dma_size_);
    cpp::span src_span = latest_chunk_.first(*dma_size_);
    std::copy(src_span.begin(), src_span.end(), dest_span.begin());
    if (!xQueueSend(dma_queue_, &dest, 0)) {
      // TODO: calculate how often we expect this to happen.
      free(dest);
      break;
    }
    latest_chunk_ = latest_chunk_.subspan(*dma_size_);
    ESP_LOGI(kTag, "wrote dma buffer of size %u", *dma_size_);
  }
  if (latest_chunk_.size() < *dma_size_) {
    // TODO: if this is the end of the stream, then we should be sending this
    // with zero padding. hmm. i guess we need an explicit EOF event?
    chunk_reader_->HandleBytesLeftOver(latest_chunk_.size());
    ESP_LOGI(kTag, "not enough samples for dma buffer");
  }
  return {};
}

auto I2SAudioOutput::SetVolume(uint8_t volume) -> void {
  volume_ = volume;
  if (!is_soft_muted_) {
    dac_->WriteVolume(volume);
  }
}

auto I2SAudioOutput::SetSoftMute(bool enabled) -> void {
  if (enabled == is_soft_muted_) {
    return;
  }
  is_soft_muted_ = enabled;
  if (is_soft_muted_) {
    dac_->WriteVolume(255);
  } else {
    dac_->WriteVolume(volume_);
  }
}

auto I2SAudioOutput::ClearDmaQueue() -> void {
  // Ensure we don't leak any memory from events leftover in the queue.
  while (uxQueueSpacesAvailable(dma_queue_) < kDmaQueueLength) {
    std::byte* data = nullptr;
    if (xQueueReceive(input_events_, &data, 0)) {
      free(data);
    } else {
      break;
    }
  }
  vQueueDelete(dma_queue_);
}

}  // namespace audio
