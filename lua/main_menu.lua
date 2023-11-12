local lvgl = require("lvgl")
local widgets = require("widgets")
local legacy_ui = require("legacy_ui")
local database = require("database")

local main_menu = {}

function main_menu:Create()
  local root = lvgl.Object(nil, {
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
  root:center()

  widgets.StatusBar(root, {})

  local list = lvgl.List(root, {
    w = lvgl.PCT(100),
    h = lvgl.PCT(100),
    flex_grow = 1,
  })

  list:add_btn(nil, "Now Playing"):onClicked(function()
    legacy_ui.open_now_playing();
  end)

  local indexes = database.get_indexes()
  for id, name in ipairs(indexes) do
    local btn = list:add_btn(nil, name)
    btn:onClicked(function()
      legacy_ui.open_browse(id);
    end)
  end

  list:add_btn(nil, "Settings"):onClicked(function()
    legacy_ui.open_settings();
  end)
end

return main_menu
