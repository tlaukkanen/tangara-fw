#include <dirent.h>
#include <stdio.h>
#include <cstdint>

#include "battery.h"
#include "dac.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_adc_cal.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "gpio-expander.h"
#include "hal/adc_types.h"
#include "hal/gpio_types.h"
#include "hal/i2s_types.h"
#include "hal/spi_types.h"
#include "storage.h"

#include "audio_common.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"

#define I2C_SDA_IO (GPIO_NUM_2)
#define I2C_SCL_IO (GPIO_NUM_4)
#define I2C_CLOCK_HZ (400000)

#define SPI_SDI_IO (GPIO_NUM_19)
#define SPI_SDO_IO (GPIO_NUM_23)
#define SPI_SCLK_IO (GPIO_NUM_18)
#define SPI_QUADWP_IO (GPIO_NUM_22)
#define SPI_QUADHD_IO (GPIO_NUM_21)

#define I2S_SCK_IO (GPIO_NUM_25)
#define I2S_BCK_IO (GPIO_NUM_26)
#define I2S_LRCK_IO (GPIO_NUM_27)
#define I2S_DIN_IO (GPIO_NUM_5)

static const char* TAG = "MAIN";

esp_err_t init_i2c(void) {
  i2c_port_t port = I2C_NUM_0;
  i2c_config_t config = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = I2C_SDA_IO,
      .scl_io_num = I2C_SCL_IO,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master =
          {
              .clk_speed = I2C_CLOCK_HZ,
          },
      // No requirements for the clock.
      .clk_flags = 0,
  };

  ESP_ERROR_CHECK(i2c_param_config(port, &config));
  ESP_ERROR_CHECK(i2c_driver_install(port, config.mode, 0, 0, 0));

  // TODO: INT line

  return ESP_OK;
}

esp_err_t init_spi(void) {
  spi_bus_config_t config = {
      .mosi_io_num = SPI_SDO_IO,
      .miso_io_num = SPI_SDI_IO,
      .sclk_io_num = SPI_SCLK_IO,
      .quadwp_io_num = SPI_QUADWP_IO,
      .quadhd_io_num = SPI_QUADHD_IO,

      // Unused
      .data4_io_num = -1,
      .data5_io_num = -1,
      .data6_io_num = -1,
      .data7_io_num = -1,

      // Use the DMA default size.
      .max_transfer_sz = 0,
      .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_IOMUX_PINS,
      .intr_flags = 0,
  };

  ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &config, SPI_DMA_CH_AUTO));

  return ESP_OK;
}

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "Initialising peripherals");

  ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LOWMED));
  init_i2c();
  init_spi();

  ESP_LOGI(TAG, "Init GPIOs");
  gay_ipod::GpioExpander expander;

  // for debugging usb ic
  // expander.set_sd_mux(gay_ipod::GpioExpander::USB);

  ESP_LOGI(TAG, "Init ADC");
  ESP_ERROR_CHECK(gay_ipod::init_adc());

  ESP_LOGI(TAG, "Init SD card");
  gay_ipod::SdStorage storage(&expander);
  gay_ipod::SdStorage::Error err = storage.Acquire();
  if (err != gay_ipod::SdStorage::Error::OK) {
    ESP_LOGE(TAG, "Failed to acquire storage!");
    return;
  }

  ESP_LOGI(TAG, "Everything looks good! Waiting a mo for debugger.");
  vTaskDelay(pdMS_TO_TICKS(1500));

  ESP_LOGI(TAG, "Trying to init DAC");
  gay_ipod::AudioDac dac(&expander);
  dac.Start();

  vTaskDelay(pdMS_TO_TICKS(1000));

  ESP_LOGI(TAG, "Looks okay? Let's play some music!");

  i2s_port_t port = I2S_NUM_0;

  i2s_config_t i2s_config = {
      // Weird enum usage in ESP IDF.
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
  };

  // ESP_ERROR_CHECK(i2s_driver_install(port, &i2s_config, 0, NULL));

  audio_pipeline_handle_t pipeline;
  audio_element_handle_t fatfs_stream_reader, i2s_stream_writer, audio_decoder;

  audio_pipeline_cfg_t pipeline_config =
      audio_pipeline_cfg_t(DEFAULT_AUDIO_PIPELINE_CONFIG());
  pipeline = audio_pipeline_init(&pipeline_config);
  assert(pipeline != NULL);
  ESP_LOGI(TAG, "Made pipeline okay.");

  fatfs_stream_cfg_t fatfs_stream_config =
      fatfs_stream_cfg_t(FATFS_STREAM_CFG_DEFAULT());
  fatfs_stream_config.type = AUDIO_STREAM_READER;
  fatfs_stream_reader = fatfs_stream_init(&fatfs_stream_config);
  assert(fatfs_stream_reader != NULL);
  ESP_LOGI(TAG, "Made reader okay.");

  i2s_stream_cfg_t i2s_stream_config = i2s_stream_cfg_t{
      .type = AUDIO_STREAM_WRITER,
      .i2s_config = i2s_config,
      .i2s_port = port,
      .use_alc = false,
      .volume = 0,  // Does nothing; use the dac
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
  // TODO fix hardcoded mclk :(
  i2s_stream_writer = i2s_stream_init(&i2s_stream_config);
  assert(i2s_stream_writer != NULL);
  ESP_LOGI(TAG, "Made i2s stream okay.");

  ESP_LOGI(TAG, "Init i2s pins");
  i2s_pin_config_t pin_config = {.mck_io_num = GPIO_NUM_0,
                                 .bck_io_num = GPIO_NUM_26,
                                 .ws_io_num = GPIO_NUM_27,
                                 .data_out_num = GPIO_NUM_5,
                                 .data_in_num = I2S_PIN_NO_CHANGE};

  ESP_ERROR_CHECK(i2s_set_pin(port, &pin_config));

  mp3_decoder_cfg_t decoder_config =
      mp3_decoder_cfg_t(DEFAULT_MP3_DECODER_CONFIG());
  audio_decoder = mp3_decoder_init(&decoder_config);
  assert(audio_decoder != NULL);
  ESP_LOGI(TAG, "Made mp3 decoder okay.");

  vTaskDelay(pdMS_TO_TICKS(500));
  ESP_LOGI(TAG, "Putting it all together");

  audio_pipeline_register(pipeline, fatfs_stream_reader, "file");
  audio_pipeline_register(pipeline, audio_decoder, "dec");
  audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

  const char* link_tag[3] = {"file", "dec", "i2s"};
  audio_pipeline_link(pipeline, &link_tag[0], 3);

  ESP_LOGI(TAG, "Trying to play something??");
  audio_element_set_uri(fatfs_stream_reader, "/sdcard/test.mp3");
  audio_pipeline_run(pipeline);

  vTaskDelay(pdMS_TO_TICKS(1000));
  ESP_LOGI(TAG, "It could be playing? idk");

  gay_ipod::AudioDac::PowerState state = dac.ReadPowerState().second;
  ESP_LOGI(TAG, "dac power state: %x", (uint8_t)state);

  vTaskDelay(pdMS_TO_TICKS(120000));

  ESP_LOGI(TAG, "Time to deinit.");
  audio_pipeline_stop(pipeline);
  audio_pipeline_wait_for_stop(pipeline);
  audio_pipeline_terminate(pipeline);

  audio_pipeline_unregister(pipeline, fatfs_stream_reader);
  audio_pipeline_unregister(pipeline, audio_decoder);
  audio_pipeline_unregister(pipeline, i2s_stream_writer);

  audio_pipeline_deinit(pipeline);
  audio_element_deinit(fatfs_stream_reader);
  audio_element_deinit(i2s_stream_writer);
  audio_element_deinit(audio_decoder);

  storage.Release();

  ESP_LOGI(TAG, "Hooray!");
}
