/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "spi.hpp"

#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "hal/spi_types.h"

namespace drivers {

static const spi_host_device_t kSpiHost = VSPI_HOST;
static const gpio_num_t kSpiSdoPin = GPIO_NUM_23;
static const gpio_num_t kSpiSdiPin = GPIO_NUM_19;
static const gpio_num_t kSpiSclkPin = GPIO_NUM_18;

esp_err_t init_spi(void) {
  spi_bus_config_t config = {
      .mosi_io_num = kSpiSdoPin,
      .miso_io_num = kSpiSdiPin,
      .sclk_io_num = kSpiSclkPin,
      .quadwp_io_num = -1,  // SPI_QUADWP_IO,
      .quadhd_io_num = -1,  // SPI_QUADHD_IO,

      // Unused
      .data4_io_num = -1,
      .data5_io_num = -1,
      .data6_io_num = -1,
      .data7_io_num = -1,

      // Use the DMA default size. The display requires larger buffers, but it
      // manages its down use of DMA-capable memory.
      .max_transfer_sz = 128 * 16 * 2,  // TODO: hmm
      .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_IOMUX_PINS,
      .intr_flags = ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM,
      //.intr_flags = ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_SHARED |
      // ESP_INTR_FLAG_IRAM,
  };

  if (esp_err_t err = spi_bus_initialize(kSpiHost, &config, SPI_DMA_CH_AUTO)) {
    return err;
  }

  return ESP_OK;
}

esp_err_t deinit_spi(void) {
  return spi_bus_free(kSpiHost);
}

}  // namespace drivers
