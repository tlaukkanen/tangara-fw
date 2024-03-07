--- @meta

--- Module for interacting with playback volume. The Bluetooth and wired outputs store their current volume separately; this API only allows interacting with the volume of the currently used output device.
--- @class volume
--- @field current_pct Property The current volume as a percentage of the current volume limit.
--- @field current_db Property The current volume in terms of decibels relative to line level (only applicable to headphone output)
--- @field left_bias Property An additional modifier in decibels to apply to the left channel (only applicable to headphone output)
--- @field limit_db Property The maximum allowed output volume, in terms of decibels relative to line level (only applicable to headphone output)
local volume = {}

return volume
