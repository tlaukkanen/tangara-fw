--- @meta

--- The 'bluetooth' module contains Properties and functions for interacting
--- with the device's Bluetooth capabilities.
--- @class bluetooth
--- @field enabled Property Whether or not the Bluetooth stack is currently enabled. This property is writeable, and can be used to enable or disable Bluetooth.
--- @field connected Property Whether or not there is an active connection to another Bluetooth device.
--- @field paired_device Property The device that is currently paired. The bluetooth stack will automatically connected to this device if possible.
--- @field devices Property Devices nearby that have been discovered.
local bluetooth = {}

return bluetooth
