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
#include <sstream>
#include <string>
#include <variant>

#include "bluetooth_types.hpp"
#include "lauxlib.h"
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
static const char kBindingMetatable[] = "binding";
static const char kBindingsTable[] = "bindings";
static const char kBinderKey[] = "binder";

auto Binding::get(lua_State* L, int idx) -> Binding* {
  return reinterpret_cast<Binding*>(luaL_testudata(L, idx, kBindingMetatable));
}

auto Binding::apply(lua_State* L, int idx) -> bool {
  Binding* b = get(L, idx);
  if (b->dirty && b->active) {
    b->dirty = false;
    // The binding needs to be reapplied. Push the Lua callback, then its arg.
    lua_getiuservalue(L, idx, 1);
    b->property->pushValue(*L);

    // Invoke the callback.
    return CallProtected(L, 1, 0) == LUA_OK;
  }
  return true;
}

static auto check_property(lua_State* state) -> Property* {
  void* data = luaL_checkudata(state, 1, kPropertyMetatable);
  luaL_argcheck(state, data != NULL, 1, "`property` expected");
  return *reinterpret_cast<Property**>(data);
}

static auto property_get(lua_State* state) -> int {
  Property* p = check_property(state);
  p->pushValue(*state);
  return 1;
}

static auto property_set(lua_State* state) -> int {
  Property* p = check_property(state);
  luaL_argcheck(state, p->isTwoWay(), 1, "property is read-only");
  bool valid = p->popValue(*state);
  lua_pushboolean(state, valid);
  return 1;
}

static auto property_bind(lua_State* state) -> int {
  Property* p = check_property(state);
  luaL_checktype(state, 2, LUA_TFUNCTION);

  // Fetch the table of live bindings.
  lua_pushstring(state, kBindingsTable);
  lua_gettable(state, LUA_REGISTRYINDEX);  // REGISTRY[kBindingsTable]

  // Create the userdata holding the new binding's metadata.
  Binding* binding =
      reinterpret_cast<Binding*>(lua_newuserdatauv(state, sizeof(Binding), 1));
  *binding = Binding{.property = p, .active = true, .dirty = true};
  luaL_setmetatable(state, kBindingMetatable);

  // Associate the callback function with the new binding.
  lua_pushvalue(state, 2);
  lua_setiuservalue(state, -2, 1);

  // Put a reference to the binding into the bindings table, so that we can
  // look it up later.
  lua_pushvalue(state, -1);
  int binding_ref = luaL_ref(state, 3);

  // Tell the property about the new binding. This was also perform the initial
  // bind.
  p->addLuaBinding(state, binding_ref);

  // Return the only remaining strong reference to the new Binding.
  return 1;
}

static auto property_tostring(lua_State* state) -> int {
  Property* p = check_property(state);
  p->pushValue(*state);

  std::stringstream str{};
  str << "property { " << luaL_tolstring(state, -1, NULL);
  if (!p->isTwoWay()) {
    str << ", read-only";
  }
  str << " }";

  lua_settop(state, 0);

  std::string res = str.str();
  lua_pushlstring(state, res.data(), res.size());
  return 1;
}

static const struct luaL_Reg kPropertyBindingFuncs[] = {
    {"get", property_get},
    {"set", property_set},
    {"bind", property_bind},
    {"__tostring", property_tostring},
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

PropertyBindings::PropertyBindings() : functions_(&memory::kSpiRamResource) {}

auto PropertyBindings::install(lua_State* L) -> void {
  lua_pushstring(L, kBinderKey);
  lua_pushlightuserdata(L, this);
  lua_settable(L, LUA_REGISTRYINDEX);

  // Create the metatable responsible for the Property API.
  luaL_newmetatable(L, kPropertyMetatable);

  lua_pushliteral(L, "__index");
  lua_pushvalue(L, -2);
  lua_settable(L, -3);  // metatable.__index = metatable

  // Add our binding funcs (get, set, bind) to the metatable.
  luaL_setfuncs(L, kPropertyBindingFuncs, 0);

  // We've finished setting up the metatable, so pop it.
  lua_pop(L, 1);

  // Create the metatable responsible for each Binding. This metatable is empty
  // as it's only used for identification.
  luaL_newmetatable(L, kBindingMetatable);
  lua_pop(L, 1);

  // Create a weak table in the registry to hold live bindings.
  lua_pushstring(L, kBindingsTable);
  lua_newtable(L);  // bindings = {}

  // Metatable for the weak table. Values are weak.
  lua_newtable(L);  // meta = {}
  lua_pushliteral(L, "__mode");
  lua_pushliteral(L, "v");
  lua_settable(L, -3);      // meta.__mode='v'
  lua_setmetatable(L, -2);  // setmetatable(bindings, meta)

  lua_settable(L, LUA_REGISTRYINDEX);  // REGISTRY[kBindingsTable] = bindings

  // Create the metatable for C++ functions.
  luaL_newmetatable(L, kFunctionMetatable);

  lua_pushliteral(L, "__call");
  lua_pushcfunction(L, generic_function_cb);
  lua_settable(L, -3);  // metatable.__call = metatable

  lua_pop(L, 1);  // Clean up the function metatable
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

auto Property::setDirect(const LuaValue& val) -> void {
  *value_ = val;
  reapplyAll();
}

auto Property::set(const LuaValue& val) -> bool {
  if (cb_ && !std::invoke(*cb_, val)) {
    return false;
  }
  setDirect(val);
  return true;
}

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

static void pushTrack(lua_State* L, const audio::TrackInfo& track) {
  lua_newtable(L);

  for (const auto& tag : track.tags->allPresent()) {
    lua_pushstring(L, database::tagName(tag).c_str());
    pushTagValue(L, track.tags->get(tag));
    lua_settable(L, -3);
  }

  if (track.duration) {
    lua_pushliteral(L, "duration");
    lua_pushinteger(L, track.duration.value());
    lua_settable(L, -3);
  }

  if (track.bitrate_kbps) {
    lua_pushliteral(L, "bitrate_kbps");
    lua_pushinteger(L, track.bitrate_kbps.value());
    lua_settable(L, -3);
  }

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

auto Property::pushValue(lua_State& s) -> int {
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
        } else if constexpr (std::is_same_v<T, audio::TrackInfo>) {
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

auto Property::popValue(lua_State& s) -> bool {
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

  return set(new_val);
}

auto Property::reapplyAll() -> void {
  for (int i = bindings_.size() - 1; i >= 0; i--) {
    auto& b = bindings_[i];
    if (!applySingle(b.first, b.second, true)) {
      // Remove the binding if we weren't able to apply it. This is usually due
      // to the binding getting GC'd.
      bindings_.erase(bindings_.begin() + i);
    }
  }
}

auto Property::applySingle(lua_State* L, int ref, bool mark_dirty) -> bool {
  int top = lua_gettop(L);

  // Push the table of bindings.
  lua_pushstring(L, kBindingsTable);
  lua_gettable(L, LUA_REGISTRYINDEX);

  // Resolve the reference.
  int type = lua_rawgeti(L, -1, ref);
  if (type == LUA_TNIL) {
    lua_settop(L, top);
    return false;
  }

  // Defensively check that the ref was actually for a Binding.
  Binding* b = Binding::get(L, -1);
  if (!b) {
    lua_settop(L, top);
    return false;
  }

  if (mark_dirty) {
    b->dirty = true;
  }

  bool ret = Binding::apply(L, -1);
  lua_settop(L, top);
  return ret;
}

auto Property::addLuaBinding(lua_State* state, int ref) -> void {
  bindings_.push_back({state, ref});
  applySingle(state, ref, true);
}

}  // namespace lua
