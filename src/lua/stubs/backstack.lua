--- Module for adding and removing screens from the system's backstack.
-- @module backstack

local backstack = {}

--- Pushes a new screen onto the backstack.
-- @tparam function constructor Called to create the UI for the new screen. A new default root object and group will be set before calling this function. The function provided should return a table holding any bindings used by this screen; the returned value is retained so long as this screen is present in the backstack.
function backstack.push(constructor) end

--- Removes the currently active screen, and instead shows the screen underneath it on the backstack. Does nothing if this is the only existing screen.
function backstack.pop() end

return backstack
