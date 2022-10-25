#pragma once

#include <cstdint>
#include "display-init.hpp"
#include "driver/spi_master.h"
#include "gpio-expander.hpp"
#include "lvgl/lvgl.h"
#include "result.hpp"

namespace gay_ipod {

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

  enum TransactionData {
    SEND_COMMAND = (1 << 0),
    SEND_DATA = (1 << 1),
    FLUSH_BUFFER = (1 << 2),
    SMALL = (1 << 3),
  };

  void SendInitialisationSequence(const uint8_t* data);

  void SendCommandWithData(uint8_t command,
                           const uint8_t* data,
                           size_t length,
                           uintptr_t flags = 0);

  void SendCmd(const uint8_t* data, size_t length, uintptr_t flags = 0);
  void SendData(const uint8_t* data, size_t length, uintptr_t flags = 0);
  void SendTransaction(const uint8_t* data, size_t length, uintptr_t flags = 0);
};

}  // namespace gay_ipod
