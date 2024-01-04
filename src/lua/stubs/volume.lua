--- Module for interacting with playback volume. The Bluetooth and wired outputs store their current volume separately; this API only allows interacting with the volume of the currently used output device.
-- @module volume

local volume = {}

--- Returns the current volume as a percentage of the current volume limit.
-- @treturn types.Property an integer property
function volume.current_pct() end

--- Returns the current volume in terms of dB from line level.
-- @treturn types.Property an integer property
function volume.current_db() end

return volume
