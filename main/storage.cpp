#include "storage.h"

#include "diskio_impl.h"
#include "diskio_sdmmc.h"
#include "driver/sdspi_host.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "ff.h"
#include "gpio-expander.h"
#include "hal/gpio_types.h"
#include "hal/spi_types.h"
#include "sdmmc_cmd.h"

namespace gay_ipod {

static const char* TAG = "SDSTORAGE";

SdStorage::SdStorage(GpioExpander *gpio) {
  this->gpio_ = gpio;
}

SdStorage::~SdStorage() {}

SdStorage::Error SdStorage::Acquire(void) {
  // First switch to this device, and pull CS.
  gpio_->set_sd_mux(GpioExpander::ESP);
  gpio_->set_sd_cs(false);
  gpio_->Write();

  // Now we can init the driver and set up the SD card into SPI mode.
  sdspi_host_init();

  sdspi_device_config_t config = {
    .host_id = VSPI_HOST,
    // CS handled manually bc it's on the GPIO expander
    .gpio_cs = GPIO_NUM_2,
    .gpio_cd = SDSPI_SLOT_NO_CD,
    .gpio_wp = SDSPI_SLOT_NO_WP,
    .gpio_int = GPIO_NUM_NC,
  };
  ESP_ERROR_CHECK(sdspi_host_init_device(&config, &handle_));

  host_ = sdmmc_host_t SDSPI_HOST_DEFAULT();
  host_.slot = handle_;
  // Will return ESP_ERR_INVALID_RESPONSE if there is no card
  esp_err_t err = sdmmc_card_init(&host_, &card_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to read, err: %d", err);
    gpio_->set_sd_cs(true);
    gpio_->Write();
    return Error::FAILED_TO_READ;
  }

  ESP_ERROR_CHECK(esp_vfs_fat_register(kStoragePath, "", kMaxOpenFiles, &fs_));
  ff_diskio_register_sdmmc(fs_->pdrv, &card_);

  // Mount right now, not on first operation.
  FRESULT ferr = f_mount(fs_, "", 1);
  if (ferr != FR_OK) {
    ESP_LOGW(TAG, "Failed to mount, err: %d", ferr);
    return Error::FAILED_TO_MOUNT;
  }

  // We're done chatting for now.
  //gpio_->set_sd_cs(true);
  //gpio_->Write();

  return Error::OK;
}

void SdStorage::Release(void) {
  gpio_->set_sd_cs(false);
  gpio_->Write();

  // Unmount and unregister the filesystem
  f_unmount("");
  ff_diskio_register(fs_->pdrv, NULL);
  esp_vfs_fat_unregister_path(kStoragePath);
  fs_ = nullptr;

  // Uninstall the SPI driver
  sdspi_host_remove_device(this->handle_);
  sdspi_host_deinit();

  gpio_->set_sd_mux(GpioExpander::USB);
  gpio_->set_sd_cs(true);
  gpio_->Write();
}

} // namespace gay_ipod
