local lvgl = require("lvgl")
local widgets = require("widgets")
local backstack = require("backstack")
local font = require("font")
local playback = require("playback")
local queue = require("queue")

local img = {
  play = "//lua/img/play.png",
  pause = "//lua/img/pause.png",
  next = "//lua/img/next.png",
  next_disabled = "//lua/img/next_disabled.png",
  prev = "//lua/img/prev.png",
  prev_disabled = "//lua/img/prev_disabled.png",
}

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
    w = lvgl.PCT(100),
    h = lvgl.SIZE_CONTENT,
    flex_grow = 1,
  }

  local artist = info:Label {
    w = lvgl.PCT(100),
    h = lvgl.SIZE_CONTENT,
    text = "",
    text_font = font.fusion_10,
    text_align = 2,
  }

  local title = info:Label {
    w = lvgl.PCT(100),
    h = lvgl.SIZE_CONTENT,
    text = "",
    text_align = 2,
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

  local prev_btn = controls:Button {}
  prev_btn:onClicked(queue.previous)
  local prev_img = prev_btn:Image { src = img.prev_disabled }

  local play_pause_btn = controls:Button {}
  play_pause_btn:onClicked(function()
    playback.playing:set(not playback.playing:get())
  end)
  local play_pause_img = play_pause_btn:Image { src = img.pause }

  local next_btn = controls:Button {}
  next_btn:onClicked(queue.next)
  local next_img = next_btn:Image { src = img.next_disabled }

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
        play_pause_img:set_src(img.pause)
      else
        play_pause_img:set_src(img.play)
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

      next_img:set_src(
        pos < queue.size:get() and img.next or img.next_disabled
      )
      prev_img:set_src(
        pos > 1 and img.prev or img.prev_disabled
      )
    end),
    queue.size:bind(function(num)
      if not num then return end
      playlist_total:set { text = tostring(num) }
    end),
  }

  return screen
end
