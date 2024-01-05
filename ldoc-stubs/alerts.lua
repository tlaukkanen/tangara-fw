--- Module for interacting with playback volume. The Bluetooth and wired outputs store their current volume separately; this API only allows interacting with the volume of the currently used output device.
-- @module alerts

local alerts = {}

--- Returns the current volume as a percentage of the current volume limit.
-- @tparam function constructor Called to create the UI for the alert. A new default root object and group will be set before calling this function.i Alerts are non-interactable; the group created for the constructor will not be granted focus.
function alerts.show(constructor) end

--- Dismisses any visible alerts, removing them from the screen.
function alerts.hide() end

return alerts
