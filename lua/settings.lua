local lvgl = require("lvgl")
local backstack = require("backstack")
local widgets = require("widgets")
local theme = require("theme")
local volume = require("volume")
local display = require("display")
local controls = require("controls")
local bluetooth = require("bluetooth")
local database = require("database")

local settings = {}

local function SettingsScreen(title)
  local menu = widgets.MenuScreen {
    show_back = true,
    title = title,
  }
  menu.content = menu.root:Object {
    flex = {
      flex_direction = "column",
      flex_wrap = "nowrap",
      justify_content = "flex-start",
      align_items = "flex-start",
      align_content = "flex-start",
    },
    w = lvgl.PCT(100),
    flex_grow = 1,
    pad_left = 4,
    pad_right = 4,
  }
  return menu
end

function settings.bluetooth()
  local menu = SettingsScreen("Bluetooth")

  local enable_container = menu.content:Object {
    flex = {
      flex_direction = "row",
      justify_content = "flex-start",
      align_items = "content",
      align_content = "flex-start",
    },
    w = lvgl.PCT(100),
    h = lvgl.SIZE_CONTENT,
    pad_bottom = 1,
  }
  enable_container:Label { text = "Enable", flex_grow = 1 }
  local enable_sw = enable_container:Switch {}
  enable_sw:onevent(lvgl.EVENT.VALUE_CHANGED, function()
    local enabled = enable_sw:enabled()
    bluetooth.enabled:set(enabled)
  end)

  menu.content:Label {
    text = "Paired Device",
    pad_bottom = 1,
  }:add_style(theme.settings_title)

  local paired_container = menu.content:Object {
    flex = {
      flex_direction = "row",
      justify_content = "flex-start",
      align_items = "flex-start",
      align_content = "flex-start",
    },
    w = lvgl.PCT(100),
    h = lvgl.SIZE_CONTENT,
    pad_bottom = 2,
  }

  local paired_device = paired_container:Label {
    flex_grow = 1,
  }
  local clear_paired = paired_container:Button {}
  clear_paired:Label { text = "x" }
  clear_paired:onClicked(function()
    bluetooth.paired_device:set()
  end)

  menu.content:Label {
    text = "Nearby Devices",
    pad_bottom = 1,
  }:add_style(theme.settings_title)

  local devices = menu.content:List {
    w = lvgl.PCT(100),
    h = lvgl.SIZE_CONTENT,
  }

  menu.bindings = {
    bluetooth.enabled:bind(function(en)
      enable_sw:set { enabled = en }
    end),
    bluetooth.paired_device:bind(function(device)
      if device then
        paired_device:set { text = device.name }
        clear_paired:clear_flag(lvgl.FLAG.HIDDEN)
      else
        paired_device:set { text = "None" }
        clear_paired:add_flag(lvgl.FLAG.HIDDEN)
      end
    end),
    bluetooth.devices:bind(function(devs)
      devices:clean()
      for _, dev in pairs(devs) do
        devices:add_btn(nil, dev.name):onClicked(function()
          bluetooth.paired_device:set(dev)
        end)
      end
    end)
  }
end

function settings.headphones()
  local menu = SettingsScreen("Headphones")

  menu.content:Label {
    text = "Maximum volume limit",
  }:add_style(theme.settings_title)

  local volume_chooser = menu.content:Dropdown {
    options = "Line Level (-10 dB)\nCD Level (+6 dB)\nMaximum (+10dB)",
    selected = 1,
  }
  local limits = { -10, 6, 10 }
  volume_chooser:onevent(lvgl.EVENT.VALUE_CHANGED, function()
    -- luavgl dropdown binding uses 0-based indexing :(
    local selection = volume_chooser:get('selected') + 1
    volume.limit_db:set(limits[selection])
  end)

  menu.content:Label {
    text = "Left/Right balance",
  }:add_style(theme.settings_title)

  local balance = menu.content:Slider {
    w = lvgl.PCT(100),
    h = 5,
    range = { min = -100, max = 100 },
    value = 0,
  }
  balance:onevent(lvgl.EVENT.VALUE_CHANGED, function()
    volume.left_bias:set(balance:value())
  end)

  local balance_label = menu.content:Label {}

  menu.bindings = {
    volume.limit_db:bind(function(limit)
      for i = 1, #limits do
        if limits[i] == limit then
          volume_chooser:set { selected = i - 1 }
        end
      end
    end),
    volume.left_bias:bind(function(bias)
      balance:set {
        value = bias
      }
      if bias < 0 then
        balance_label:set {
          text = string.format("Left %.2fdB", bias / 4)
        }
      elseif bias > 0 then
        balance_label:set {
          text = string.format("Right %.2fdB", -bias / 4)
        }
      else
        balance_label:set { text = "Balanced" }
      end
    end),
  }

  return menu
end

function settings.display()
  local menu = SettingsScreen("Display")

  local brightness_title = menu.content:Object {
    flex = {
      flex_direction = "row",
      justify_content = "flex-start",
      align_items = "flex-start",
      align_content = "flex-start",
    },
    w = lvgl.PCT(100),
    h = lvgl.SIZE_CONTENT,
  }
  brightness_title:Label { text = "Brightness", flex_grow = 1 }
  local brightness_pct = brightness_title:Label {}
  brightness_pct:add_style(theme.settings_title)

  local brightness = menu.content:Slider {
    w = lvgl.PCT(100),
    h = 5,
    range = { min = 0, max = 100 },
    value = display.brightness:get(),
  }
  brightness:onevent(lvgl.EVENT.VALUE_CHANGED, function()
    display.brightness:set(brightness:value())
  end)

  menu.bindings = {
    display.brightness:bind(function(b)
      brightness_pct:set { text = tostring(b) .. "%" }
    end)
  }

  return menu
end

function settings.input()
  local menu = SettingsScreen("Input Method")

  menu.content:Label {
    text = "Control scheme",
  }:add_style(theme.settings_title)

  local schemes = controls.schemes()
  local option_to_scheme = {}
  local scheme_to_option = {}

  local option_idx = 0
  local options = ""

  for i, v in pairs(schemes) do
    option_to_scheme[option_idx] = i
    scheme_to_option[i] = option_idx
    if option_idx > 0 then
      options = options .. "\n"
    end
    options = options .. v
    option_idx = option_idx + 1
  end

  local controls_chooser = menu.content:Dropdown {
    options = options,
  }

  menu.bindings = {
    controls.scheme:bind(function(s)
      local option = scheme_to_option[s]
      controls_chooser:set({ selected = option })
    end)
  }

  controls_chooser:onevent(lvgl.EVENT.VALUE_CHANGED, function()
    local option = controls_chooser:get('selected')
    local scheme = option_to_scheme[option]
    controls.scheme:set(scheme)
  end)

  return menu
end

function settings.database()
  local menu = SettingsScreen("Database")
  local db = require("database")
  widgets.Row(menu.content, "Schema version", db.version())
  widgets.Row(menu.content, "Size on disk", string.format("%.1f KiB", db.size() / 1024))

  local actions_container = menu.content:Object {
    w = lvgl.PCT(100),
    h = lvgl.SIZE_CONTENT,
    flex = {
      flex_direction = "row",
      justify_content = "center",
      align_items = "space-evenly",
      align_content = "center",
    },
    pad_top = 4,
    pad_column = 4,
  }
  actions_container:add_style(theme.list_item)

  local update = actions_container:Button {}
  update:Label { text = "Update" }
  update:onClicked(function()
    database.update()
  end)
end

function settings.firmware()
  local menu = SettingsScreen("Firmware")
  local version = require("version")
  widgets.Row(menu.content, "ESP32", version.esp())
  widgets.Row(menu.content, "SAMD21", version.samd())
  widgets.Row(menu.content, "Collator", version.collator())
end

function settings.root()
  local menu = widgets.MenuScreen {
    show_back = true,
    title = "Settings",
  }
  menu.list = menu.root:List {
    w = lvgl.PCT(100),
    h = lvgl.PCT(100),
    flex_grow = 1,
  }

  local function section(name)
    menu.list:add_text(name):add_style(theme.list_heading)
  end

  local function submenu(name, fn)
    local item = menu.list:add_btn(nil, name)
    item:onClicked(function()
      backstack.push(fn)
    end)
    item:add_style(theme.list_item)
  end

  section("Audio")
  submenu("Bluetooth", settings.bluetooth)
  submenu("Headphones", settings.headphones)

  section("Interface")
  submenu("Display", settings.display)
  submenu("Input Method", settings.input)

  section("System")
  submenu("Database", settings.database)
  submenu("Firmware", settings.firmware)
  submenu("Licenses", function()
    return require("licenses")()
  end)

  return menu
end

return settings
