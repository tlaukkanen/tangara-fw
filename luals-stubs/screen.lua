--- @meta

--- A distinct full-screen UI. Each screen has an associated LVGL UI tree and
--- group, and can be shown on-screen using the 'backstack' module.
--- Screens make use of prototype inheritance in order to provide a consistent
--- interface for the C++ firmware to work with.
--- See [Programming in Lua, chapter 16](https://www.lua.org/pil/16.2.html).
--- @class screen
local screen = {}

--- Creates a new screen instance.
function screen:new(params) end

--- Called just before this screen is first displayed to the user.
function screen:createUi() end

--- Called whenever this screen is displayed to the user.
function screen:onShown() end

--- Called whenever this screen is being hidden by the user; either because a
--- new screen is being pushed on top of this way, or because this screen has
--- been popped off of the stack.
function screen:onHidden() end

return screen
