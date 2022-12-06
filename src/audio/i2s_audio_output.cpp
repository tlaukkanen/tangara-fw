#include "i2s_audio_output.hpp"

#include <algorithm>

#include "esp_err.h"
#include "freertos/portmacro.h"

#include "audio_element.hpp"
#include "dac.hpp"
#include "gpio_expander.hpp"
#include "result.hpp"

static const TickType_t kIdleTimeBeforeMute = pdMS_TO_TICKS(1000);
static const char* kTag = "I2SOUT";

namespace audio {

auto I2SAudioOutput::create(drivers::GpioExpander* expander)
    -> cpp::result<std::unique_ptr<I2SAudioOutput>, Error> {
  // First, we need to perform initial configuration of the DAC chip.
  auto dac_result = drivers::AudioDac::create(expander);
  if (dac_result.has_error()) {
    ESP_LOGE(kTag, "failed to init dac: %d", dac_result.error());
    return cpp::fail(DAC_CONFIG);
  }
  std::unique_ptr<drivers::AudioDac> dac = std::move(dac_result.value());

  // Soft mute immediately, in order to minimise any clicks and pops caused by
  // the initial output element and pipeline configuration.
  dac->WriteVolume(255);

  return std::make_unique<I2SAudioOutput>(expander, std::move(dac));
}

I2SAudioOutput::I2SAudioOutput(drivers::GpioExpander* expander,
                               std::unique_ptr<drivers::AudioDac> dac)
    : expander_(expander),
      dac_(std::move(dac)),
      volume_(255),
      is_soft_muted_(false) {}

I2SAudioOutput::~I2SAudioOutput() {
  // TODO: power down the DAC.
}

auto I2SAudioOutput::ProcessStreamInfo(const StreamInfo& info)
    -> cpp::result<void, AudioProcessingError> {
  // TODO(jacqueline): probs do something with the channel hey

  if (!info.BitsPerSample() && !info.SampleRate()) {
    return cpp::fail(UNSUPPORTED_STREAM);
  }

  drivers::AudioDac::BitsPerSample bps;
  switch (*info.BitsPerSample()) {
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
      return cpp::fail(UNSUPPORTED_STREAM);
  }

  drivers::AudioDac::SampleRate sample_rate;
  switch (*info.SampleRate()) {
    case 44100:
      sample_rate = drivers::AudioDac::SAMPLE_RATE_44_1;
      break;
    case 48000:
      sample_rate = drivers::AudioDac::SAMPLE_RATE_48;
      break;
    default:
      return cpp::fail(UNSUPPORTED_STREAM);
  }

  dac_->Reconfigure(bps, sample_rate);

  return {};
}

auto I2SAudioOutput::ProcessChunk(const cpp::span<std::byte>& chunk)
    -> cpp::result<std::size_t, AudioProcessingError> {
  SetSoftMute(false);
  // TODO(jacqueline): write smaller parts with a small delay so that we can
  // be responsive to pause and seek commands.
  return dac_->WriteData(chunk, portMAX_DELAY);
}

auto I2SAudioOutput::IdleTimeout() const -> TickType_t {
  return kIdleTimeBeforeMute;
}

auto I2SAudioOutput::ProcessIdle() -> cpp::result<void, AudioProcessingError> {
  // TODO(jacqueline): Consider powering down the dac completely maybe?
  SetSoftMute(true);
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
