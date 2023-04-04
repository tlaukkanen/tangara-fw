#include <dirent.h>
#include <stdint.h>
#include <stdio.h>

#include <cstddef>
#include <cstdint>
#include <memory>

#include "core/lv_disp.h"
#include "core/lv_obj_pos.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_freertos_hooks.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "font/lv_font.h"
#include "freertos/portmacro.h"
#include "hal/gpio_types.h"
#include "hal/spi_types.h"
#include "lvgl/lvgl.h"
#include "misc/lv_color.h"
#include "misc/lv_style.h"
#include "misc/lv_timer.h"
#include "widgets/lv_label.h"

#include "app_console.hpp"
#include "audio_playback.hpp"
#include "battery.hpp"
#include "dac.hpp"
#include "display.hpp"
#include "display_init.hpp"
#include "gpio_expander.hpp"
#include "i2c.hpp"
#include "spi.hpp"
#include "storage.hpp"
#include "touchwheel.hpp"

static const char* TAG = "MAIN";

void IRAM_ATTR tick_hook(void) {
  lv_tick_inc(1);
}

static const size_t kLvglStackSize = 8 * 1024;
static StaticTask_t sLvglTaskBuffer = {};
static StackType_t sLvglStack[kLvglStackSize] = {0};

struct LvglArgs {
  drivers::GpioExpander* gpio_expander;
};

extern "C" void lvgl_main(void* voidArgs) {
  ESP_LOGI(TAG, "starting LVGL task");
  LvglArgs* args = (LvglArgs*)voidArgs;
  drivers::GpioExpander* gpio_expander = args->gpio_expander;

  // Dispose of the args now that we've gotten everything out of them.
  delete args;

  ESP_LOGI(TAG, "init lvgl");
  lv_init();

  // LVGL has been initialised, so we can now start reporting ticks to it.
  esp_register_freertos_tick_hook(&tick_hook);

  ESP_LOGI(TAG, "init display");
  std::unique_ptr<drivers::Display> display =
      drivers::Display::create(gpio_expander, drivers::displays::kST7735R);

  lv_style_t style;
  lv_style_init(&style);
  lv_style_set_text_color(&style, LV_COLOR_MAKE(0xFF, 0, 0));
  // TODO: find a nice bitmap font for this display size and density.
  // lv_style_set_text_font(&style, &lv_font_montserrat_24);

  auto label = lv_label_create(NULL);
  lv_label_set_text(label, "COLOURS!!");
  lv_obj_add_style(label, &style, 0);

  lv_obj_center(label);
  lv_scr_load(label);

  while (1) {
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // TODO: break from the loop to kill this task, so that we can do our RAII
  // cleanup, unregister our tick callback and so on.
}

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "Initialising peripherals");

  ESP_ERROR_CHECK(drivers::init_i2c());
  ESP_ERROR_CHECK(drivers::init_spi());

  ESP_LOGI(TAG, "Init GPIOs");
  drivers::GpioExpander* expander = new drivers::GpioExpander();

  ESP_LOGI(TAG, "Enable power rails for development");
  expander->with([&](auto& gpio) {
    gpio.set_pin(drivers::GpioExpander::SD_MUX_EN_ACTIVE_LOW, 0);
    gpio.set_pin(drivers::GpioExpander::SD_MUX_SWITCH,
                 drivers::GpioExpander::SD_MUX_ESP);
    gpio.set_pin(drivers::GpioExpander::SD_CARD_POWER_ENABLE, 0);
  });

  ESP_LOGI(TAG, "Init battery measurement");
  drivers::Battery* battery = new drivers::Battery();
  ESP_LOGI(TAG, "it's reading %d mV!", (int)battery->Millivolts());

  ESP_LOGI(TAG, "Init SD card");
  auto storage_res = drivers::SdStorage::create(expander);
  std::shared_ptr<drivers::SdStorage> storage;
  if (storage_res.has_error()) {
    ESP_LOGE(TAG, "Failed! Do you have an SD card?");
  } else {
    storage = std::move(storage_res.value());
  }

  ESP_LOGI(TAG, "Init touch wheel");
  std::unique_ptr<drivers::TouchWheel> touchwheel =
      std::make_unique<drivers::TouchWheel>();

  LvglArgs* lvglArgs = (LvglArgs*)calloc(1, sizeof(LvglArgs));
  lvglArgs->gpio_expander = expander;
  xTaskCreateStaticPinnedToCore(&lvgl_main, "LVGL", kLvglStackSize,
                                (void*)lvglArgs, 1, sLvglStack,
                                &sLvglTaskBuffer, 1);

  std::shared_ptr<audio::AudioPlayback> playback;
  if (storage) {
    ESP_LOGI(TAG, "Init audio pipeline");
    auto playback_res = audio::AudioPlayback::create(expander, storage);
    if (playback_res.has_error()) {
      ESP_LOGE(TAG, "Failed! Playback will not work.");
    } else {
      playback = std::move(playback_res.value());
    }
  }

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
