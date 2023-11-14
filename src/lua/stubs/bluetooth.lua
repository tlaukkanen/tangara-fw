--- Properties and functions for handling Bluetooth connectivity
-- @module bluetooth

local bluetooth = {}

-- Whether or not the Bluetooth stack is currently enabled. This property is writeable, and can be used to enable or disable Bluetooth.
-- @treturn types.Property a boolean property
function bluetooth.enabled() end

--- Whether or not there is an active connection to another Bluetooth device.
-- @treturn types.Property a boolean property
function bluetooth.connected() end

return bluetooth
