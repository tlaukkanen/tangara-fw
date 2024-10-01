-- Load fonts first, since they're parsed asynchronously and we can do much of
-- the other UI setup in parallel.
local font = require("font")

-- require() everything else needed for the main menu + global bindings. Do
-- this now instead of in init_ui because loading and parsing the scripts can
-- take a while.
local vol = require("volume")
local theme = require("theme")
local controls = require("controls")
local time = require("time")
local sd_card = require("sd_card")
local backstack = require("backstack")
local main_menu = require("main_menu")

local function init_ui()
  -- Theme is set within init_ui because the theme needs fonts to be ready.
  -- Set the theme to the saved theme if available
  local saved_theme = theme.theme_filename()
  local res = theme.load_theme(saved_theme)
  if not res then
    -- Set a default theme (in case the saved theme does not load)
    local default_theme = require("theme_light")
    theme.set(default_theme)
  end

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
        theme.set_subject(container, "pop_up")
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
        collectgarbage()
      elseif time.ticks() - lock_time > 8000 then
        local queue = require("queue")
        if queue.size:get() > 0 then
          require("playing"):push_if_not_shown()
        end
      end
    end),
    sd_card.mounted:bind(function(mounted)
      backstack.reset(main_menu:new())
    end),
  }
end

-- Wait for fonts to finish, then show the main menu.
-- We could show an intermediary Lua-controlled splash/loading UI whilst we
-- wait, but in practice loading the fonts takes only a few hundred ms longer
-- than all the other UI init.
font.on_loaded(function()
  init_ui()
end)
