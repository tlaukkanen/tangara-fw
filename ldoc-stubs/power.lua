--- Properties and functions that deal with the device's battery and power state
-- @module power

local power = {}

--- The battery's current charge as a percentage
-- @see types.Property
power.battery_pct = types.Property

--- The battery's current voltage, in millivolts.
-- @see types.Property
power.battery_millivolts = types.Property

--- Whether or not the device is currently receiving external power
-- @see types.Property
power.plugged_in = types.Property

return power
