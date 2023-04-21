#include <dirent.h>
#include <stdint.h>
#include <stdio.h>

#include <cstddef>
#include <cstdint>
#include <memory>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "driver_cache.hpp"
#include "esp_freertos_hooks.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "hal/spi_types.h"

#include "app_console.hpp"
#include "audio_playback.hpp"
#include "battery.hpp"
#include "dac.hpp"
#include "display.hpp"
#include "display_init.hpp"
#include "gpio_expander.hpp"
#include "i2c.hpp"
#include "lvgl_task.hpp"
#include "spi.hpp"
#include "storage.hpp"
#include "touchwheel.hpp"

static const char* TAG = "MAIN";

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "Initialising peripherals");

  ESP_ERROR_CHECK(drivers::init_i2c());
  ESP_ERROR_CHECK(drivers::init_spi());
  std::unique_ptr<drivers::DriverCache> drivers =
      std::make_unique<drivers::DriverCache>();

  ESP_LOGI(TAG, "Init GPIOs");
  drivers::GpioExpander* expander = drivers->AcquireGpios();

  ESP_LOGI(TAG, "Enable power rails for development");
  expander->with(
      [&](auto& gpio) { gpio.set_pin(drivers::GpioExpander::AMP_EN, 1); });

  ESP_LOGI(TAG, "Init battery measurement");
  drivers::Battery* battery = new drivers::Battery();
  ESP_LOGI(TAG, "it's reading %d mV!", (int)battery->Millivolts());

  ESP_LOGI(TAG, "Init SD card");
  auto storage = drivers->AcquireStorage();
  if (!storage) {
    ESP_LOGE(TAG, "Failed! Do you have an SD card?");
  }

  ESP_LOGI(TAG, "Init touch wheel");
  std::shared_ptr<drivers::TouchWheel> touchwheel =
      drivers->AcquireTouchWheel();

  std::atomic<bool> lvgl_quit;
  TaskHandle_t lvgl_task_handle;
  ui::StartLvgl(drivers.get(), &lvgl_quit, &lvgl_task_handle);

  ESP_LOGI(TAG, "Init audio pipeline");
  std::unique_ptr<audio::AudioPlayback> playback =
      std::make_unique<audio::AudioPlayback>(drivers.get());

  ESP_LOGI(TAG, "Waiting for background tasks before launching console...");
  vTaskDelay(pdMS_TO_TICKS(1000));

  ESP_LOGI(TAG, "Launch console");
  console::AppConsole console(playback.get());
  console.Launch();

  uint8_t prev_position = 0;
  while (1) {
    touchwheel->Update();
    auto wheel_data = touchwheel->GetTouchWheelData();
    if (wheel_data.wheel_position != prev_position) {
      prev_position = wheel_data.wheel_position;
      ESP_LOGI(TAG, "Touch wheel pos: %u", prev_position);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
