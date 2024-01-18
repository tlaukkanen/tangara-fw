/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "property.hpp"
#include <sys/_stdint.h>

#include <cmath>
#include <memory>
#include <memory_resource>
#include <string>
#include <variant>

#include "bluetooth_types.hpp"
#include "lua.h"
#include "lua.hpp"
#include "lua_thread.hpp"
#include "lvgl.h"
#include "memory_resource.hpp"
#include "service_locator.hpp"
#include "track.hpp"
#include "types.hpp"

namespace lua {

static const char kPropertyMetatable[] = "property";
static const char kFunctionMetatable[] = "c_func";
static const char kBindingsTable[] = "bindings";
static const char kBinderKey[] = "binder";

static auto check_property(lua_State* state) -> Property* {
  void* data = luaL_checkudata(state, 1, kPropertyMetatable);
  luaL_argcheck(state, data != NULL, 1, "`property` expected");
  return *reinterpret_cast<Property**>(data);
}

static auto property_get(lua_State* state) -> int {
  Property* p = check_property(state);
  p->PushValue(*state);
  return 1;
}

static auto property_set(lua_State* state) -> int {
  Property* p = check_property(state);
  luaL_argcheck(state, p->IsTwoWay(), 1, "property is read-only");
  bool valid = p->PopValue(*state);
  lua_pushboolean(state, valid);
  return 1;
}

static auto property_bind(lua_State* state) -> int {
  Property* p = check_property(state);
  luaL_checktype(state, 2, LUA_TFUNCTION);

  // Copy the function, as we need to invoke it then store our reference.
  lua_pushvalue(state, 2);
  // ...and another copy, since we return the original closure.
  lua_pushvalue(state, 2);

  p->PushValue(*state);
  CallProtected(state, 1, 0);  // Invoke the initial binding.

  lua_pushstring(state, kBindingsTable);
  lua_gettable(state, LUA_REGISTRYINDEX);  // REGISTRY[kBindingsTable]
  lua_insert(state, -2);          // Move bindings to the bottom, with fn above.
  int ref = luaL_ref(state, -2);  // bindings[ref] = fn

  p->AddLuaBinding(state, ref);

  // Pop the bindings table, leaving one of the copies of the callback fn at
  // the top of the stack.
  lua_pop(state, 1);

  return 1;
}

static const struct luaL_Reg kPropertyBindingFuncs[] = {{"get", property_get},
                                                        {"set", property_set},
                                                        {"bind", property_bind},
                                                        {NULL, NULL}};

static auto generic_function_cb(lua_State* state) -> int {
  lua_pushstring(state, kBinderKey);
  lua_gettable(state, LUA_REGISTRYINDEX);
  PropertyBindings* binder =
      reinterpret_cast<PropertyBindings*>(lua_touserdata(state, -1));

  size_t* index =
      reinterpret_cast<size_t*>(luaL_checkudata(state, 1, kFunctionMetatable));
  const LuaFunction& fn = binder->GetFunction(*index);

  // Ensure the C++ function is called with a clean stack; we don't want it to
  // see the index we just used.
  lua_remove(state, 1);

  return std::invoke(fn, state);
}

PropertyBindings::PropertyBindings(lua_State& s) {
  lua_pushstring(&s, kBinderKey);
  lua_pushlightuserdata(&s, this);
  lua_settable(&s, LUA_REGISTRYINDEX);

  // Create the metatable responsible for the Property API.
  luaL_newmetatable(&s, kPropertyMetatable);

  lua_pushliteral(&s, "__index");
  lua_pushvalue(&s, -2);
  lua_settable(&s, -3);  // metatable.__index = metatable

  // Add our binding funcs (get, set, bind) to the metatable.
  luaL_setfuncs(&s, kPropertyBindingFuncs, 0);

  // Create a weak table in the registry to hold live bindings.
  lua_pushstring(&s, kBindingsTable);
  lua_newtable(&s);  // bindings = {}

  // Metatable for the weak table. Values are weak.
  lua_newtable(&s);  // meta = {}
  lua_pushliteral(&s, "__mode");
  lua_pushliteral(&s, "v");
  lua_settable(&s, -3);      // meta.__mode='v'
  lua_setmetatable(&s, -2);  // setmetatable(bindings, meta)

  lua_settable(&s, LUA_REGISTRYINDEX);  // REGISTRY[kBindingsTable] = bindings

  // Create the metatable for C++ functions.
  luaL_newmetatable(&s, kFunctionMetatable);

  lua_pushliteral(&s, "__call");
  lua_pushcfunction(&s, generic_function_cb);
  lua_settable(&s, -3);  // metatable.__call = metatable

  lua_pop(&s, 1);  // Clean up the function metatable
}

auto PropertyBindings::Register(lua_State* s, Property* prop) -> void {
  Property** data =
      reinterpret_cast<Property**>(lua_newuserdata(s, sizeof(uintptr_t)));
  *data = prop;

  luaL_setmetatable(s, kPropertyMetatable);
}

auto PropertyBindings::Register(lua_State* s, LuaFunction fn) -> void {
  size_t* index = reinterpret_cast<size_t*>(lua_newuserdata(s, sizeof(size_t)));
  *index = functions_.size();
  functions_.push_back(fn);

  luaL_setmetatable(s, kFunctionMetatable);
}

auto PropertyBindings::GetFunction(size_t i) -> const LuaFunction& {
  assert(i < functions_.size());
  return functions_[i];
};

template <class... Ts>
inline constexpr bool always_false_v = false;

Property::Property(const LuaValue& val)
    : value_(memory::SpiRamAllocator<LuaValue>().new_object<LuaValue>(val)),
      cb_(),
      bindings_(&memory::kSpiRamResource) {}

Property::Property(const LuaValue& val,
                   std::function<bool(const LuaValue& val)> cb)
    : value_(memory::SpiRamAllocator<LuaValue>().new_object<LuaValue>(val)),
      cb_(cb),
      bindings_(&memory::kSpiRamResource) {}

static auto pushTagValue(lua_State* L, const database::TagValue& val) -> void {
  std::visit(
      [&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::pmr::string>) {
          lua_pushlstring(L, arg.data(), arg.size());
        } else if constexpr (std::is_same_v<
                                 T, cpp::span<const std::pmr::string>>) {
          lua_createtable(L, 0, arg.size());
          for (const auto& i : arg) {
            lua_pushlstring(L, i.data(), i.size());
            lua_pushboolean(L, true);
            lua_rawset(L, -3);
          }
        } else if constexpr (std::is_same_v<T, uint32_t>) {
          lua_pushinteger(L, arg);
        } else {
          lua_pushnil(L);
        }
      },
      val);
}

static void pushTrack(lua_State* L, const audio::Track& track) {
  lua_newtable(L);

  for (const auto& tag : track.tags->allPresent()) {
    lua_pushstring(L, database::tagName(tag).c_str());
    pushTagValue(L, track.tags->get(tag));
    lua_settable(L, -3);
  }
  if (track.db_info) {
    lua_pushliteral(L, "id");
    lua_pushinteger(L, track.db_info->id);
    lua_settable(L, -3);
  }

  lua_pushliteral(L, "duration");
  lua_pushinteger(L, track.duration);
  lua_settable(L, -3);

  lua_pushliteral(L, "bitrate_kbps");
  lua_pushinteger(L, track.bitrate_kbps);
  lua_settable(L, -3);

  lua_pushliteral(L, "encoding");
  lua_pushstring(L, codecs::StreamTypeToString(track.encoding).c_str());
  lua_settable(L, -3);
}

static void pushDevice(lua_State* L, const drivers::bluetooth::Device& dev) {
  lua_createtable(L, 0, 4);

  lua_pushliteral(L, "address");
  auto* mac = reinterpret_cast<drivers::bluetooth::mac_addr_t*>(
      lua_newuserdata(L, sizeof(drivers::bluetooth::mac_addr_t)));
  *mac = dev.address;
  lua_rawset(L, -3);

  // What I just did there was perfectly safe. Look, I can prove it:
  static_assert(
      std::is_trivially_copy_assignable<drivers::bluetooth::mac_addr_t>());
  static_assert(
      std::is_trivially_destructible<drivers::bluetooth::mac_addr_t>());

  lua_pushliteral(L, "name");
  lua_pushlstring(L, dev.name.data(), dev.name.size());
  lua_rawset(L, -3);

  // FIXME: This field deserves a little more structure.
  lua_pushliteral(L, "class");
  lua_pushinteger(L, dev.class_of_device);
  lua_rawset(L, -3);

  lua_pushliteral(L, "signal_strength");
  lua_pushinteger(L, dev.signal_strength);
  lua_rawset(L, -3);
}

auto Property::PushValue(lua_State& s) -> int {
  std::visit(
      [&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          lua_pushnil(&s);
        } else if constexpr (std::is_same_v<T, int>) {
          lua_pushinteger(&s, arg);
        } else if constexpr (std::is_same_v<T, bool>) {
          lua_pushboolean(&s, arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
          lua_pushstring(&s, arg.c_str());
        } else if constexpr (std::is_same_v<T, audio::Track>) {
          pushTrack(&s, arg);
        } else if constexpr (std::is_same_v<T, drivers::bluetooth::Device>) {
          pushDevice(&s, arg);
        } else if constexpr (std::is_same_v<
                                 T, std::vector<drivers::bluetooth::Device>>) {
          lua_createtable(&s, arg.size(), 0);
          size_t i = 1;
          for (const auto& dev : arg) {
            pushDevice(&s, dev);
            lua_rawseti(&s, -2, i++);
          }
        } else {
          static_assert(always_false_v<T>, "PushValue missing type");
        }
      },
      *value_);
  return 1;
}

auto popRichType(lua_State* L) -> LuaValue {
  lua_pushliteral(L, "address");
  lua_gettable(L, -2);

  if (lua_isuserdata(L, -1)) {
    // This must be a bt device!
    drivers::bluetooth::mac_addr_t mac =
        *reinterpret_cast<drivers::bluetooth::mac_addr_t*>(
            lua_touserdata(L, -1));
    lua_pop(L, 1);

    lua_pushliteral(L, "name");
    lua_gettable(L, -2);

    std::pmr::string name = lua_tostring(L, -1);
    lua_pop(L, 1);

    return drivers::bluetooth::Device{
        .address = mac,
        .name = name,
        .class_of_device = 0,
        .signal_strength = 0,
    };
  }

  return std::monostate{};
}

auto Property::PopValue(lua_State& s) -> bool {
  LuaValue new_val;
  switch (lua_type(&s, 2)) {
    case LUA_TNIL:
      new_val = std::monostate{};
      break;
    case LUA_TNUMBER:
      if (lua_isinteger(&s, 2)) {
        new_val = lua_tointeger(&s, 2);
      } else {
        new_val = static_cast<lua_Integer>(std::round(lua_tonumber(&s, 2)));
      }
      break;
    case LUA_TBOOLEAN:
      new_val = static_cast<bool>(lua_toboolean(&s, 2));
      break;
    case LUA_TSTRING:
      new_val = lua_tostring(&s, 2);
      break;
    default:
      if (lua_istable(&s, 2)) {
        new_val = popRichType(&s);
        if (std::holds_alternative<std::monostate>(new_val)) {
          return false;
        }
      } else {
        return false;
      }
  }

  if (cb_ && std::invoke(*cb_, new_val)) {
    Update(new_val);
    return true;
  }
  return false;
}

auto Property::Update(const LuaValue& v) -> void {
  *value_ = v;

  for (int i = bindings_.size() - 1; i >= 0; i--) {
    auto& b = bindings_[i];

    lua_pushstring(b.first, kBindingsTable);
    lua_gettable(b.first, LUA_REGISTRYINDEX);       // REGISTRY[kBindingsTable]
    int type = lua_rawgeti(b.first, -1, b.second);  // push bindings[i]

    // Has closure has been GCed?
    if (type == LUA_TNIL) {
      // Clean up after ourselves.
      lua_pop(b.first, 1);
      // Remove the binding.
      bindings_.erase(bindings_.begin() + i);
      continue;
    }

    PushValue(*b.first);           // push the argument
    CallProtected(b.first, 1, 0);  // invoke the closure
  }
}

auto Property::AddLuaBinding(lua_State* state, int ref) -> void {
  bindings_.push_back({state, ref});
}

}  // namespace lua
