#ifndef STORAGE_H
#define STORAGE_H

#include "driver/sdmmc_types.h"
#include "driver/sdspi_host.h"
#include "esp_err.h"
#include "gpio-expander.h"

namespace gay_ipod {

class SdStorage {
  public:
    SdStorage(GpioExpander *gpio);
    ~SdStorage();

    esp_err_t Acquire(void);
    esp_err_t Release(void);

    // Not copyable or movable.
    // TODO: maybe this could be movable?
    SdStorage(const SdStorage&) = delete;
    SdStorage& operator=(const SdStorage&) = delete;

  private:
    GpioExpander *gpio_;

    sdspi_dev_handle_t handle_;
    sdmmc_host_t host_;
    sdmmc_card_t card_;
};

} // namespace gay_ipod

#endif
