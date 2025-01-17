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
function screen:create_ui() end

--- Called whenever this screen is displayed to the user.
function screen:on_show() end

--- Called whenever this screen is being hidden by the user; either because a
--- new screen is being pushed on top of this way, or because this screen has
--- been popped off of the stack.
function screen:on_hide() end

--- Called when this screen is about to be popped off of the stack. If this
--- returns false, it will not be popped. May be a function, or any boolean
--- convertable value.
function screen:can_pop() end

return screen
