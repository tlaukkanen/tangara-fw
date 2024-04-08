local lvgl = require("lvgl")
local backstack = require("backstack")
local widgets = require("widgets")
local styles = require("styles")
local volume = require("volume")
local display = require("display")
local controls = require("controls")
local bluetooth = require("bluetooth")
local theme = require("theme")
local database = require("database")
local screen = require("screen")
local usb = require("usb")

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

local BluetoothSettings = screen:new {
  createUi = function(self)
    self.menu = SettingsScreen("Bluetooth")

    local enable_container = self.menu.content:Object {
      flex = {
        flex_direction = "row",
        justify_content = "flex-start",
        align_items = "center",
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

    theme.set_style(self.menu.content:Label {
      text = "Paired Device",
      pad_bottom = 1,
    }, "settings_title")

    local paired_container = self.menu.content:Object {
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

    theme.set_style(self.menu.content:Label {
      text = "Nearby Devices",
      pad_bottom = 1,
    }, "settings_title")

    local devices = self.menu.content:List {
      w = lvgl.PCT(100),
      h = lvgl.SIZE_CONTENT,
    }

    self.bindings = {
      bluetooth.enabled:bind(function(en)
        if en then
          enable_sw:add_state(lvgl.STATE.CHECKED)
        else
          enable_sw:clear_state(lvgl.STATE.CHECKED)
        end
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
}

local HeadphonesSettings = screen:new {
  createUi = function(self)
    self.menu = SettingsScreen("Headphones")

    theme.set_style(self.menu.content:Label {
      text = "Maxiumum volume limit",
    }, "settings_title")

    local volume_chooser = self.menu.content:Dropdown {
      options = "Line Level (-10 dB)\nCD Level (+6 dB)\nMaximum (+10dB)",
      selected = 1,
    }
    local limits = { -10, 6, 10 }
    volume_chooser:onevent(lvgl.EVENT.VALUE_CHANGED, function()
      -- luavgl dropdown binding uses 0-based indexing :(
      local selection = volume_chooser:get('selected') + 1
      volume.limit_db:set(limits[selection])
    end)

    theme.set_style(self.menu.content:Label {
      text = "Left/Right balance",
    }, "settings_title")

    local balance = self.menu.content:Slider {
      w = lvgl.PCT(100),
      h = 5,
      range = { min = -100, max = 100 },
      value = 0,
    }
    balance:onevent(lvgl.EVENT.VALUE_CHANGED, function()
      volume.left_bias:set(balance:value())
    end)

    local balance_label = self.menu.content:Label {}

    self.bindings = {
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
  end
}

local DisplaySettings = screen:new {
  createUi = function(self)
    self.menu = SettingsScreen("Display")

    local brightness_title = self.menu.content:Object {
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
    theme.set_style(brightness_pct, "settings_title")

    local brightness = self.menu.content:Slider {
      w = lvgl.PCT(100),
      h = 5,
      range = { min = 0, max = 100 },
      value = display.brightness:get(),
    }
    brightness:onevent(lvgl.EVENT.VALUE_CHANGED, function()
      display.brightness:set(brightness:value())
    end)

    self.bindings = {
      display.brightness:bind(function(b)
        brightness_pct:set { text = tostring(b) .. "%" }
      end)
    }
  end
}

local InputSettings = screen:new {
  createUi = function(self)
    self.menu = SettingsScreen("Input Method")

    theme.set_style(self.menu.content:Label {
      text = "Control scheme",
    }, "settings_title")

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

    local controls_chooser = self.menu.content:Dropdown {
      options = options,
    }

    self.bindings = {
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

    theme.set_style(self.menu.content:Label {
      text = "Scroll Sensitivity",
    }, "settings_title")

    local slider_scale = 4; -- Power steering
    local sensitivity = self.menu.content:Slider {
      w = lvgl.PCT(90),
      h = 5,
      range = { min = 0, max = 255 / slider_scale },
      value = controls.scroll_sensitivity:get() / slider_scale,
    }
    sensitivity:onevent(lvgl.EVENT.VALUE_CHANGED, function()
      controls.scroll_sensitivity:set(sensitivity:value() * slider_scale)
    end)
  end
}

local MassStorageSettings = screen:new {
  createUi = function(self)
    self.menu = SettingsScreen("USB Storage")
    local version = require("version").samd()
    if tonumber(version) < 3 then
      self.menu.content:Label {
        w = lvgl.PCT(100),
        text = "Usb Mass Storage requires a SAMD21 firmware version >=3."
      }
      return
    end

    local enable_container = self.menu.content:Object {
      flex = {
        flex_direction = "row",
        justify_content = "flex-start",
        align_items = "center",
        align_content = "flex-start",
      },
      w = lvgl.PCT(100),
      h = lvgl.SIZE_CONTENT,
      pad_bottom = 1,
    }
    enable_container:Label { text = "Enable", flex_grow = 1 }
    local enable_sw = enable_container:Switch {}

    local busy_text = self.menu.content:Label {
      w = lvgl.PCT(100),
      text = "USB is currently busy. Do not unplug or remove the SD card.",
      long_mode = lvgl.LABEL.LONG_WRAP,
    }

    local bind_switch = function()
      if usb.msc_enabled:get() then
        enable_sw:add_state(lvgl.STATE.CHECKED)
      else
        enable_sw:clear_state(lvgl.STATE.CHECKED)
      end
    end

    enable_sw:onevent(lvgl.EVENT.VALUE_CHANGED, function()
      if not usb.msc_busy:get() then
        usb.msc_enabled:set(enable_sw:enabled())
      end
      bind_switch()
    end)

    self.bindings = {
      usb.msc_enabled:bind(bind_switch),
      usb.msc_busy:bind(function(busy)
        if busy then
          busy_text:clear_flag(lvgl.FLAG.HIDDEN)
        else
          busy_text:add_flag(lvgl.FLAG.HIDDEN)
        end
      end)
    }
  end,
  canPop = function()
    return not usb.msc_enabled:get()
  end
}

local DatabaseSettings = screen:new {
  createUi = function(self)
    self.menu = SettingsScreen("Database")
    local db = require("database")
    widgets.Row(self.menu.content, "Schema version", db.version())
    widgets.Row(self.menu.content, "Size on disk", string.format("%.1f KiB", db.size() / 1024))

    local auto_update_container = self.menu.content:Object {
      flex = {
        flex_direction = "row",
        justify_content = "flex-start",
        align_items = "flex-start",
        align_content = "flex-start",
      },
      w = lvgl.PCT(100),
      h = lvgl.SIZE_CONTENT,
    }
    auto_update_container:add_style(styles.list_item)
    auto_update_container:Label { text = "Auto update", flex_grow = 1 }
    local auto_update_sw = auto_update_container:Switch {}

    auto_update_sw:onevent(lvgl.EVENT.VALUE_CHANGED, function()
      database.auto_update:set(auto_update_sw:enabled())
    end)

    local actions_container = self.menu.content:Object {
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
    actions_container:add_style(styles.list_item)

    local update = actions_container:Button {}
    update:Label { text = "Update now" }
    update:onClicked(function()
      database.update()
    end)

    self.bindings = {
      database.auto_update:bind(function(en)
        if en then
          auto_update_sw:add_state(lvgl.STATE.CHECKED)
        else
          auto_update_sw:clear_state(lvgl.STATE.CHECKED)
        end
      end),
    }
  end
}

local FirmwareSettings = screen:new {
  createUi = function(self)
    self.menu = SettingsScreen("Firmware")
    local version = require("version")
    widgets.Row(self.menu.content, "ESP32", version.esp())
    widgets.Row(self.menu.content, "SAMD21", version.samd())
    widgets.Row(self.menu.content, "Collator", version.collator())
  end
}

local LicensesScreen = screen:new {
  createUi = function(self)
    self.root = require("licenses")()
  end
}

return screen:new {
  createUi = function(self)
    self.menu = widgets.MenuScreen {
      show_back = true,
      title = "Settings",
    }
    self.list = self.menu.root:List {
      w = lvgl.PCT(100),
      h = lvgl.PCT(100),
      flex_grow = 1,
    }

    local function section(name)
      local elem = self.list:Label {
        text = name,
        pad_left = 4,
      }
      theme.set_style(elem, "settings_title")
    end

    local function submenu(name, class)
      local item = self.list:add_btn(nil, name)
      item:onClicked(function()
        backstack.push(class:new())
      end)
      item:add_style(styles.list_item)
    end

    section("Audio")
    submenu("Bluetooth", BluetoothSettings)
    submenu("Headphones", HeadphonesSettings)

    section("Interface")
    submenu("Display", DisplaySettings)
    submenu("Input Method", InputSettings)

    section("USB")
    submenu("Storage", MassStorageSettings)

    section("System")
    submenu("Database", DatabaseSettings)
    submenu("Firmware", FirmwareSettings)
    submenu("Licenses", LicensesScreen)
  end
}
