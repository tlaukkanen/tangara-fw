local font = require("font")
local vol = require("volume")
local theme = require("theme")
local controls = require("controls")
local time = require("time")
local sd_card = require("sd_card")
local backstack = require("backstack")
local main_menu = require("main_menu")

local theme_dark = require("theme_dark")
theme.set(theme_dark)

local lock_time = time.ticks()

-- Set up property bindings that are used across every screen.
GLOBAL_BINDINGS = {
  -- Show an alert with the current volume whenever the volume changes
  vol.current_pct:bind(function(pct)
    require("alerts").show(function()
      local container = lvgl.Object(nil, {
        w = lvgl.PCT(80),
        h = lvgl.SIZE_CONTENT,
        flex = {
          flex_direction = "column",
          justify_content = "center",
          align_items = "center",
          align_content = "center",
        },
        radius = 8,
        pad_all = 2,
      })
      theme.set_style(container, "pop_up")
      container:Label {
        text = string.format("Volume %i%%", pct),
        text_font = font.fusion_10
      }
      container:Bar {
        w = lvgl.PCT(100),
        h = 8,
        range = { min = 0, max = 100 },
        value = pct,
      }
      container:center()
    end)
  end),
  -- When the device has been locked for a while, default to showing the now
  -- playing screen after unlocking.
  controls.lock_switch:bind(function(locked)
    if locked then
      lock_time = time.ticks()
    elseif time.ticks() - lock_time > 8000 then
      local queue = require("queue")
      if queue.size:get() > 0 then
        require("playing"):pushIfNotShown()
      end
    end
  end),
  sd_card.mounted:bind(function(mounted)
    backstack.reset(main_menu:new())
  end),
}
