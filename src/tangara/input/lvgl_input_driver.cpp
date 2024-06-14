/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input/lvgl_input_driver.hpp"

#include <cstdint>
#include <memory>
#include <variant>

#include "core/lv_group.h"
#include "indev/lv_indev.h"
#include "lua.hpp"
#include "lvgl.h"

#include "drivers/nvs.hpp"

#include "input/device_factory.hpp"
#include "input/feedback_haptics.hpp"
#include "input/input_hook.hpp"
#include "input/input_touch_wheel.hpp"
#include "input/input_trigger.hpp"
#include "input/input_volume_buttons.hpp"
#include "lua/lua_thread.hpp"
#include "lua/property.hpp"
#include "misc/lv_event.h"

[[maybe_unused]] static constexpr char kTag[] = "input";

static constexpr char kLuaTriggerMetatableName[] = "input_trigger";
static constexpr char kLuaOverrideText[] = "lua_callback";

namespace input {

static void read_cb(lv_indev_t* dev, lv_indev_data_t* data) {
  LvglInputDriver* instance =
      reinterpret_cast<LvglInputDriver*>(lv_indev_get_user_data(dev));
  instance->read(data);
}

static void feedback_cb(lv_event_t* ev) {
  LvglInputDriver* instance =
      reinterpret_cast<LvglInputDriver*>(lv_event_get_user_data(ev));
  instance->feedback(lv_event_get_code(ev));
}

static void focus_cb(lv_group_t* group) {
  LvglInputDriver* instance =
      reinterpret_cast<LvglInputDriver*>(group->user_data);
  instance->feedback(LV_EVENT_FOCUSED);
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
      inputs_(factory.createInputs(nvs.PrimaryInput())),
      feedbacks_(factory.createFeedbacks()),
      is_locked_(false) {
  device_ = lv_indev_create();
  lv_indev_set_type(device_, LV_INDEV_TYPE_ENCODER);
  lv_indev_set_user_data(device_, this);
  lv_indev_set_read_cb(device_, read_cb);
  lv_indev_add_event_cb(device_, feedback_cb, LV_EVENT_ALL, this);
}

auto LvglInputDriver::setGroup(lv_group_t* g) -> void {
  lv_group_t* prev = lv_indev_get_group(device_);
  if (prev && prev != g) {
    lv_group_set_focus_cb(prev, NULL);
  }
  if (!g) {
    return;
  }
  lv_indev_set_group(device_, g);

  g->user_data = this;
  lv_group_set_focus_cb(g, focus_cb);

  // Emit a synthetic 'focus' event for the current selection, since
  // otherwise our feedback devices won't know that the selection changed.
  feedback(LV_EVENT_FOCUSED);
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
    device->feedback(lv_indev_get_group(device_), event);
  }
}

LvglInputDriver::LuaTrigger::LuaTrigger(LvglInputDriver& driver,
                                        IInputDevice& dev,
                                        TriggerHooks& trigger)
    : driver_(&driver), device_(dev.name()), trigger_(trigger.name()) {
  for (auto& hook : trigger.hooks()) {
    auto cb = hook.get().callback();
    if (cb) {
      hooks_[hook.get().name()] = hook.get().callback()->name;
    } else {
      hooks_[hook.get().name()] = "";
    }
  }
}

auto LvglInputDriver::LuaTrigger::get(lua_State* L, int idx) -> LuaTrigger& {
  return **reinterpret_cast<LuaTrigger**>(
      luaL_checkudata(L, idx, kLuaTriggerMetatableName));
}

auto LvglInputDriver::LuaTrigger::luaGc(lua_State* L) -> int {
  LuaTrigger& trigger = LuaTrigger::get(L, 1);
  delete &trigger;
  return 0;
}

auto LvglInputDriver::LuaTrigger::luaToString(lua_State* L) -> int {
  LuaTrigger& trigger = LuaTrigger::get(L, 1);
  std::stringstream out;
  out << "{ ";
  for (const auto& hook : trigger.hooks_) {
    if (!hook.second.empty()) {
      out << hook.first << "=" << hook.second << " ";
    }
  }
  out << "}";
  lua_pushlstring(L, out.str().data(), out.str().size());
  return 1;
}

auto LvglInputDriver::LuaTrigger::luaNewIndex(lua_State* L) -> int {
  LuaTrigger& trigger = LuaTrigger::get(L, 1);
  luaL_checktype(L, 3, LUA_TFUNCTION);

  size_t len = 0;
  const char* str = luaL_checklstring(L, 2, &len);
  if (!str) {
    return 0;
  }
  OverrideSelector selector{
      .device_name = trigger.device_,
      .trigger_name = trigger.trigger_,
      .hook_name = std::string{str, len},
  };
  for (const auto& hook : trigger.hooks_) {
    if (hook.first == selector.hook_name) {
      trigger.driver_->setOverride(L, selector);
      trigger.hooks_[hook.first] = kLuaOverrideText;
      return 0;
    }
  }
  return 0;
}

auto LvglInputDriver::pushHooks(lua_State* L) -> int {
  if (luaL_getmetatable(L, kLuaTriggerMetatableName) == LUA_TNIL) {
    luaL_newmetatable(L, kLuaTriggerMetatableName);
    luaL_setfuncs(L, LuaTrigger::kFuncs, 0);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  lua_newtable(L);

  for (auto& dev : inputs_) {
    lua_pushlstring(L, dev->name().data(), dev->name().size());
    lua_newtable(L);

    for (auto& trigger : dev->triggers()) {
      lua_pushlstring(L, trigger.get().name().data(),
                      trigger.get().name().size());
      LuaTrigger** lua_obj = reinterpret_cast<LuaTrigger**>(
          lua_newuserdatauv(L, sizeof(LuaTrigger*), 0));
      *lua_obj = new LuaTrigger(*this, *dev, trigger);
      luaL_setmetatable(L, kLuaTriggerMetatableName);
      lua_rawset(L, -3);
    }

    lua_rawset(L, -3);
  }

  return 1;
}

auto LvglInputDriver::setOverride(lua_State* L,
                                  const OverrideSelector& selector) -> void {
  if (overrides_.contains(selector)) {
    LuaOverride& prev = overrides_[selector];
    luaL_unref(prev.L, LUA_REGISTRYINDEX, prev.ref);
  }

  int ref = luaL_ref(L, LUA_REGISTRYINDEX);
  LuaOverride override{
      .L = L,
      .ref = ref,
  };
  overrides_[selector] = override;
  applyOverride(selector, override);
}

auto LvglInputDriver::applyOverride(const OverrideSelector& selector,
                                    LuaOverride& override) -> void {
  // In general, this algorithm is a very slow approach. We could do better
  // by maintaing maps from [device|trigger|hook]_name to the relevant
  // trigger, but in practice I expect maybe like 5 overrides total ever,
  // spread across 2 devices with 2 or 5 hooks each. So it's not that big a
  // deal. Don't worry about it!!

  // Look for a matching device.
  for (auto& device : inputs_) {
    if (device->name() != selector.device_name) {
      continue;
    }
    // Look for a matching trigger
    for (auto& trigger : device->triggers()) {
      if (trigger.get().name() != selector.trigger_name) {
        continue;
      }
      // Look for a matching hook
      for (auto& hook : trigger.get().hooks()) {
        if (hook.get().name() != selector.hook_name) {
          continue;
        }
        // We found the target! Apply the override.
        auto lua_callback = [=](lv_indev_data_t* d) {
          lua_rawgeti(override.L, LUA_REGISTRYINDEX, override.ref);
          lua::CallProtected(override.L, 0, 0);
        };
        hook.get().override(
            HookCallback{.name = kLuaOverrideText, .fn = lua_callback});
      }
    }
  }
}

}  // namespace input
