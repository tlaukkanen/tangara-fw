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
      latest_chunk_() {}

I2SAudioOutput::~I2SAudioOutput() {}

auto I2SAudioOutput::HasUnprocessedInput() -> bool {
  return latest_chunk_.size() > 0;
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

  dac_->Reconfigure(bps, sample_rate);

  return {};
}

auto I2SAudioOutput::ProcessChunk(const cpp::span<std::byte>& chunk)
    -> cpp::result<std::size_t, AudioProcessingError> {
  latest_chunk_ = chunk_reader_->HandleNewData(chunk);
  return 0;
}

auto I2SAudioOutput::ProcessEndOfStream() -> void {
  dac_->Stop();
  SendOrBufferEvent(std::unique_ptr<StreamEvent>(
      StreamEvent::CreateEndOfStream(input_events_)));
}

auto I2SAudioOutput::ProcessLogStatus() -> void {
  dac_->LogStatus();
}

auto I2SAudioOutput::Process() -> cpp::result<void, AudioProcessingError> {
  // Note: no logging here!
  std::size_t bytes_written = dac_->WriteData(latest_chunk_);
  if (bytes_written == latest_chunk_.size_bytes()) {
    latest_chunk_ = cpp::span<std::byte>();
    chunk_reader_->HandleBytesLeftOver(0);
  } else {
    latest_chunk_ = latest_chunk_.subspan(bytes_written);
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

}  // namespace audio
