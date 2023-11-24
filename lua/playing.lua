local lvgl = require("lvgl")
local widgets = require("widgets")
local backstack = require("backstack")
local font = require("font")
local playback = require("playback")
local queue = require("queue")

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

  local info = screen.root:Object {
    flex = {
      flex_direction = "column",
      flex_wrap = "wrap",
      justify_content = "center",
      align_items = "center",
      align_content = "center",
    },
    w = lvgl.HOR_RES(),
    h = lvgl.SIZE_CONTENT,
    flex_grow = 1,
  }

  local artist = info:Label {
    w = lvgl.SIZE_CONTENT,
    h = lvgl.SIZE_CONTENT,
    text = "",
    text_font = font.fusion_10,
  }

  local title = info:Label {
    w = lvgl.SIZE_CONTENT,
    h = lvgl.SIZE_CONTENT,
    text = "",
  }

  local playlist = screen.root:Object {
    flex = {
      flex_direction = "row",
      justify_content = "center",
      align_items = "center",
      align_content = "center",
    },
    w = lvgl.SIZE_CONTENT,
    h = lvgl.SIZE_CONTENT,
  }

  local playlist_pos = playlist:Label {
    text = "",
    text_font = font.fusion_10,
  }
  playlist:Label {
    text = "/",
    text_font = font.fusion_10,
  }
  local playlist_total = playlist:Label {
    text = "",
    text_font = font.fusion_10,
  }

  local scrubber = screen.root:Bar {
    w = lvgl.PCT(100),
    h = 5,
    range = { min = 0, max = 100 },
    value = 0,
  }

  local controls = screen.root:Object {
    flex = {
      flex_direction = "row",
      justify_content = "center",
      align_items = "center",
      align_content = "center",
    },
    w = lvgl.PCT(100),
    h = lvgl.SIZE_CONTENT,
    pad_column = 8,
    pad_all = 2,
  }

  local cur_time = controls:Label {
    w = lvgl.SIZE_CONTENT,
    h = lvgl.SIZE_CONTENT,
    text = "",
    text_font = font.fusion_10,
  }

  controls:Object({ flex_grow = 1, h = 1 }) -- spacer

  controls:Image {
    src = "//lua/img/prev.png",
  }
  local play_pause_btn = controls:Button {}
  play_pause_btn:onClicked(function()
    playback.playing:set(not playback.playing:get())
  end)
  local play_pause_img = play_pause_btn:Image {
    src = "//lua/img/pause.png",
  }
  controls:Image {
    src = "//lua/img/next.png",
  }
  controls:Object({ flex_grow = 1, h = 1 }) -- spacer

  local end_time = controls:Label {
    w = lvgl.SIZE_CONTENT,
    h = lvgl.SIZE_CONTENT,
    align = lvgl.ALIGN.RIGHT_MID,
    text = "",
    text_font = font.fusion_10,
  }

  local format_time = function(time)
    return string.format("%d:%02d", time // 60, time % 60)
  end

  screen.bindings = {
    playback.playing:bind(function(playing)
      if playing then
        play_pause_img:set_src("//lua/img/pause.png")
      else
        play_pause_img:set_src("//lua/img/play.png")
      end
    end),
    playback.position:bind(function(pos)
      if not pos then return end
      cur_time:set {
        text = format_time(pos)
      }
      scrubber:set { value = pos }
    end),
    playback.track:bind(function(track)
      if not track then return end
      end_time:set {
        text = format_time(track.duration)
      }
      title:set { text = track.title }
      artist:set { text = track.artist }
      scrubber:set {
        range = { min = 0, max = track.duration }
      }
    end),
    queue.position:bind(function(pos)
      if not pos then return end
      playlist_pos:set { text = tostring(pos) }
    end),
    queue.size:bind(function(num)
      if not num then return end
      playlist_total:set { text = tostring(num) }
    end),
  }

  return screen
end
