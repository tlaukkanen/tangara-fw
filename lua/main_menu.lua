local lvgl = require("lvgl")
local widgets = require("widgets")
local legacy_ui = require("legacy_ui")
local database = require("database")
local backstack = require("backstack")
local browser = require("browser")
local playing = require("playing")
local theme = require("theme")

return function()
  local menu = {}
  menu.root = lvgl.Object(nil, {
    flex = {
      flex_direction = "column",
      flex_wrap = "wrap",
      justify_content = "flex-start",
      align_items = "flex-start",
      align_content = "flex-start",
    },
    w = lvgl.HOR_RES(),
    h = lvgl.VER_RES(),
  })
  menu.root:center()

  menu.status_bar = widgets.StatusBar(menu.root, { transparent_bg = true })

  menu.list = lvgl.List(menu.root, {
    w = lvgl.PCT(100),
    h = lvgl.PCT(100),
    flex_grow = 1,
  })

  local now_playing = menu.list:add_btn(nil, "Now Playing")
  now_playing:onClicked(function()
    backstack.push(playing)
  end)
  now_playing:add_style(theme.list_item)

  local indexes = database.indexes()
  for _, idx in ipairs(indexes) do
    local btn = menu.list:add_btn(nil, tostring(idx))
    btn:onClicked(function()
      backstack.push(function()
        return browser {
          title = tostring(idx),
          iterator = idx:iter()
        }
      end)
    end)
    btn:add_style(theme.list_item)
  end

  local settings = menu.list:add_btn(nil, "Settings")
  settings:onClicked(function()
    legacy_ui.open_settings();
  end)
  settings:add_style(theme.list_item)

  return menu
end
