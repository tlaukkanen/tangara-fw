#include "display.hpp"
#include <cstdint>
#include <cstring>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/portable.h"
#include "hal/gpio_types.h"
#include "hal/spi_types.h"

namespace gay_ipod {

static const gpio_num_t kCommandOrDataPin = GPIO_NUM_21;
static const gpio_num_t kLedPin = GPIO_NUM_22;

static const uint8_t kDisplayWidth = 128;
static const uint8_t kDisplayHeight = 160;
static const uint8_t kDelayBit = 0x80;

enum StCommands {
  ST77XX_NOP = 0x00,
  ST77XX_SWRESET = 0x01,
  ST77XX_RDDID = 0x04,
  ST77XX_RDDST = 0x09,

  ST77XX_SLPIN = 0x10,
  ST77XX_SLPOUT = 0x11,
  ST77XX_PTLON = 0x12,
  ST77XX_NORON = 0x13,

  ST77XX_INVOFF = 0x20,
  ST77XX_INVON = 0x21,
  ST77XX_DISPOFF = 0x28,
  ST77XX_DISPON = 0x29,
  ST77XX_CASET = 0x2A,
  ST77XX_RASET = 0x2B,
  ST77XX_RAMWR = 0x2C,
  ST77XX_RAMRD = 0x2E,

  ST77XX_PTLAR = 0x30,
  ST77XX_TEOFF = 0x34,
  ST77XX_TEON = 0x35,
  ST77XX_MADCTL = 0x36,
  ST77XX_COLMOD = 0x3A,

  ST77XX_MADCTL_MY = 0x80,
  ST77XX_MADCTL_MX = 0x40,
  ST77XX_MADCTL_MV = 0x20,
  ST77XX_MADCTL_ML = 0x10,
  ST77XX_MADCTL_RGB = 0x00,

  ST77XX_RDID1 = 0xDA,
  ST77XX_RDID2 = 0xDB,
  ST77XX_RDID3 = 0xDC,
  ST77XX_RDID4 = 0xDD,

  ST7735_MADCTL_BGR = 0x08,
  ST7735_MADCTL_MH = 0x04,

  ST7735_FRMCTR1 = 0xB1,
  ST7735_FRMCTR2 = 0xB2,
  ST7735_FRMCTR3 = 0xB3,
  ST7735_INVCTR = 0xB4,
  ST7735_DISSET5 = 0xB6,

  ST7735_PWCTR1 = 0xC0,
  ST7735_PWCTR2 = 0xC1,
  ST7735_PWCTR3 = 0xC2,
  ST7735_PWCTR4 = 0xC3,
  ST7735_PWCTR5 = 0xC4,
  ST7735_VMCTR1 = 0xC5,

  ST7735_PWCTR6 = 0xFC,

  ST7735_GMCTRP1 = 0xE0,
  ST7735_GMCTRN1 = 0xE1,
};

// Based on Adafruit library, which seems to be the most complete.
// clang-format off
static uint8_t kST7735RCommonHeader[]{
    15,                          // 15 commands in list:
    ST77XX_SWRESET,   kDelayBit, //  1: Software reset, 0 args, w/delay
      150,                       //     150 ms delay
    ST77XX_SLPOUT,    kDelayBit, //  2: Out of sleep mode, 0 args, w/delay
      255,                       //     500 ms delay
    ST7735_FRMCTR1, 3,           //  3: Framerate ctrl - normal mode, 3 arg:
      0x01, 0x2C, 0x2D,          //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    ST7735_FRMCTR2, 3,           //  4: Framerate ctrl - idle mode, 3 args:
      0x01, 0x2C, 0x2D,          //     Rate = fosc/(1x2+40) * (LINE+2C+2D)
    ST7735_FRMCTR3, 6,           //  5: Framerate - partial mode, 6 args:
      0x01, 0x2C, 0x2D,          //     Dot inversion mode
      0x01, 0x2C, 0x2D,          //     Line inversion mode
    ST7735_INVCTR,  1,           //  6: Display inversion ctrl, 1 arg:
      0x07,                      //     No inversion
    ST7735_PWCTR1,  3,           //  7: Power control, 3 args, no delay:
      0xA2,
      0x02,                      //     -4.6V
      0x84,                      //     AUTO mode
    ST7735_PWCTR2,  1,           //  8: Power control, 1 arg, no delay:
      0xC5,                      //     VGH25=2.4C VGSEL=-10 VGH=3 * AVDD
    ST7735_PWCTR3,  2,           //  9: Power control, 2 args, no delay:
      0x0A,                      //     Opamp current small
      0x00,                      //     Boost frequency
    ST7735_PWCTR4,  2,           // 10: Power control, 2 args, no delay:
      0x8A,                      //     BCLK/2,
      0x2A,                      //     opamp current small & medium low
    ST7735_PWCTR5,  2,           // 11: Power control, 2 args, no delay:
      0x8A, 0xEE,
    ST7735_VMCTR1,  1,           // 12: Power control, 1 arg, no delay:
      0x0E,
    ST77XX_INVOFF,  0,           // 13: Don't invert display, no args
    ST77XX_MADCTL,  1,           // 14: Mem access ctl (directions), 1 arg:
      0xC8,                      //     row/col addr, bottom-top refresh
    ST77XX_COLMOD,  1,           // 15: set color mode, 1 arg, no delay:
      0x05
};

static uint8_t kST7735RCommonGreen[]{
    2,                           //  2 commands in list:
    ST77XX_CASET,   4,           //  1: Column addr set, 4 args, no delay:
      0x00, 0x02,                //     XSTART = 0
      0x00, 0x7F+0x02,           //     XEND = 127
    ST77XX_RASET,   4,           //  2: Row addr set, 4 args, no delay:
      0x00, 0x01,                //     XSTART = 0
      0x00, 0x9F+0x01};

static uint8_t kST7735RCommonFooter[]{
    4,                           //  4 commands in list:
    ST7735_GMCTRP1, 16      ,    //  1: Gamma Adjustments (pos. polarity), 16 args + delay:
      0x02, 0x1c, 0x07, 0x12,    //     (Not entirely necessary, but provides
      0x37, 0x32, 0x29, 0x2d,    //      accurate colors)
      0x29, 0x25, 0x2B, 0x39,
      0x00, 0x01, 0x03, 0x10,
    ST7735_GMCTRN1, 16      ,    //  2: Gamma Adjustments (neg. polarity), 16 args + delay:
      0x03, 0x1d, 0x07, 0x06,    //     (Not entirely necessary, but provides
      0x2E, 0x2C, 0x29, 0x2D,    //      accurate colors)
      0x2E, 0x2E, 0x37, 0x3F,
      0x00, 0x00, 0x02, 0x10,
    ST77XX_NORON,     kDelayBit, //  3: Normal display on, no args, w/delay
      10,                           //     10 ms delay
    ST77XX_DISPON,    kDelayBit, //  4: Main screen turn on, no args w/delay
      100
};
// clang-format on

InitialisationData kInitData = {
    .num_sequences = 3,
    .sequences = {kST7735RCommonHeader, kST7735RCommonGreen,
                  kST7735RCommonFooter}};

auto Display::create(GpioExpander* expander,
                     const InitialisationData& init_data)
    -> cpp::result<std::unique_ptr<Display>, Error> {
  // First, set up our GPIOs
  gpio_config_t gpio_cfg = {
      .pin_bit_mask = GPIO_SEL_22 | GPIO_SEL_21,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&gpio_cfg);

  gpio_set_level(kLedPin, 1);
  gpio_set_level(kCommandOrDataPin, 0);

  // Next, init the SPI device
  auto lock = expander->AcquireSpiBus(GpioExpander::DISPLAY);
  spi_device_interface_config_t spi_cfg = {
      .command_bits = 0,  // No command phase
      .address_bits = 0,  // No address phase
      .dummy_bits = 0,
      // For ST7789, mode should be 2
      .mode = 0,
      .duty_cycle_pos = 0,  // Unused
      .cs_ena_pretrans = 0,
      .cs_ena_posttrans = 0,
      .clock_speed_hz = SPI_MASTER_FREQ_40M,
      .input_delay_ns = 0,  // TODO: tune?
      .spics_io_num = -1,   // TODO: change for R2
      .flags = 0,
      .queue_size = 0,
      .pre_cb = NULL,
      .post_cb = NULL,
  };
  spi_device_handle_t handle;
  spi_bus_add_device(VSPI_HOST, &spi_cfg, &handle);

  auto display = std::make_unique<Display>(expander, handle);

  // Now we reset the display into a known state, then configure it
  // setRotation
  for (int i = 0; i < init_data.num_sequences; i++) {
    display->SendInitialisationSequence(init_data.sequences[i]);
  }

  return std::move(display);
}

Display::Display(GpioExpander* gpio, spi_device_handle_t handle)
    : gpio_(gpio), handle_(handle) {}

Display::~Display() {
  // TODO.
}

void Display::SendInitialisationSequence(uint8_t* data) {
  uint8_t commands_remaining, command, num_args;
  uint16_t sleep_duration_ms;

  // First byte of the data is the number of commands.
  commands_remaining = *data;
  while (commands_remaining > 0) {
    command = *(data++);
    num_args = *(data++);
    bool has_delay = (num_args & kDelayBit) > 0;
    num_args &= ~kDelayBit;

    SendCommandWithData(command, data, num_args);

    data += num_args;
    if (has_delay) {
      sleep_duration_ms = *(data++);
      if (sleep_duration_ms == 0xFF) {
        sleep_duration_ms = 500;
      }
      vTaskDelay(pdMS_TO_TICKS(sleep_duration_ms));
    }
  }
}

void Display::SendCommandWithData(uint8_t command,
                                  uint8_t* data,
                                  size_t length) {
  gpio_set_level(kCommandOrDataPin, 0);
  SendTransaction(&command, 1);
  gpio_set_level(kCommandOrDataPin, 1);
  SendTransaction(data, length);
}

void Display::SendCmd(uint8_t* data, size_t length) {
  gpio_set_level(kCommandOrDataPin, 0);
  SendTransaction(data, length);
}

void Display::SendData(uint8_t* data, size_t length) {
  gpio_set_level(kCommandOrDataPin, 1);
  SendTransaction(data, length);
}

void Display::SendTransaction(uint8_t* data, size_t length) {
  if (length == 0) {
    return;
  }

  // TODO: Check if we should malloc this from DMA-capable memory.
  spi_transaction_t transaction;
  transaction.rx_buffer = NULL;
  // Length is in bits, so multiply by 8.
  transaction.length = length * 8;

  // If the data to transmit is very short, then we can fit it directly
  // inside the transaction struct.
  if (length < 4) {
    transaction.flags = SPI_TRANS_USE_TXDATA;
    std::memcpy(&transaction.tx_data, data, length);
  } else {
    transaction.tx_buffer = data;
  }

  // TODO: acquire the bus first? Or in an outer scope?
  // TODO: fail gracefully
  ESP_ERROR_CHECK(spi_device_polling_transmit(handle_, &transaction));
}

}  // namespace gay_ipod
