--- Userdata-based types used throughout the rest of the API. These types are
--- not generally constructable within Lua code.
-- @module types
local types = {}

--- A observable value, owned by the C++ firmware.
-- @type Property
types.Property = {}

--- Gets the current value
-- @return The property's current value.
function Property:get() end

--- Sets a new value. Not all properties may be set from within Lua code. For
--- example, it makes little sense to attempt to override the current battery
--- level.
-- @param val The new value. This should generally be of the same type as the existing value.
-- @return true if the new value was applied, or false if the backing C++ code rejected the new value (e.g. if it was out of range, or the wrong type).
function Property:set(val) end

--- Invokes the given function once immediately with the current value, and then again whenever the value changes.
--- The function is invoked for *all* changes; both from the underlying C++ data, and from calls to `set` (if this is a Lua-writeable property).
--- The binding will be active **only** so long as the given function remains in scope.
-- @param fn callback function to apply property values. Must accept one argument; the updated value.
-- @return fn, for more ergonmic use with anonymous closures.
function Property:bind(fn) end

--- Table containing information about a track. Most fields are optional.
-- @type Track
types.Track = {}

--- The title of the track, or the filename if no title is available.
types.Track.title = ""

return Property
