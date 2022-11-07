#include "playback.hpp"

#include "dac.hpp"

#include <cstdint>

#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_pipeline.h"
#include "driver/i2s.h"
#include "esp_err.h"
#include "freertos/portmacro.h"
#include "mp3_decoder.h"

static const char* kTag = "PLAYBACK";
static const i2s_port_t kI2SPort = I2S_NUM_0;

namespace drivers {

static audio_element_status_t status_from_the_void(void* status) {
  uintptr_t as_pointer_int = reinterpret_cast<uintptr_t>(status);
  return static_cast<audio_element_status_t>(as_pointer_int);
}

auto DacAudioPlayback::create(AudioDac* dac)
    -> cpp::result<std::unique_ptr<DacAudioPlayback>, Error> {
  // Ensure we're soft-muted before initialising, in order to reduce protential
  // clicks and pops.
  dac->WriteVolume(255);

  audio_pipeline_handle_t pipeline;
  audio_element_handle_t fatfs_stream_reader;
  audio_element_handle_t i2s_stream_writer;
  audio_event_iface_handle_t event_interface;

  audio_pipeline_cfg_t pipeline_config =
      audio_pipeline_cfg_t(DEFAULT_AUDIO_PIPELINE_CONFIG());
  pipeline = audio_pipeline_init(&pipeline_config);
  if (pipeline == NULL) {
    return cpp::fail(Error::PIPELINE_INIT);
  }

  fatfs_stream_cfg_t fatfs_stream_config =
      fatfs_stream_cfg_t(FATFS_STREAM_CFG_DEFAULT());
  fatfs_stream_config.type = AUDIO_STREAM_READER;
  fatfs_stream_reader = fatfs_stream_init(&fatfs_stream_config);
  if (fatfs_stream_reader == NULL) {
    return cpp::fail(Error::PIPELINE_INIT);
  }

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
  i2s_stream_writer = i2s_stream_init(&i2s_stream_config);
  if (i2s_stream_writer == NULL) {
    return cpp::fail(Error::PIPELINE_INIT);
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
    return cpp::fail(Error::PIPELINE_INIT);
  }

  // TODO: Create encoders dynamically when we need them.
  audio_element_handle_t mp3_decoder;
  mp3_decoder_cfg_t mp3_config =
      mp3_decoder_cfg_t(DEFAULT_MP3_DECODER_CONFIG());
  mp3_decoder = mp3_decoder_init(&mp3_config);
  assert(mp3_decoder != NULL);

  audio_event_iface_cfg_t event_config = AUDIO_EVENT_IFACE_DEFAULT_CFG();
  event_interface = audio_event_iface_init(&event_config);

  audio_pipeline_set_listener(pipeline, event_interface);
  audio_element_msg_set_listener(fatfs_stream_reader, event_interface);
  audio_element_msg_set_listener(mp3_decoder, event_interface);
  audio_element_msg_set_listener(i2s_stream_writer, event_interface);

  // TODO: most of this is likely post-init, since it involves a decoder.
  // All the elements of our pipeline have been initialised. Now switch them
  // together.
  audio_pipeline_register(pipeline, fatfs_stream_reader, "file");
  audio_pipeline_register(pipeline, mp3_decoder, "dec");
  audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

  const char* link_tag[3] = {"file", "dec", "i2s"};
  audio_pipeline_link(pipeline, &link_tag[0], 3);

  return std::make_unique<DacAudioPlayback>(dac, pipeline, fatfs_stream_reader,
                                            i2s_stream_writer, event_interface,
                                            mp3_decoder);
}

DacAudioPlayback::DacAudioPlayback(AudioDac* dac,
                                   audio_pipeline_handle_t pipeline,
                                   audio_element_handle_t fatfs_stream_reader,
                                   audio_element_handle_t i2s_stream_writer,
                                   audio_event_iface_handle_t event_interface,
                                   audio_element_handle_t mp3_decoder)
    : dac_(dac),
      pipeline_(pipeline),
      fatfs_stream_reader_(fatfs_stream_reader),
      i2s_stream_writer_(i2s_stream_writer),
      event_interface_(event_interface),
      mp3_decoder_(mp3_decoder) {}

DacAudioPlayback::~DacAudioPlayback() {
  dac_->WriteVolume(255);

  audio_pipeline_remove_listener(pipeline_);
  audio_element_msg_remove_listener(fatfs_stream_reader_, event_interface_);
  audio_element_msg_remove_listener(mp3_decoder_, event_interface_);
  audio_element_msg_remove_listener(i2s_stream_writer_, event_interface_);

  audio_pipeline_stop(pipeline_);
  audio_pipeline_wait_for_stop(pipeline_);
  audio_pipeline_terminate(pipeline_);

  audio_pipeline_unregister(pipeline_, fatfs_stream_reader_);
  audio_pipeline_unregister(pipeline_, mp3_decoder_);
  audio_pipeline_unregister(pipeline_, i2s_stream_writer_);

  audio_event_iface_destroy(event_interface_);

  audio_pipeline_deinit(pipeline_);
  audio_element_deinit(fatfs_stream_reader_);
  audio_element_deinit(i2s_stream_writer_);
  audio_element_deinit(mp3_decoder_);
}

void DacAudioPlayback::Play(const std::string& filename) {
  dac_->WriteVolume(255);
  // TODO: handle reconfiguring the pipeline if needed.
  audio_element_set_uri(fatfs_stream_reader_, filename.c_str());
  audio_pipeline_run(pipeline_);
  dac_->WriteVolume(volume_);
}

void DacAudioPlayback::Resume() {
  // TODO.
}
void DacAudioPlayback::Pause() {
  // TODO.
}

void DacAudioPlayback::ProcessEvents() {
  while (1) {
    audio_event_iface_msg_t event;
    esp_err_t err =
        audio_event_iface_listen(event_interface_, &event, portMAX_DELAY);
    if (err != ESP_OK) {
      ESP_LOGI(kTag, "error listening for event:%x", err);
      continue;
    }
    ESP_LOGI(kTag, "received event, cmd %i", event.cmd);

    if (event.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
        event.source == (void*)mp3_decoder_ &&
        event.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
      audio_element_info_t music_info;
      audio_element_getinfo(mp3_decoder_, &music_info);
      ESP_LOGI(kTag, "sample_rate=%d, bits=%d, ch=%d", music_info.sample_rates,
               music_info.bits, music_info.channels);
      audio_element_setinfo(i2s_stream_writer_, &music_info);
      i2s_stream_set_clk(i2s_stream_writer_, music_info.sample_rates,
                         music_info.bits, music_info.channels);
    }

    if (event.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
        event.source == (void*)fatfs_stream_reader_ &&
        event.cmd == AEL_MSG_CMD_REPORT_STATUS) {
      audio_element_status_t status = status_from_the_void(event.data);
      if (status == AEL_STATUS_STATE_FINISHED) {
        // TODO: enqueue next track?
      }
    }

    if (event.source_type == AUDIO_ELEMENT_TYPE_ELEMENT &&
        event.source == (void*)i2s_stream_writer_ &&
        event.cmd == AEL_MSG_CMD_REPORT_STATUS) {
      audio_element_status_t status = status_from_the_void(event.data);
      if (status == AEL_STATUS_STATE_FINISHED) {
        // TODO.
        return;
      }
    }

    if (event.need_free_data) {
      ESP_LOGI(kTag, "freeing event data");
      free(event.data);
    }
  }
}

/* for gapless */
void DacAudioPlayback::set_next_file(const std::string& filename) {
  next_filename_ = filename;
}

void DacAudioPlayback::set_volume(uint8_t volume) {
  volume_ = volume;
  // TODO: don't write immediately if we're muting to change track or similar.
  dac_->WriteVolume(volume);
}

auto DacAudioPlayback::volume() -> uint8_t {
  return volume_;
}

}  // namespace drivers
