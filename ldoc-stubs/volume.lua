--- Module for interacting with playback volume. The Bluetooth and wired outputs store their current volume separately; this API only allows interacting with the volume of the currently used output device.
-- @module volume

local volume = {}

--- The current volume as a percentage of the current volume limit.
-- @see types.Property
volume.current_pct = types.Property

--- The current volume in terms of decibels relative to line level.
-- @see types.Property
volume.current_db = types.Property

return volume
