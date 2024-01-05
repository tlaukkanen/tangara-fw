--- @meta

---@class Property
local property = {}

function property:get() end

function property:set(val) end

--- @param fn function
function property:bind(fn) end

return property
