--- @meta 

--- The `alerts` module contains functions for showing transient popups over
--- the current screen.
--- @class alerts
local alerts = {}

--- Shows a new alert, replacing any other alerts.
--- @param constructor function Called to create the UI for the alert. A new default root object and group will be set before calling this function.i Alerts are non-interactable; the group created for the constructor will not be granted focus.
function alerts.show(constructor) end

--- Dismisses any visible alerts, removing them from the screen.
function alerts.hide() end

return alerts
