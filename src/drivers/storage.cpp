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

#include "gpio_expander.hpp"

static const char* kTag = "SDSTORAGE";
static const uint8_t kMaxOpenFiles = 8;

namespace drivers {

const char* kStoragePath = "/sdcard";

// Static functions for interrop with the ESP IDF API, which requires a
// function pointer.
namespace callback {
static std::atomic<SdStorage*> instance = nullptr;
static std::atomic<esp_err_t (*)(sdspi_dev_handle_t, sdmmc_command_t*)>
    bootstrap = nullptr;

static esp_err_t do_transaction(sdspi_dev_handle_t handle,
                                sdmmc_command_t* cmdinfo) {
  auto bootstrap_fn = bootstrap.load();
  if (bootstrap_fn != nullptr) {
    return bootstrap_fn(handle, cmdinfo);
  }
  auto instance_unwrapped = instance.load();
  if (instance_unwrapped == nullptr) {
    ESP_LOGW(kTag, "uncaught sdspi transaction");
    return ESP_OK;
  }
  // TODO: what if a transaction comes in right now?
  return instance_unwrapped->HandleTransaction(handle, cmdinfo);
}
}  // namespace callback

auto SdStorage::create(GpioExpander* gpio) -> cpp::result<SdStorage*, Error> {
  gpio->set_pin(GpioExpander::SD_CARD_POWER_ENABLE, 0);
  gpio->set_pin(GpioExpander::SD_MUX_EN_ACTIVE_LOW, 0);
  gpio->set_pin(GpioExpander::SD_MUX_SWITCH, GpioExpander::SD_MUX_ESP);
  gpio->Write();

  sdspi_dev_handle_t handle;
  std::unique_ptr<sdmmc_host_t> host;
  std::unique_ptr<sdmmc_card_t> card;
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

  host = std::make_unique<sdmmc_host_t>(sdmmc_host_t SDSPI_HOST_DEFAULT());
  card = std::make_unique<sdmmc_card_t>();

  // We manage the CS pin ourselves via the GPIO expander. To do this safely in
  // a multithreaded environment, we wrap the ESP IDF do_transaction function
  // with our own that acquires the CS mutex for the duration of the SPI
  // transaction.
  auto do_transaction = host->do_transaction;
  host->do_transaction = &callback::do_transaction;
  host->slot = handle;
  callback::bootstrap = do_transaction;

  // Will return ESP_ERR_INVALID_RESPONSE if there is no card
  esp_err_t err = sdmmc_card_init(host.get(), card.get());
  if (err != ESP_OK) {
    ESP_LOGW(kTag, "Failed to read, err: %d", err);
    return cpp::fail(Error::FAILED_TO_READ);
  }

  ESP_ERROR_CHECK(esp_vfs_fat_register(kStoragePath, "", kMaxOpenFiles, &fs));
  ff_diskio_register_sdmmc(fs->pdrv, card.get());

  // Mount right now, not on first operation.
  FRESULT ferr = f_mount(fs, "", 1);
  if (ferr != FR_OK) {
    ESP_LOGW(kTag, "Failed to mount, err: %d", ferr);
    return cpp::fail(Error::FAILED_TO_MOUNT);
  }

  return new SdStorage(gpio, do_transaction, handle, std::move(host),
                       std::move(card), fs);
}

SdStorage::SdStorage(GpioExpander* gpio,
                     esp_err_t (*do_transaction)(sdspi_dev_handle_t,
                                                 sdmmc_command_t*),
                     sdspi_dev_handle_t handle,
                     std::unique_ptr<sdmmc_host_t> host,
                     std::unique_ptr<sdmmc_card_t> card,
                     FATFS* fs)
    : gpio_(gpio),
      do_transaction_(do_transaction),
      handle_(handle),
      host_(std::move(host)),
      card_(std::move(card)),
      fs_(fs) {
  callback::instance = this;
  callback::bootstrap = nullptr;
}

SdStorage::~SdStorage() {
  // Unmount and unregister the filesystem
  f_unmount("");
  ff_diskio_register(fs_->pdrv, NULL);
  esp_vfs_fat_unregister_path(kStoragePath);
  fs_ = nullptr;

  callback::instance = nullptr;

  // Uninstall the SPI driver
  sdspi_host_remove_device(this->handle_);
  sdspi_host_deinit();

  gpio_->set_pin(GpioExpander::SD_CARD_POWER_ENABLE, 0);
  gpio_->set_pin(GpioExpander::SD_MUX_EN_ACTIVE_LOW, 1);
  gpio_->Write();
}

auto SdStorage::HandleTransaction(sdspi_dev_handle_t handle,
                                  sdmmc_command_t* cmdinfo) -> esp_err_t {
  // TODO: not needed anymore?
  return do_transaction_(handle, cmdinfo);
}

auto SdStorage::GetFs() -> FATFS* {
  return fs_;
}

}  // namespace drivers
