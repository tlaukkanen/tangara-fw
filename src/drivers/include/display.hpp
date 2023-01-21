#pragma once

#include <cstdint>

#include "driver/spi_master.h"
#include "lvgl/lvgl.h"
#include "result.hpp"

#include "display_init.hpp"
#include "gpio_expander.hpp"
#include "sys/_stdint.h"

namespace drivers {

/*
 * Display driver for LVGL.
 */
class Display {
 public:
  enum Error {};
  static auto create(GpioExpander* expander,
                     const displays::InitialisationData& init_data)
      -> cpp::result<std::unique_ptr<Display>, Error>;

  Display(GpioExpander* gpio, spi_device_handle_t handle);
  ~Display();

  void WriteData();

  void Flush(lv_disp_drv_t* disp_drv,
             const lv_area_t* area,
             lv_color_t* color_map);

  void IRAM_ATTR PostTransaction(const spi_transaction_t& transaction);

  void ServiceTransactions();

 private:
  GpioExpander* gpio_;
  spi_device_handle_t handle_;

  lv_disp_draw_buf_t buffers_;
  lv_disp_drv_t driver_;
  lv_disp_t* display_ = nullptr;

  enum TransactionType {
    COMMAND = 0,
    DATA = 1,
  };

  enum TransactionFlags {
    LVGL_FLUSH = 1,
  };

  void SendInitialisationSequence(const uint8_t* data);

  void SendCommandWithData(uint8_t command,
                           const uint8_t* data,
                           size_t length,
                           uintptr_t flags = 0);

  void SendCmd(const uint8_t* data, size_t length, uintptr_t flags = 0);
  void SendData(const uint8_t* data, size_t length, uintptr_t flags = 0);
  void SendTransaction(TransactionType type,
                       const uint8_t* data,
                       size_t length,
                       uint32_t flags = 0);
};

}  // namespace drivers
