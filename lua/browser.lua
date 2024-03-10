local lvgl = require("lvgl")
local widgets = require("widgets")
local backstack = require("backstack")
local font = require("font")
local queue = require("queue")
local playing = require("playing")
local styles = require("styles")
local playback = require("playback")
local theme = require("theme")

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
      pad_bottom = 2,
      bg_opa = lvgl.OPA(100),
      scrollbar_mode = lvgl.SCROLLBAR_MODE.OFF,
    }
    theme.set_style(header, "header")


    header:Label {
      text = opts.breadcrumb,
      text_font = font.fusion_10,
    }

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
    local original_iterator = opts.iterator:clone()
    local enqueue = widgets.IconBtn(buttons, "//lua/img/enqueue.png", "Enqueue")
    enqueue:onClicked(function()
      queue.add(original_iterator)
      playback.playing:set(true)
    end)
    -- enqueue:add_flag(lvgl.FLAG.HIDDEN)
    local play = widgets.IconBtn(buttons, "//lua/img/play_small.png", "Play")
    play:onClicked(function()
      queue.clear()
      queue.add(original_iterator)
      playback.playing:set(true)
      backstack.push(playing)
    end
    )
  end

  screen.list = lvgl.List(screen.root, {
    w = lvgl.PCT(100),
    h = lvgl.PCT(100),
    flex_grow = 1,
    scrollbar_mode = lvgl.SCROLLBAR_MODE.OFF,
  })

  local back = screen.list:add_btn(nil, "< Back")
  back:onClicked(backstack.pop)
  back:add_style(styles.list_item)

  screen.focused_item = 0
  screen.last_item = 0
  screen.add_item = function(item)
    if not item then return end
    screen.last_item = screen.last_item + 1
    local this_item = screen.last_item
    local btn = screen.list:add_btn(nil, tostring(item))
    btn:onClicked(function()
      local contents = item:contents()
      if type(contents) == "userdata" then
        backstack.push(function()
          return browser.create({
            title = opts.title,
            iterator = contents,
            breadcrumb = tostring(item),
          })
        end)
      else
        queue.clear()
        queue.add(contents)
        playback.playing:set(true)
        backstack.push(playing)
      end
    end)
    btn:onevent(lvgl.EVENT.FOCUSED, function()
      screen.focused_item = this_item
      if screen.last_item - 5 < this_item then
        screen.add_item(opts.iterator())
      end
    end)
    btn:add_style(styles.list_item)
  end

  for _ = 1, 8 do
    local val = opts.iterator()
    if not val then break end
    screen.add_item(val)
  end

  return screen
end

return browser.create
