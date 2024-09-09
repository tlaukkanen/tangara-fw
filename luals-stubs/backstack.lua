--- @meta

--- The `backstack` module contains functions that can be used to implement a
--- basic stack-based navigation hierarchy. See also the `screen` module, which
--- provides a class prototype meant for use with this module.
--- @class backstack
local backstack = {}

--- Displays the given screen to the user. If there was already a screen being
--- displayed, then the current screen is removed from the display, and added
--- to the backstack.
--- @param screen screen The screen to display.
function backstack.push(screen) end

--- Removes the current screen from the display, then replaces it with the
--- screen that is at the top of the backstack. This function does nothing if
--- there are no other screens in the stack.
function backstack.pop() end

--- Sets a new root screen, replacing any existing screens
--- @param screen screen The new root screen
function backstack.reset(screen) end

return backstack
