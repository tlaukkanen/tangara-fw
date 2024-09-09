local lvgl = require("lvgl")
local widgets = require("widgets")
local database = require("database")
local backstack = require("backstack")
local browser = require("browser")
local playing = require("playing")
local styles = require("styles")
local filesystem = require("filesystem")
local screen = require("screen")
local font = require("font")
local theme = require("theme")
local img = require("images")
local playback = require("playback")

return widgets.MenuScreen:new {
  create_ui = function(self)
    widgets.MenuScreen.create_ui(self)

    -- At the top, a card showing details about the current track. Hidden if
    -- there is no track currently playing.
    -- || Cool Song 0:01

    local now_playing = lvgl.Button(self.root, {
      flex = {
        flex_direction = "row",
        flex_wrap = "nowrap",
        justify_content = "flex-start",
        align_items = "center",
        align_content = "flex-start",
      },
      w = lvgl.PCT(100),
      h = lvgl.SIZE_CONTENT,
      margin_all = 2,
      pad_bottom = 2,
      pad_column = 4,
      border_width = 1,
    })
    theme.set_style(now_playing, "now_playing");

    local play_pause = now_playing:Image { src = img.play_small }
    local title = now_playing:Label {
      flex_grow = 1,
      h = lvgl.SIZE_CONTENT,
      text = " ",
      long_mode = 1,
    }
    local time_remaining = now_playing:Label {
      text = " ",
      text_font = font.fusion_10,
    }

    now_playing:onClicked(function() backstack.push(playing:new()) end)

    local has_focus = false
    local track_duration = nil

    self.bindings = self.bindings + {
      playback.playing:bind(function(playing)
        if playing then
          play_pause:set_src(img.play_small)
        else
          play_pause:set_src(img.pause_small)
        end
      end),
      playback.track:bind(function(track)
        if not track then
          now_playing:add_flag(lvgl.FLAG.HIDDEN)
          return
        else
          has_focus = true
          now_playing:clear_flag(lvgl.FLAG.HIDDEN)
        end
        title:set { text = track.title }
        if track.duration then
          track_duration = track.duration
        end
      end),
      playback.position:bind(function(pos)
        if not pos then return end
        if not track_duration then return end
        local remaining = track_duration - pos
        if remaining < 0 then remaining = 0 end
        time_remaining:set {
          text = string.format("%d:%02d", remaining // 60, remaining % 60)
        }
      end),
    }

    -- Next, a list showing the user's prefer's music source. This defaults to
    -- a list of all available database indexes, but could also be the contents
    -- of the SD card root.

    if require("sd_card").mounted:get() then
      local list = lvgl.List(self.root, {
        w = lvgl.PCT(100),
        h = lvgl.PCT(100),
        flex_grow = 1,
      })

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
        if not has_focus then
          has_focus = true
          btn:focus()
        end
      end
    else
      local container = self.root:Object {
        w = lvgl.PCT(100),
        flex_grow = 1,
      }
      container:Label {
        w = lvgl.PCT(100),
        h = lvgl.SIZE_CONTENT,
        text_align = 2,
        long_mode = 0,
        margin_all = 4,
        text = "SD Card is not inserted or could not be opened.",
      }:center();
    end

    -- Finally, the bottom bar with icon buttons for other device features.

    local bottom_bar = lvgl.Object(self.root, {
      flex = {
        flex_direction = "row",
        flex_wrap = "nowrap",
        justify_content = "space-evenly",
        align_items = "center",
        align_content = "flex-start",
      },
      w = lvgl.PCT(100),
      h = lvgl.SIZE_CONTENT,
      pad_top = 4,
      pad_bottom = 2,
    })

    -- local queue_btn = bottom_bar:Button {}
    -- queue_btn:Image { src = img.queue }
    -- theme.set_style(queue_btn, "icon_enabled")

    local files_btn = bottom_bar:Button {}
    files_btn:onClicked(function()
      backstack.push(require("file_browser"):new {
        title = "Files",
        iterator = filesystem.iterator(""),
      })
    end)
    files_btn:Image { src = img.files }
    theme.set_style(files_btn, "menu_icon")

    local settings_btn = bottom_bar:Button {}
    settings_btn:onClicked(function()
      backstack.push(require("settings"):new())
    end)
    settings_btn:Image { src = img.settings }
    theme.set_style(settings_btn, "menu_icon")
  end,
}
