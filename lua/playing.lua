local lvgl = require("lvgl")
local widgets = require("widgets")
local backstack = require("backstack")
local font = require("font")

return function(opts)
  local screen = {}
  screen.root = lvgl.Object(nil, {
    flex = {
      flex_direction = "column",
      flex_wrap = "wrap",
      justify_content = "center",
      align_items = "center",
      align_content = "center",
    },
    w = lvgl.HOR_RES(),
    h = lvgl.VER_RES(),
  })
  screen.root:center()

  screen.status_bar = widgets.StatusBar(screen.root, {
    back_cb = backstack.pop,
    transparent_bg = true,
  })

  local track_info = screen.root:Object {
    flex = {
      flex_direction = "column",
      justify_content = "center",
      align_items = "center",
      align_content = "center",
    },
    w = lvgl.SIZE_CONTENT,
    flex_grow = 1,
  }

  local artist = track_info:Label {
    text = "Cool Artist",
    text_font = font.fusion_10,
  }

  local artist = track_info:Label {
    text = "Good Album",
    text_font = font.fusion_10,
  }

  local title = track_info:Label {
    text = "A really good song",
  }

  local scrubber = screen.root:Object {}

  local times = screen.root:Object {
    flex = {
      flex_direction = "row",
      justify_content = "center",
      align_items = "space-between",
      align_content = "center",
    },
    w = lvgl.PCT(100),
    h = lvgl.SIZE_CONTENT,
  }
  local cur_time = track_info:Label {
    text = "1:09",
  }
  local end_time = track_info:Label {
    text = "4:20",
  }


  local controls = screen.root:Object {
    flex = {
      flex_direction = "row",
      justify_content = "center",
      align_items = "space-evenly",
      align_content = "center",
    },
    w = lvgl.PCT(100),
    h = lvgl.SIZE_CONTENT,
  }
  controls:Label {
    text = ">",
  }
  controls:Label {
    text = ">",
  }
  controls:Label {
    text = ">",
  }
  controls:Label {
    text = ">",
  }

  return screen
end
