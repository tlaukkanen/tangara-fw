local lvgl = require("lvgl")
local widgets = require("widgets")
local backstack = require("backstack")
local font = require("font")
local playback = require("playback")
local queue = require("queue")
local screen = require("screen")
local theme = require("theme")

local img = require("images")

local format_time = function(time)
  time = math.floor(time)
  return string.format("%d:%02d", time // 60, time % 60)
end

local is_now_playing_shown = false

local icon_enabled_class = "icon_enabled"
local icon_disabled_class = "icon_disabled"

return screen:new {
  create_ui = function(self)
    self.root = lvgl.Object(nil, {
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
    self.root:center()

    self.status_bar = widgets.StatusBar(self, {
      back_cb = backstack.pop,
      transparent_bg = true,
    })

    local info = self.root:Object {
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

    local playlist = self.root:Object {
      flex = {
        flex_direction = "row",
        justify_content = "center",
        align_items = "center",
        align_content = "center",
      },
      w = lvgl.PCT(95),
      h = lvgl.SIZE_CONTENT,
    }

    playlist:Object({ w = 3, h = 1 }) -- spacer

    local cur_time = playlist:Label {
      w = lvgl.SIZE_CONTENT,
      h = lvgl.SIZE_CONTENT,
      text = "",
      text_font = font.fusion_10,
    }

    playlist:Object({ flex_grow = 1, h = 1 }) -- spacer

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

    playlist:Object({ flex_grow = 1, h = 1 }) -- spacer

    local end_time = playlist:Label {
      w = lvgl.SIZE_CONTENT,
      h = lvgl.SIZE_CONTENT,
      align = lvgl.ALIGN.RIGHT_MID,
      text = format_time(0),
      text_font = font.fusion_10,
    }
    playlist:Object({ w = 3, h = 1 }) -- spacer

    local scrubber = self.root:Slider {
      w = lvgl.PCT(90),
      h = 5,
      range = { min = 0, max = 100 },
      value = 0,
    }
    theme.set_style(scrubber, "scrubber");
    local scrubber_desc = widgets.Description(scrubber, "Scrubber")

    scrubber:onevent(lvgl.EVENT.RELEASED, function()
      local track = playback.track:get()
      if not track then return end
      if not track.duration then return end
      playback.position:set(scrubber:value() / 100 * track.duration)
    end)
    scrubber:onevent(lvgl.EVENT.VALUE_CHANGED, function()
      if scrubber:is_dragged() then
        local track = playback.track:get()
        if not track then return end
        if not track.duration then return end
        cur_time:set {
          text = format_time(scrubber:value() / 100 * track.duration)
        }
      end
    end)

    self.root:Object({ w = 1, h = 1 }) -- spacer

    local controls = self.root:Object {
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

    controls:Object({ flex_grow = 1, h = 1 }) -- spacer

    local repeat_btn = controls:Button {}
    repeat_btn:onClicked(function()
      queue.repeat_track:set(not queue.repeat_track:get())
    end)
    local repeat_img = repeat_btn:Image { src = img.repeat_src }
    theme.set_style(repeat_btn, icon_enabled_class)
    local repeat_desc = widgets.Description(repeat_btn)


    local prev_btn = controls:Button {}
    prev_btn:onClicked(function()
      if playback.position:get() > 3 then
        playback.position:set(0)
      else
        queue.previous()
      end
    end)
    local prev_img = prev_btn:Image { src = img.prev }
    theme.set_style(prev_btn, icon_enabled_class)
    local prev_desc = widgets.Description(prev_btn, "Previous track")

    local play_pause_btn = controls:Button {}
    play_pause_btn:onClicked(function()
      playback.playing:set(not playback.playing:get())
    end)
    play_pause_btn:focus()
    local play_pause_img = play_pause_btn:Image { src = img.pause }
    theme.set_style(play_pause_btn, icon_enabled_class)
    local play_pause_desc = widgets.Description(play_pause_btn, "Play")

    local next_btn = controls:Button {}
    next_btn:onClicked(queue.next)
    local next_img = next_btn:Image { src = img.next }
    theme.set_style(next_btn, icon_disabled_class)
    local next_desc = widgets.Description(next_btn, "Next track")

    local shuffle_btn = controls:Button {}
    shuffle_btn:onClicked(function()
      queue.random:set(not queue.random:get())
    end)
    local shuffle_img = shuffle_btn:Image { src = img.shuffle }
    theme.set_style(shuffle_btn, icon_enabled_class)
    local shuffle_desc = widgets.Description(shuffle_btn)

    controls:Object({ flex_grow = 1, h = 1 }) -- spacer


    self.bindings = self.bindings + {
      playback.playing:bind(function(playing)
        if playing then
          play_pause_img:set_src(img.pause)
          play_pause_desc:set { text = "Pause" }
        else
          play_pause_img:set_src(img.play)
          play_pause_desc:set { text = "Play" }
        end
      end),
      playback.position:bind(function(pos)
        if not pos then return end
        if not scrubber:is_dragged() then
          cur_time:set {
            text = format_time(pos)
          }
          local track = playback.track:get()
          if not track then return end
          if not track.duration then return end
          scrubber:set { value = pos / track.duration * 100 }
        end
      end),
      playback.track:bind(function(track)
        if not track then
          if queue.loading:get() then
            title:set { text = "Loading..." }
          else
            title:set { text = "" }
          end
          artist:set { text = "" }
          cur_time:set { text = format_time(0) }
          end_time:set { text = format_time(0) }
          scrubber:set { value = 0 }
          return
        end
        if track.duration then
          end_time:set { text = format_time(track.duration) }
        else
          end_time:set { text = format_time(playback.position:get()) }
        end
        title:set { text = track.title }
        artist:set { text = track.artist }
      end),
      queue.position:bind(function(pos)
        if not pos then return end
        playlist_pos:set { text = tostring(pos) }

        local can_next = pos < queue.size:get() or queue.random:get()
        theme.set_style(
          next_btn, can_next and icon_enabled_class or icon_disabled_class
        )
      end),
      queue.random:bind(function(shuffling)
        theme.set_style(shuffle_btn, shuffling and icon_enabled_class or icon_disabled_class)
        if shuffling then
          shuffle_img:set_src(img.shuffle)
          shuffle_desc:set { text = "Disable shuffle" }
        else
          shuffle_img:set_src(img.shuffle_off)
          shuffle_desc:set { text = "Enable shuffle" }
        end
      end),
      queue.repeat_track:bind(function(en)
        theme.set_style(repeat_btn, en and icon_enabled_class or icon_disabled_class)
        if en then
          repeat_img:set_src(img.repeat_src)
          repeat_desc:set { text = "Disable track repeat" }
        else
          repeat_img:set_src(img.repeat_off)
          repeat_desc:set { text = "Enable track repeat" }
        end
      end),
      queue.size:bind(function(num)
        if not num then return end
        playlist_total:set { text = tostring(num) }
      end),
    }
  end,
  on_show = function() is_now_playing_shown = true end,
  on_hide = function() is_now_playing_shown = false end,
  push_if_not_shown = function(self)
    if not is_now_playing_shown then
      backstack.push(self:new())
    end
  end
}
