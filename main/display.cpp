#include "display.hpp"
#include <cstdint>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "hal/gpio_types.h"
#include "hal/spi_types.h"

namespace gay_ipod {

static const gpio_num_t kCommandOrDataPin = GPIO_NUM_21;
static const gpio_num_t kLedPin = GPIO_NUM_22;

static const uint8_t kDisplayWidth = 128;
static const uint8_t kDisplayHeight = 160;

auto Display::create(GpioExpander* expander)
    -> cpp::result<std::unique_ptr<Display>, Error> {
  // First, set up our GPIOs
#define SPI_QUADWP_IO (GPIO_NUM_22)
#define SPI_QUADHD_IO (GPIO_NUM_21)
  gpio_config_t gpio_cfg = {
    .intr_type = GPIO_INTR_DISABLE,
    .mode = GPIO_MODE_OUTPUT,
    .pin_bit_mask = (1 << GPIO_OUTPUT_22) | (1 << GPIO_OUTPUT_21),
    .pull_down_en = 0,
    .pull_up_en = 0,
  }
  gpio_config(&gpio_cfg);

  gpio_set_level(kLedPin, 1);
  gpio_set_level(kCommandOrDataPin, 1);

  // Next, init the SPI device
  auto lock = expander->AcquireSpiBus(GpioExpander::DISPLAY);
  spi_device_interface_config_t spi_cfg = {
    .command_bits = 0, // Unused
    .address_bits = 0, // Unused
    .dummy_bits = 0,
    .mode = 0,
    .duty_cycle_pos = 0, // Unused
    .cs_ena_pretrans = 0,
    .cs_ena_posttrans = 0,
    .clock_speed_hz = 32000000,
    .input_delay_ns = 0,
    .spics_io_num = -1,
    .flags = 0,
    .queue_size = 0,
    .pre_cb = NULL,
    .post_cb = NULL,
  };
  spi_device_handle_t handle;
  spi_bus_add_device(VSPI_HOST, &spi_cfg, &handle);

  // Now we reset the display into a known state, then configure it
  // https://github.com/adafruit/Adafruit-ST7735-Library/blob/master/Adafruit_ST77xx.cpp
  // https://github.com/adafruit/Adafruit-ST7735-Library/blob/master/Adafruit_ST7735.cpp
  // commonInit with Rcmd1
  // displayInit with Rcmd2green
  // displayInit with Rcmd3
  // setRotation
}

} // namespace gay_ipod
