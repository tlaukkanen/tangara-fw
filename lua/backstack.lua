local lvgl = require("lvgl")

local backstack = {
  root = lvgl.Object(nil, {
    w = lvgl.HOR_RES(),
    h = lvgl.VER_RES(),
  }),
  stack = {},
}

function backstack:Top()
  return self.stack[#self.stack]
end

function backstack:SetTopParent(parent)
  local top = self:Top()
  if top and top.root then
    top.root:set_parent(parent)
  end
end

function backstack:Push(screen)
  self:SetTopParent(nil)
  table.insert(self.stack, screen)
  self:SetTopParent(self.root)
end

function backstack:Pop(num)
  num = num or 1
  for _ = 1, num do
    local removed = table.remove(self.stack)
    removed.root:delete()
  end
  self:SetTopParent(self.root)
end

return backstack
