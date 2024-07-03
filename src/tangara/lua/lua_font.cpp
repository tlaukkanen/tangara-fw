/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "lua_font.hpp"

#include <cstdint>
#include <string>

#include "lauxlib.h"
#include "lua.h"
#include "lvgl.h"

#include "events/event_queue.hpp"
#include "lua/bridge.hpp"
#include "lua/lua_registry.hpp"
#include "lua/lua_thread.hpp"

namespace lua {

[[maybe_unused]] static constexpr char kTag[] = "lua_font";

/* Reads the given file completely into PSRAM. */
static auto readFont(std::string path) -> std::span<uint8_t> {
  // This following is a bit C-brained. Sorry.
  FILE* f = fopen(path.c_str(), "r");
  if (!f) {
    return {};
  }

  uint8_t* data = NULL;
  long len = 0;

  if (fseek(f, 0, SEEK_END)) {
    goto fail;
  }

  len = ftell(f);
  if (len <= 0) {
    goto fail;
  }

  if (fseek(f, 0, SEEK_SET)) {
    len = 0;
    goto fail;
  }

  data = reinterpret_cast<uint8_t*>(heap_caps_malloc(len, MALLOC_CAP_SPIRAM));
  if (!data) {
    len = 0;
    goto fail;
  }

  if (fread(data, 1, len, f) < len) {
    heap_caps_free(data);
    len = 0;
  }

fail:
  fclose(f);

  return {data, static_cast<size_t>(len)};
}

static auto parseFont(std::span<uint8_t> data) -> lv_font_t* {
  if (data.empty()) {
    return nullptr;
  }

  lv_font_t* font = lv_binfont_create_from_buffer(data.data(), data.size());
  heap_caps_free(data.data());

  return font;
}

auto loadFont(lua_State* L, const char* path, int cb_ref) -> void {
  // Most Lua file paths start with "//" in order to deal with LVGL's Windows-y
  // approach to paths. Try to handle such paths correctly so that paths in Lua
  // code look a bit more consistent.
  std::string path_str = path;
  if (path_str.starts_with("//")) {
    path++;
    path_str = path;
  }

  // Do the file read from the current thread, since the path might be for a
  // file in flash, and we can't read from flash in a background task.
  auto font_data = readFont(path_str);

  Bridge* bridge = Bridge::Get(L);
  bridge->services().bg_worker().Dispatch<void>([=]() {
    // Do the parsing now that we're in the background.
    lv_font_t* font = parseFont(font_data);

    // Hop back to the UI task to invoke the Lua callback.
    events::Ui().RunOnTask([=] {
      // Retrieve the callback by ref, and release the ref.
      lua_rawgeti(L, LUA_REGISTRYINDEX, cb_ref);
      luaL_unref(L, LUA_REGISTRYINDEX, cb_ref);

      // We always invoke the callback, but we don't always have a result.
      if (font) {
        lua_pushlightuserdata(L, (void*)font);
      } else {
        lua_pushnil(L);
      }

      CallProtected(L, 1, 0);
    });
  });
}

}  // namespace lua
