local lvgl = require("lvgl")
local backstack = require("backstack")
local widgets = require("widgets")
local theme = require("theme")
local volume = require("volume")

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
      align_items = "flex-start",
      align_content = "flex-start",
    },
    w = lvgl.PCT(100),
    h = lvgl.SIZE_CONTENT,
  }
  enable_container:Label { text = "Enable", flex_grow = 1 }
  enable_container:Switch {}

  local preferred_container = menu.content:Object {
    flex = {
      flex_direction = "row",
      justify_content = "flex-start",
      align_items = "flex-start",
      align_content = "flex-start",
    },
    w = lvgl.PCT(100),
    h = lvgl.SIZE_CONTENT,
  }
  preferred_container:add_style(theme.settings_title)
  preferred_container:Label {
    text = "Preferred Device",
    flex_grow = 1,
  }
  preferred_container:Label { text = "x" }

  local preferred_device = menu.content:Label {
    text = "My Cool Speakers",
  }

  menu.content:Label {
    text = "Available Devices",
  }:add_style(theme.settings_title)

  local known_devices = menu.content:List {
    w = lvgl.PCT(100),
    flex_grow = 1,
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
    local selection = volume_chooser:get('selected')
    volume.limit_db.set(limits[selection])
  end)

  menu.content:Label {
    text = "Left/Right balance",
  }:add_style(theme.settings_title)

  local balance = menu.content:Slider {
    w = lvgl.PCT(100),
    h = 5,
    range = { min = -5, max = 5 },
    value = 0,
  }
  balance:onevent(lvgl.EVENT.VALUE_CHANGED, function()
    volume.left_bias:set(balance:value())
  end)

  local balance_label = menu.content:Label {}

  menu.bindings = {
    volume.limit_db:bind(function(limit)
      print("new limit", limit)
      for i=1,#limits do
        if limits[i] == limit then
          volume_chooser:set{ selected = i }
        end
      end
    end),
    volume.left_bias:bind(function(bias)
      balance:set {
        value = bias
      }
      if bias < 0 then
        balance_label:set {
          text = string.format("Left -%.2fdB", bias / 4)
        }
      else
        balance_label:set {
          text = string.format("Right -%.2fdB", -bias / 4)
        }
      end
    end),
  }

  return menu
end

function settings.display()
  local menu = SettingsScreen("Display")

  menu.content:Label {
    text = "Brightness",
  }:add_style(theme.settings_title)

  local brightness = menu.content:Slider {
    w = lvgl.PCT(100),
    h = 5,
    range = { min = 0, max = 100 },
    value = 50,
  }
  brightness:onevent(lvgl.EVENT.VALUE_CHANGED, function()
    print("bright", brightness:value())
  end)
end

function settings.input()
  local menu = SettingsScreen("Input Method")

  menu.content:Label {
    text = "Control scheme",
  }:add_style(theme.settings_title)

  local controls_chooser = menu.content:Dropdown {
    options = "Buttons only\nD-Pad\nTouchwheel",
    selected = 3,
  }
  controls_chooser:onevent(lvgl.EVENT.VALUE_CHANGED, function()
    local selection = controls_chooser:get('selected')
    print("controls", selection)
  end)
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

  local recreate = actions_container:Button {}
  recreate:Label { text = "Recreate" }
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
