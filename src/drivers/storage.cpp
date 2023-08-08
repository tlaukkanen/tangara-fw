/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "storage.hpp"

#include <atomic>
#include <memory>
#include <mutex>

#include "diskio_impl.h"
#include "diskio_sdmmc.h"
#include "driver/gpio.h"
#include "driver/sdmmc_types.h"
#include "driver/sdspi_host.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "ff.h"
#include "hal/gpio_types.h"
#include "hal/spi_types.h"
#include "sdmmc_cmd.h"

#include "gpios.hpp"

static const char* kTag = "SDSTORAGE";
static const uint8_t kMaxOpenFiles = 8;

namespace drivers {

const char* kStoragePath = "/sdcard";

auto SdStorage::Create(IGpios* gpio) -> cpp::result<SdStorage*, Error> {
  gpio->WriteSync(IGpios::Pin::kSdPowerEnable, 1);
  gpio->WriteSync(IGpios::Pin::kSdMuxSwitch, IGpios::SD_MUX_ESP);
  gpio->WriteSync(IGpios::Pin::kSdMuxDisable, 0);

  sdspi_dev_handle_t handle;
  FATFS* fs = nullptr;

  // Now we can init the driver and set up the SD card into SPI mode.
  sdspi_host_init();

  sdspi_device_config_t config = {
      .host_id = VSPI_HOST,
      .gpio_cs = GPIO_NUM_21,
      .gpio_cd = SDSPI_SLOT_NO_CD,
      .gpio_wp = SDSPI_SLOT_NO_WP,
      .gpio_int = GPIO_NUM_NC,
  };
  if (esp_err_t err = sdspi_host_init_device(&config, &handle) != ESP_OK) {
    ESP_LOGE(kTag, "Failed to init, err %d", err);
    return cpp::fail(Error::FAILED_TO_INIT);
  }

  auto host = std::make_unique<sdmmc_host_t>(sdmmc_host_t SDSPI_HOST_DEFAULT());
  auto card = std::make_unique<sdmmc_card_t>();

  host->slot = handle;

  // Will return ESP_ERR_INVALID_RESPONSE if there is no card
  esp_err_t err = sdmmc_card_init(host.get(), card.get());
  if (err != ESP_OK) {
    ESP_LOGW(kTag, "Failed to read, err: %s", esp_err_to_name(err));
    return cpp::fail(Error::FAILED_TO_READ);
  }

  ESP_ERROR_CHECK(esp_vfs_fat_register(kStoragePath, "", kMaxOpenFiles, &fs));
  ff_diskio_register_sdmmc(fs->pdrv, card.get());
  ff_sdmmc_set_disk_status_check(fs->pdrv, true);

  // Mount right now, not on first operation.
  FRESULT ferr = f_mount(fs, "", 1);
  if (ferr != FR_OK) {
    std::string err_str;
    switch (ferr) {
      case FR_DISK_ERR:
        err_str = "FR_DISK_ERR";
        break;
      case FR_NOT_READY:
        err_str = "FR_NOT_READY";
        break;
      case FR_NO_FILESYSTEM:
        err_str = "FR_NO_FILESYSTEM";
        break;
      default:
        err_str = std::to_string(ferr);
    }
    ESP_LOGW(kTag, "Failed to mount, err: %s", err_str.c_str());
    return cpp::fail(Error::FAILED_TO_MOUNT);
  }

  return new SdStorage(gpio, handle, std::move(host), std::move(card), fs);
}

SdStorage::SdStorage(IGpios* gpio,
                     sdspi_dev_handle_t handle,
                     std::unique_ptr<sdmmc_host_t> host,
                     std::unique_ptr<sdmmc_card_t> card,
                     FATFS* fs)
    : gpio_(gpio),
      handle_(handle),
      host_(std::move(host)),
      card_(std::move(card)),
      fs_(fs) {}

SdStorage::~SdStorage() {
  // Unmount and unregister the filesystem
  f_unmount("");
  ff_diskio_register(fs_->pdrv, NULL);
  esp_vfs_fat_unregister_path(kStoragePath);
  fs_ = nullptr;

  // Uninstall the SPI driver
  sdspi_host_remove_device(this->handle_);
  sdspi_host_deinit();

  gpio_->WriteSync(IGpios::Pin::kSdPowerEnable, 1);
  gpio_->WriteSync(IGpios::Pin::kSdMuxDisable, 1);
}

auto SdStorage::GetFs() -> FATFS* {
  return fs_;
}

}  // namespace drivers
