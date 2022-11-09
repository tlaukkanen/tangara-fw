#pragma once

#include "esp_err.h"

namespace drivers {

esp_err_t init_spi(void);
esp_err_t deinit_spi(void);

}  // namespace drivers
