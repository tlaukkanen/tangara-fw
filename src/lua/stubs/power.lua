--- Properties and functions that deal with the device's battery and power state
-- @module power

local power = {}

--- battery_pct returns the battery's current charge as a percentage
-- @treturn types.Property an integer property, from 0 to 100
function power.battery_pct() end

--- battery_millivolts returns the battery's current voltage in millivolts
-- @treturn types.Property an integer property, typically from about 3000 to about 4200.
function power.battery_millivolts() end

--- plugged_in returns whether or not the device is currently receiving external power
-- @treturn types.Property a boolean property
function power.plugged_in() end

return power