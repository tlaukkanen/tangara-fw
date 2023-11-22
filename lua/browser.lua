local lvgl = require("lvgl")
local widgets = require("widgets")
local legacy_ui = require("legacy_ui")
local database = require("database")
local backstack = require("backstack")

local browser = {}

function browser.create(opts)
  local screen = {}
  screen.root = lvgl.Object(nil, {
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
  screen.root:center()

  screen.status_bar = widgets.StatusBar(screen.root, {
    back_cb = backstack.pop,
    title = opts.title,
  })

  if opts.breadcrumb then
    local header = screen.root:Object {
      flex = {
        flex_direction = "column",
        flex_wrap = "wrap",
        justify_content = "flex-start",
        align_items = "flex-start",
        align_content = "flex-start",
      },
      w = lvgl.HOR_RES(),
      h = lvgl.SIZE_CONTENT,
      pad_left = 4,
      pad_right = 4,
      pad_top = 2,
      pad_bottom = 2,
      bg_opa = lvgl.OPA(100),
      bg_color = "#f3e5f5",
      scrollbar_mode = lvgl.SCROLLBAR_MODE.OFF,
    }

    header:Label { text = opts.breadcrumb }

    local buttons = header:Object({
      flex = {
        flex_direction = "row",
        flex_wrap = "wrap",
        justify_content = "flex-end",
        align_items = "center",
        align_content = "center",
      },
      w = lvgl.PCT(100),
      h = lvgl.SIZE_CONTENT,
      pad_column = 4,
    })
    local enqueue = buttons:Button {}
    enqueue:Label { text = "Enqueue" }
    enqueue:add_flag(lvgl.FLAG.HIDDEN)
    local play = buttons:Button {}
    play:Label { text = "Play all" }
  end

  screen.list = lvgl.List(screen.root, {
    w = lvgl.PCT(100),
    h = lvgl.PCT(100),
    flex_grow = 1,
    scrollbar_mode = lvgl.SCROLLBAR_MODE.OFF,
  })

  screen.focused_item = 0
  screen.last_item = 0
  screen.add_item = function(item)
    if not item then return end
    screen.last_item = screen.last_item + 1
    local this_item = screen.last_item
    local btn = screen.list:add_btn(nil, tostring(item))
    btn:onClicked(function()
      local contents = item:contents()
      if type(contents) == "function" then
        backstack.push(function()
          return browser.create({
            title = opts.title,
            iterator = contents,
            breadcrumb = tostring(item),
          })
        end)
      else
        print("selected track", contents)
      end
    end)
    btn:onevent(lvgl.EVENT.FOCUSED, function()
      screen.focused_item = this_item
      if screen.last_item - 5 < this_item then
        opts.iterator(screen.add_item)
      end
    end)
  end

  for _ = 1, 8 do
    opts.iterator(screen.add_item)
  end

  return screen
end

return browser.create
