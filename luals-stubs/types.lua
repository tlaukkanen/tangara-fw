--- @meta

--- A observable value, owned by the C++ firmware.
---@class Property
local property = {}

--- @return integer|string|table|boolean val Returns the property's current value
function property:get() end

--- Sets a new value. Not all properties may be set from within Lua code. For
--- example, it makes little sense to attempt to override the current battery
--- level.
--- @param val? integer|string|table|boolean The new value. Optional; if not argument is passed, the property will be set to 'nil'.
--- @return boolean success whether or not the new value was accepted
function property:set(val) end

--- Invokes the given function once immediately with the current value, and then again whenever the value changes.
--- The function is invoked for *all* changes; both from the underlying C++ data, and from calls to `set` (if this is a Lua-writeable property).
--- The binding will be active **only** so long as the given function remains in scope.
--- @param fn function callback to apply property values. Must accept one argument; the updated value.
function property:bind(fn) end

return property
