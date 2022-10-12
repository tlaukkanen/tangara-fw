#pragma once

#include "driver/sdmmc_types.h"
#include "driver/sdspi_host.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "gpio-expander.h"

namespace gay_ipod {

// Static functions for interrop with the ESP IDF API, which requires a function
// pointer.
namespace sdspi {
// Holds a lambda created by SdStorage.
static std::function<esp_err_t(sdspi_dev_handle_t, sdmmc_command_t*)>
    do_transaction_wrapper;

// Fits the required function pointer signature, but just delegates to the
// wrapper function. Does that make this the wrapper? Who knows.
__attribute__((unused))  // (gcc incorrectly thinks this is unused)
static esp_err_t
do_transaction(sdspi_dev_handle_t handle, sdmmc_command_t* cmdinfo) {
  return do_transaction_wrapper(handle, cmdinfo);
}
}  // namespace sdspi

static const char* kStoragePath = "/sdcard";
static const uint8_t kMaxOpenFiles = 8;

class SdStorage {
 public:
  SdStorage(GpioExpander* gpio);
  ~SdStorage();

  enum Error {
    OK,
    /** We couldn't interact with the SD card at all. Is it missing? */
    FAILED_TO_READ,
    /** We couldn't mount the SD card. Is it formatted? */
    FAILED_TO_MOUNT,
  };

  // FIXME: these methods should also handling powering the SD card up and
  // down once we have that capability.

  /**
   * Initialises the SDSPI driver and mounts the SD card for reading and
   * writing. This must be called before any interactions with the underlying
   * storage.
   */
  Error Acquire(void);

  /**
   * Unmounts the SD card and frees memory associated with the SDSPI driver.
   */
  void Release(void);

  // Not copyable or movable.
  // TODO: maybe this could be movable?
  SdStorage(const SdStorage&) = delete;
  SdStorage& operator=(const SdStorage&) = delete;

 private:
  GpioExpander* gpio_;

  // SPI and SD driver info
  sdspi_dev_handle_t handle_;
  sdmmc_host_t host_;
  sdmmc_card_t card_;

  // Filesystem info
  FATFS* fs_ = nullptr;
};

}  // namespace gay_ipod
