--- @meta

--- The `power` module contains properties and functions that relate to the
--- device's battery and charging state.
--- @class power
--- @field battery_pct Property The battery's current charge, as a percentage of the maximum charge.
--- @field battery_millivolts Property The battery's current voltage, in millivolts.
--- @field plugged_in Property Whether or not the device is currently receiving external power.
--- @field fast_charge Property Whether or not fast charging is enabled. Fast charging can fully recharge the battery up to two times faster than regular charging, but will have a small negative impact on the lifetime of the battery.
--- @field charge_state Property a string property describing the current charging state. May be one of "no_battery", "critical", "discharging", "charge_regular", "charge_fast", "full_charge", "fault", or "unknown".
local power = {}

return power
