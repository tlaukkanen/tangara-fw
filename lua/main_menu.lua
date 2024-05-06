local lvgl = require("lvgl")
local widgets = require("widgets")
local database = require("database")
local backstack = require("backstack")
local browser = require("browser")
local playing = require("playing")
local styles = require("styles")
local filesystem = require("filesystem")
local screen = require("screen")

return widgets.MenuScreen:new {
  createUi = function(self)
    widgets.MenuScreen.createUi(self)

    local list = lvgl.List(self.root, {
      w = lvgl.PCT(100),
      h = lvgl.PCT(100),
      flex_grow = 1,
    })

    local now_playing = list:add_btn(nil, "Now Playing")
    now_playing:onClicked(function()
      backstack.push(playing:new())
    end)
    now_playing:add_style(styles.list_item)

    local indexes = database.indexes()
    for _, idx in ipairs(indexes) do
      local btn = list:add_btn(nil, tostring(idx))
      btn:onClicked(function()
        backstack.push(browser:new {
          title = tostring(idx),
          iterator = idx:iter(),
        })
      end)
      btn:add_style(styles.list_item)
    end

    local files = list:add_btn(nil, "Files")
    files:onClicked(function()
      backstack.push(require("file_browser"):new {
        title = "Files",
        iterator = filesystem.iterator(""),
      })
    end)
    files:add_style(styles.list_item)

    local settings = list:add_btn(nil, "Settings")
    settings:onClicked(function()
      backstack.push(require("settings"):new())
    end)
    settings:add_style(styles.list_item)
  end,
}
