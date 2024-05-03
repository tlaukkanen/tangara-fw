/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "drivers/spiffs.hpp"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

namespace drivers {

[[maybe_unused]] static constexpr char kTag[] = "spiffs";

static auto mount_script_dir() -> esp_err_t {
  esp_vfs_spiffs_conf_t config{
      .base_path = "/lua",
      .partition_label = "lua",
      .max_files = 5,
      .format_if_mount_failed = false,
  };

  esp_err_t res = esp_vfs_spiffs_register(&config);
  if (res == ESP_OK) {
    size_t total, used;
    esp_spiffs_info("lua", &total, &used);
    ESP_LOGI(kTag, "lua scripts mounted okay. %d / %d ", used / 1024,
             total / 1024);
  }

  return res;
}

static auto mount_repl_dir() -> esp_err_t {
  esp_vfs_spiffs_conf_t config{
      .base_path = "/repl",
      .partition_label = "repl",
      .max_files = 5,
      .format_if_mount_failed = false,
  };

  esp_err_t res = esp_vfs_spiffs_register(&config);
  if (res == ESP_OK) {
    size_t total, used;
    esp_spiffs_info("repl", &total, &used);
    ESP_LOGI(kTag, "lua repl mounted okay. %d / %d ", used / 1024,
             total / 1024);
  }

  return res;
}

esp_err_t spiffs_mount() {
  esp_err_t res;
  if ((res = mount_script_dir()) != ESP_OK) {
    return res;
  }
  if ((res = mount_repl_dir()) != ESP_OK) {
    return res;
  }
  return ESP_OK;
}

}  // namespace drivers
