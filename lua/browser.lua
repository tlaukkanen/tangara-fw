local lvgl = require("lvgl")
local widgets = require("widgets")
local backstack = require("backstack")
local font = require("font")
local queue = require("queue")
local playing = require("playing")
local theme = require("theme")
local playback = require("playback")
local screen = require("screen")

return screen:new {
  createUi = function(self)
    self.root = lvgl.Object(nil, {
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
    self.root:center()

    self.status_bar = widgets.StatusBar(self.root, {
      title = self.title,
    })

    if self.breadcrumb then
      local header = self.root:Object {
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
        bg_color = "#fafafa",
        scrollbar_mode = lvgl.SCROLLBAR_MODE.OFF,
      }

      header:Label {
        text = self.breadcrumb,
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
      local original_iterator = self.iterator:clone()
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

    self.list = lvgl.List(self.root, {
      w = lvgl.PCT(100),
      h = lvgl.PCT(100),
      flex_grow = 1,
      scrollbar_mode = lvgl.SCROLLBAR_MODE.OFF,
    })

    local back = self.list:add_btn(nil, "< Back")
    back:onClicked(backstack.pop)
    back:add_style(theme.list_item)

    self.focused_item = 0
    self.last_item = 0
    self.add_item = function(item)
      if not item then return end
      self.last_item = self.last_item + 1
      local this_item = self.last_item
      local btn = self.list:add_btn(nil, tostring(item))
      btn:onClicked(function()
        local contents = item:contents()
        if type(contents) == "userdata" then
          backstack.push(require("browser"):new {
            title = self.title,
            iterator = contents,
            breadcrumb = tostring(item),
          })
        else
          queue.clear()
          queue.add(contents)
          playback.playing:set(true)
          backstack.push(playing:new())
        end
      end)
      btn:onevent(lvgl.EVENT.FOCUSED, function()
        self.focused_item = this_item
        if self.last_item - 5 < this_item then
          self.add_item(self.iterator())
        end
      end)
      btn:add_style(theme.list_item)
    end

    for _ = 1, 8 do
      local val = self.iterator()
      if not val then break end
      self.add_item(val)
    end
  end
}
