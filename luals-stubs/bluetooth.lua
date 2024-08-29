--- @meta

--- The 'bluetooth' module contains Properties and functions for interacting
--- with the device's Bluetooth capabilities.
--- @class bluetooth
--- @field enabled Property Whether or not the Bluetooth stack is currently enabled. This property is writeable, and can be used to enable or disable Bluetooth.
--- @field connected Property Whether or not there is an active connection to another Bluetooth device.
--- @field connecting Property Whether or not we are currently connecting to the paired device.
--- @field discovering Property Whether or not we are actively scanning for new devices.
--- @field paired_device Property The device that is currently paired. The bluetooth stack will automatically connect to this device if possible.
--- @field discovered_devices Property Devices nearby that have been discovered.
--- @field known_devices Property Devices that have previously been paired.
local bluetooth = {}

--- Enables Bluetooth, this is the same as bluetooth.enabled:set(true)
function bluetooth.enable() end

--- Disables Bluetooth, this is the same as bluetooth.enabled:set(false)
function bluetooth.disable() end

return bluetooth
