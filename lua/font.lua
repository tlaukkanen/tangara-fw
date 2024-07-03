local lvgl = require("lvgl")

local fonts = {}
local fonts_priv = {
  has_invoked_cb = false,
  cb = nil,
}

function fonts_priv.has_loaded_all()
  return fonts.fusion_12 and fonts.fusion_10
end

function fonts_priv.invoke_cb()
  if fonts_priv.has_invoked_cb or not fonts_priv.cb then return end
  if not fonts_priv.has_loaded_all() then return end
  fonts_priv.has_invoked_cb = true
  fonts_priv.cb()
end

lvgl.Font("//lua/fonts/fusion12", function(font)
  fonts.fusion_12 = font
  fonts_priv.invoke_cb()
end)

lvgl.Font("//lua/fonts/fusion10", function(font)
  fonts.fusion_10 = font
  fonts_priv.invoke_cb()
end)

function fonts.on_loaded(cb)
  fonts_priv.cb = cb
  fonts_priv.has_invoked_cb = false
  fonts_priv.invoke_cb()
end

return fonts
