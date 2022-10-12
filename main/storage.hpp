#pragma once

#include "gpio-expander.hpp"

#include <memory>

#include "driver/sdmmc_types.h"
#include "driver/sdspi_host.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "result.hpp"

namespace gay_ipod {

extern const char* kStoragePath;

class SdStorage {
 public:
  enum Error {
    FAILED_TO_INIT,
    /** We couldn't interact with the SD card at all. Is it missing? */
    FAILED_TO_READ,
    /** We couldn't mount the SD card. Is it formatted? */
    FAILED_TO_MOUNT,
  };

  static auto create(GpioExpander* gpio)
      -> cpp::result<std::unique_ptr<SdStorage>, Error>;

  SdStorage(GpioExpander* gpio,
            esp_err_t (*do_transaction)(sdspi_dev_handle_t, sdmmc_command_t*),
            sdspi_dev_handle_t handle_,
            std::unique_ptr<sdmmc_host_t>& host_,
            std::unique_ptr<sdmmc_card_t>& card_,
            FATFS* fs_);
  ~SdStorage();

  auto HandleTransaction(sdspi_dev_handle_t handle, sdmmc_command_t* cmdinfo)
      -> esp_err_t;

  // Not copyable or movable.
  // TODO: maybe this could be movable?
  SdStorage(const SdStorage&) = delete;
  SdStorage& operator=(const SdStorage&) = delete;

 private:
  GpioExpander* gpio_;

  esp_err_t (*do_transaction_)(sdspi_dev_handle_t, sdmmc_command_t*) = nullptr;

  // SPI and SD driver info
  sdspi_dev_handle_t handle_;
  std::unique_ptr<sdmmc_host_t> host_;
  std::unique_ptr<sdmmc_card_t> card_;

  // Filesystem info
  FATFS* fs_ = nullptr;
};

}  // namespace gay_ipod
