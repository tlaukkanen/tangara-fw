/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <set>

#include "core/lv_group.h"
#include "drivers/gpios.hpp"
#include "hal/lv_hal_indev.h"
#include "input/device_factory.hpp"
#include "input/feedback_device.hpp"

#include "input/input_device.hpp"
#include "input/input_hook.hpp"
#include "lua/lua_thread.hpp"
#include "lua/property.hpp"
#include "drivers/nvs.hpp"
#include "drivers/touchwheel.hpp"

namespace input {

/*
 * Implementation of an LVGL input device. This class composes multiple
 * IInputDevice and IFeedbackDevice instances together into a single LVGL
 * device.
 */
class LvglInputDriver {
 public:
  LvglInputDriver(drivers::NvsStorage& nvs, DeviceFactory&);

  auto mode() -> lua::Property& { return mode_; }

  auto read(lv_indev_data_t* data) -> void;
  auto feedback(uint8_t) -> void;

  auto registration() -> lv_indev_t* { return registration_; }
  auto lock(bool l) -> void { is_locked_ = l; }

  auto pushHooks(lua_State* L) -> int;

 private:
  drivers::NvsStorage& nvs_;
  DeviceFactory& factory_;

  lua::Property mode_;
  lv_indev_drv_t driver_;
  lv_indev_t* registration_;

  std::vector<std::shared_ptr<IInputDevice>> inputs_;
  std::vector<std::shared_ptr<IFeedbackDevice>> feedbacks_;

  /*
   * Key for identifying which device, trigger, and specific hook are being
   * overriden by Lua.
   */
  struct OverrideSelector {
    std::string device_name;
    std::string trigger_name;
    std::string hook_name;

    friend bool operator<(const OverrideSelector& l,
                          const OverrideSelector& r) {
      return std::tie(l.device_name, l.trigger_name, l.hook_name) <
             std::tie(r.device_name, r.trigger_name, r.hook_name);
    }
  };

  /* Userdata object for tracking the Lua mirror of a TriggerHooks object. */
  class LuaTrigger {
   public:
    LuaTrigger(LvglInputDriver&, IInputDevice&, TriggerHooks&);

    static auto get(lua_State*, int idx) -> LuaTrigger&;
    static auto luaGc(lua_State*) -> int;
    static auto luaToString(lua_State*) -> int;
    static auto luaNewIndex(lua_State*) -> int;

    static constexpr struct luaL_Reg kFuncs[] = {{"__gc", luaGc},
                                                 {"__tostring", luaToString},
                                                 {"__newindex", luaNewIndex},
                                                 {NULL, NULL}};

   private:
    LvglInputDriver* driver_;

    std::string device_;
    std::string trigger_;
    std::map<std::string, std::string> hooks_;
  };

  /* A hook override implemented as a lua callback */
  struct LuaOverride {
    lua_State* L;
    int ref;
  };

  std::map<OverrideSelector, LuaOverride> overrides_;

  auto setOverride(lua_State* L, const OverrideSelector&) -> void;
  auto applyOverride(const OverrideSelector&, LuaOverride&) -> void;

  bool is_locked_;
};

}  // namespace input
