/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lua_testing.hpp"

#include "lauxlib.h"
#include "lua.h"

#include "audio/audio_events.hpp"
#include "events/event_queue.hpp"

namespace lua {

static auto testing_tone(lua_State* L) -> int {
  auto freq = luaL_checkinteger(L, 1);
  events::Audio().Dispatch(
      audio::PlaySineWave{.frequency = static_cast<uint32_t>(freq)});
  events::Audio().Dispatch(audio::TogglePlayPause{.set_to = true});
  return 0;
}

static const struct luaL_Reg kTestingFuncs[] = {
    {"tone", testing_tone},
    {NULL, NULL},
};

static auto lua_testing(lua_State* state) -> int {
  luaL_newlib(state, kTestingFuncs);
  return 1;
}

auto RegisterTestingModule(lua_State* L) -> void {
  luaL_requiref(L, "testing", lua_testing, false);
  lua_pop(L, 1);
}

}  // namespace lua
