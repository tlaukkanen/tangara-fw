#include "i2s_audio_output.hpp"

#include <algorithm>

#include "audio_element.h"
#include "driver/i2s.h"
#include "esp_err.h"
#include "freertos/portmacro.h"
#include "i2s_stream.h"

static const i2s_port_t kI2SPort = I2S_NUM_0;
static const char* kTag = "I2SOUT";

namespace drivers {

auto I2SAudioOutput::create(GpioExpander* expander)
    -> cpp::result<std::unique_ptr<I2SAudioOutput>, Error> {
  // First, we need to perform initial configuration of the DAC chip.
  auto dac_result = drivers::AudioDac::create(expander);
  if (dac_result.has_error()) {
    ESP_LOGE(kTag, "failed to init dac: %d", dac_result.error());
    return cpp::fail(DAC_CONFIG);
  }
  std::unique_ptr<AudioDac> dac = std::move(dac_result.value());

  // Soft mute immediately, in order to minimise any clicks and pops caused by
  // the initial output element and pipeline configuration.
  dac->WriteVolume(255);

  i2s_stream_cfg_t i2s_stream_config = i2s_stream_cfg_t{
      .type = AUDIO_STREAM_WRITER,
      .i2s_config =
          {
              // static_cast bc esp-adf uses enums incorrectly
              .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
              .sample_rate = 44100,
              .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
              .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
              .communication_format = I2S_COMM_FORMAT_STAND_I2S,
              .intr_alloc_flags = ESP_INTR_FLAG_LOWMED,
              .dma_buf_count = 8,
              .dma_buf_len = 64,
              .use_apll = false,
              .tx_desc_auto_clear = false,
              .fixed_mclk = 0,
              .mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT,
              .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
          },
      .i2s_port = kI2SPort,
      .use_alc = false,
      .volume = 0,  // Does nothing; use AudioDac to change this.
      .out_rb_size = I2S_STREAM_RINGBUFFER_SIZE,
      .task_stack = I2S_STREAM_TASK_STACK,
      .task_core = I2S_STREAM_TASK_CORE,
      .task_prio = I2S_STREAM_TASK_PRIO,
      .stack_in_ext = false,
      .multi_out_num = 0,
      .uninstall_drv = true,
      .need_expand = false,
      .expand_src_bits = I2S_BITS_PER_SAMPLE_16BIT,
  };
  audio_element_handle_t i2s_stream_writer =
      i2s_stream_init(&i2s_stream_config);
  if (i2s_stream_writer == NULL) {
    return cpp::fail(Error::STREAM_INIT);
  }

  // NOTE: i2s_stream_init does some additional setup that hardcodes MCK as
  // GPIO0. This happens to work fine for us, but be careful if changing.
  i2s_pin_config_t pin_config = {.mck_io_num = GPIO_NUM_0,
                                 .bck_io_num = GPIO_NUM_26,
                                 .ws_io_num = GPIO_NUM_27,
                                 .data_out_num = GPIO_NUM_5,
                                 .data_in_num = I2S_PIN_NO_CHANGE};
  if (esp_err_t err = i2s_set_pin(kI2SPort, &pin_config) != ESP_OK) {
    ESP_LOGE(kTag, "failed to configure i2s pins %x", err);
    return cpp::fail(Error::I2S_CONFIG);
  }

  return std::make_unique<I2SAudioOutput>(dac, i2s_stream_writer);
}

I2SAudioOutput::I2SAudioOutput(std::unique_ptr<AudioDac>& dac,
                               audio_element_handle_t element)
    : IAudioOutput(element), dac_(std::move(dac)) {
  volume_ = 255;
}
I2SAudioOutput::~I2SAudioOutput() {
  // TODO: power down the DAC.
}

auto I2SAudioOutput::SetVolume(uint8_t volume) -> void {
  volume_ = volume;
  if (!is_soft_muted_) {
    dac_->WriteVolume(volume);
  }
}

auto I2SAudioOutput::SetSoftMute(bool enabled) -> void {
  if (enabled) {
    is_soft_muted_ = true;
    dac_->WriteVolume(255);
  } else {
    is_soft_muted_ = false;
    dac_->WriteVolume(volume_);
  }
}

auto I2SAudioOutput::Configure(audio_element_info_t& info) -> void {
  audio_element_setinfo(element_, &info);
  i2s_stream_set_clk(element_, info.sample_rates, info.bits, info.channels);
}

}  // namespace drivers
