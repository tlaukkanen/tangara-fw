/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lvgl_input_driver.hpp"
#include <stdint.h>

#include <cstdint>
#include <memory>
#include <variant>

#include "device_factory.hpp"
#include "feedback_haptics.hpp"
#include "input_touch_wheel.hpp"
#include "input_trigger.hpp"
#include "input_volume_buttons.hpp"
#include "lauxlib.h"
#include "lua.h"
#include "lvgl.h"
#include "nvs.hpp"
#include "property.hpp"

[[maybe_unused]] static constexpr char kTag[] = "input";

namespace input {

static void read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  LvglInputDriver* instance =
      reinterpret_cast<LvglInputDriver*>(drv->user_data);
  instance->read(data);
}

static void feedback_cb(lv_indev_drv_t* drv, uint8_t event) {
  LvglInputDriver* instance =
      reinterpret_cast<LvglInputDriver*>(drv->user_data);
  instance->feedback(event);
}

auto intToMode(int raw) -> std::optional<drivers::NvsStorage::InputModes> {
  switch (raw) {
    case 0:
      return drivers::NvsStorage::InputModes::kButtonsOnly;
    case 1:
      return drivers::NvsStorage::InputModes::kButtonsWithWheel;
    case 2:
      return drivers::NvsStorage::InputModes::kDirectionalWheel;
    case 3:
      return drivers::NvsStorage::InputModes::kRotatingWheel;
    default:
      return {};
  }
}

LvglInputDriver::LvglInputDriver(drivers::NvsStorage& nvs,
                                 DeviceFactory& factory)
    : nvs_(nvs),
      factory_(factory),
      mode_(static_cast<int>(nvs.PrimaryInput()),
            [&](const lua::LuaValue& val) {
              if (!std::holds_alternative<int>(val)) {
                return false;
              }
              auto mode = intToMode(std::get<int>(val));
              if (!mode) {
                return false;
              }
              nvs.PrimaryInput(*mode);
              inputs_ = factory.createInputs(*mode);
              return true;
            }),
      driver_(),
      registration_(nullptr),
      inputs_(factory.createInputs(nvs.PrimaryInput())),
      feedbacks_(factory.createFeedbacks()),
      is_locked_(false) {
  lv_indev_drv_init(&driver_);
  driver_.type = LV_INDEV_TYPE_ENCODER;
  driver_.read_cb = read_cb;
  driver_.feedback_cb = feedback_cb;
  driver_.user_data = this;
  driver_.long_press_time = kLongPressDelayMs;
  driver_.long_press_repeat_time = kRepeatDelayMs;

  registration_ = lv_indev_drv_register(&driver_);
}

auto LvglInputDriver::read(lv_indev_data_t* data) -> void {
  // TODO: we should pass lock state on to the individual devices, since they
  // may wish to either ignore the lock state, or power down until unlock.
  if (is_locked_) {
    return;
  }
  for (auto&& device : inputs_) {
    device->read(data);
  }
}

auto LvglInputDriver::feedback(uint8_t event) -> void {
  if (is_locked_) {
    return;
  }
  for (auto&& device : feedbacks_) {
    device->feedback(event);
  }
}

auto LvglInputDriver::pushHooks(lua_State* L) -> int {
  lua_newtable(L);

  for (auto& dev : inputs_) {
    lua_pushlstring(L, dev->name().data(), dev->name().size());
    lua_newtable(L);

    for (auto& hook : dev->hooks()) {
      lua_pushlstring(L, hook.name().data(), hook.name().size());
      hook.pushHooks(L);
      lua_rawset(L, -3);
    }

    lua_rawset(L, -3);
  }
  return 1;
}

}  // namespace input
