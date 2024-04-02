--- @meta

--- The `power` module contains properties and functions that relate to the
--- device's battery and charging state.
--- @class power
--- @field battery_pct Property The battery's current charge, as a percentage of the maximum charge.
--- @field battery_millivolts Property The battery's current voltage, in millivolts.
--- @field plugged_in Property Whether or not the device is currently receiving external power.
local power = {}

return power
