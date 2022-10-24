#pragma once

#include <cstdint>
#include "driver/spi_master.h"
#include "gpio-expander.hpp"
#include "result.hpp"

// https://github.com/adafruit/Adafruit-ST7735-Library/blob/master/Adafruit_ST77xx.cpp
// https://github.com/adafruit/Adafruit-ST7735-Library/blob/master/Adafruit_ST7735.cpp
namespace gay_ipod {

struct InitialisationData {
  uint8_t num_sequences;
  uint8_t* sequences[4];
};

extern InitialisationData kInitData;

/*
 * Display driver for LVGL.
 */
class Display {
 public:
  enum Error {};
  static auto create(GpioExpander* expander,
                     const InitialisationData& init_data)
      -> cpp::result<std::unique_ptr<Display>, Error>;

  Display(GpioExpander* gpio, spi_device_handle_t handle);
  ~Display();

  void WriteData();

 private:
  GpioExpander* gpio_;
  spi_device_handle_t handle_;

  void SendInitialisationSequence(uint8_t* data);

  void SendCommandWithData(uint8_t command, uint8_t* data, size_t length);

  void SendCmd(uint8_t* data, size_t length);
  void SendData(uint8_t* data, size_t length);
  void SendTransaction(uint8_t* data, size_t length);
};

}  // namespace gay_ipod
