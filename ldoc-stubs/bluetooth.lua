--- Properties and functions for handling Bluetooth connectivity
-- @module bluetooth
local bluetooth = {}

--- Whether or not the Bluetooth stack is currently enabled. This property is writeable, and can be used to enable or disable Bluetooth.
-- @see types.Property
bluetooth.enabled = types.Property

--- Whether or not there is an active connection to another Bluetooth device.
-- @see types.Property
bluetooth.connected = types.Property

return bluetooth
