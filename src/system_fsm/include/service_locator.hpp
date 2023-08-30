/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "battery.hpp"
#include "bluetooth.hpp"
#include "database.hpp"
#include "gpios.hpp"
#include "nvs.hpp"
#include "samd.hpp"
#include "tag_parser.hpp"
#include "touchwheel.hpp"
#include "track_queue.hpp"

namespace system_fsm {

class ServiceLocator {
 public:
  static auto instance() -> ServiceLocator&;

  auto gpios() -> drivers::Gpios& {
    assert(gpios_ != nullptr);
    return *gpios_;
  }

  auto gpios(std::unique_ptr<drivers::Gpios> i) { gpios_ = std::move(i); }

  auto samd() -> drivers::Samd& {
    assert(samd_ != nullptr);
    return *samd_;
  }

  auto samd(std::unique_ptr<drivers::Samd> i) { samd_ = std::move(i); }

  auto nvs() -> drivers::NvsStorage& {
    assert(nvs_ != nullptr);
    return *nvs_;
  }

  auto nvs(std::unique_ptr<drivers::NvsStorage> i) { nvs_ = std::move(i); }

  auto bluetooth() -> drivers::Bluetooth& {
    assert(bluetooth_ != nullptr);
    return *bluetooth_;
  }

  auto bluetooth(std::unique_ptr<drivers::Bluetooth> i) {
    bluetooth_ = std::move(i);
  }

  auto battery() -> battery::Battery& {
    assert(battery_ != nullptr);
    return *battery_;
  }

  auto battery(std::unique_ptr<battery::Battery> i) { battery_ = std::move(i); }

  auto touchwheel() -> std::optional<drivers::TouchWheel*> {
    if (!touchwheel_) {
      return {};
    }
    return touchwheel_.get();
  }

  auto touchwheel(std::unique_ptr<drivers::TouchWheel> i) {
    touchwheel_ = std::move(i);
  }

  auto database() -> std::weak_ptr<database::Database> { return database_; }

  auto database(std::unique_ptr<database::Database> i) {
    database_ = std::move(i);
  }

  auto tag_parser() -> database::ITagParser& {
    assert(tag_parser_ != nullptr);
    return *tag_parser_;
  }

  auto tag_parser(std::unique_ptr<database::ITagParser> i) {
    tag_parser_ = std::move(i);
  }

  auto track_queue() -> audio::TrackQueue& {
    assert(queue_ != nullptr);
    return *queue_;
  }

  auto track_queue(std::unique_ptr<audio::TrackQueue> i) {
    queue_ = std::move(i);
  }

 private:
  std::unique_ptr<drivers::Gpios> gpios_;
  std::unique_ptr<drivers::Samd> samd_;
  std::unique_ptr<drivers::NvsStorage> nvs_;
  std::unique_ptr<drivers::TouchWheel> touchwheel_;
  std::unique_ptr<drivers::Bluetooth> bluetooth_;

  std::unique_ptr<audio::TrackQueue> queue_;
  std::unique_ptr<battery::Battery> battery_;

  std::shared_ptr<database::Database> database_;
  std::unique_ptr<database::ITagParser> tag_parser_;
};

}  // namespace system_fsm
