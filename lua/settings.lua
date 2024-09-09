local lvgl = require("lvgl")
local backstack = require("backstack")
local widgets = require("widgets")
local styles = require("styles")
local volume = require("volume")
local display = require("display")
local controls = require("controls")
local bluetooth = require("bluetooth")
local theme = require("theme")
local filesystem = require("filesystem")
local database = require("database")
local usb = require("usb")
local font = require("font")
local main_menu = require("main_menu")

local SettingsScreen = widgets.MenuScreen:new {
  show_back = true,
  create_ui = function(self)
    widgets.MenuScreen.create_ui(self)
    self.content = self.root:Object {
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
  end
}

local BluetoothPairing = SettingsScreen:new {
  title = "Nearby Devices",
  create_ui = function(self)
    SettingsScreen.create_ui(self)

    local devices = self.content:List {
      w = lvgl.PCT(100),
      h = lvgl.SIZE_CONTENT,
    }

    self.bindings = self.bindings + {
      bluetooth.discovered_devices:bind(function(devs)
        devices:clean()
        for _, dev in pairs(devs) do
          devices:add_btn(nil, dev.name):onClicked(function()
            bluetooth.paired_device:set(dev)
            backstack.pop()
          end)
        end
      end)
    }
  end,
  on_show = function() bluetooth.discovering:set(true) end,
  on_hide = function() bluetooth.discovering:set(false) end,
}

local BluetoothSettings = SettingsScreen:new {
  title = "Bluetooth",
  create_ui = function(self)
    SettingsScreen.create_ui(self)

    -- Enable/Disable switch
    local enable_container = self.content:Object {
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

    self.bindings = self.bindings + {
      bluetooth.enabled:bind(function(en)
        if en then
          enable_sw:add_state(lvgl.STATE.CHECKED)
        else
          enable_sw:clear_state(lvgl.STATE.CHECKED)
        end
      end),
    }

    -- Connection status
    -- This is presented as a label on the field showing the currently paired
    -- device.

    local paired_label =
        self.content:Label {
          text = "",
          pad_bottom = 1,
        }
    theme.set_style(paired_label, "settings_title")

    self.bindings = self.bindings + {
      bluetooth.connecting:bind(function(conn)
        if conn then
          paired_label:set { text = "Connecting to:" }
        else
          if bluetooth.connected:get() then
            paired_label:set { text = "Connected to:" }
          else
            paired_label:set { text = "Paired with:" }
          end
        end
      end),
    }

    -- The name of the currently paired device.
    local paired_container = self.content:Object {
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

    self.bindings = self.bindings + {
      bluetooth.paired_device:bind(function(device)
        if device then
          paired_device:set { text = device.name }
          clear_paired:clear_flag(lvgl.FLAG.HIDDEN)
        else
          paired_device:set { text = "None" }
          clear_paired:add_flag(lvgl.FLAG.HIDDEN)
        end
      end),
    }

    theme.set_style(self.content:Label {
      text = "Known Devices",
      pad_bottom = 1,
    }, "settings_title")

    local devices = self.content:List {
      w = lvgl.PCT(100),
      h = lvgl.SIZE_CONTENT,
    }

    -- 'Pair new device' button that goes to the discovery screen.

    local button_container = self.content:Object {
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
    button_container:add_style(styles.list_item)

    local pair_new = button_container:Button {}
    pair_new:Label { text = "Pair new device" }
    pair_new:onClicked(function()
      backstack.push(BluetoothPairing:new())
    end)


    self.bindings = self.bindings + {
      bluetooth.known_devices:bind(function(devs)
        local group = lvgl.group.get_default()
        group.remove_obj(pair_new)
        devices:clean()
        for _, dev in pairs(devs) do
          devices:add_btn(nil, dev.name):onClicked(function()
            bluetooth.paired_device:set(dev)
          end)
        end
        group:add_obj(pair_new)
      end)
    }
  end
}

local HeadphonesSettings = SettingsScreen:new {
  title = "Headphones",
  create_ui = function(self)
    SettingsScreen.create_ui(self)

    theme.set_style(self.content:Label {
      text = "Maxiumum volume limit",
    }, "settings_title")

    local volume_chooser = self.content:Dropdown {
      options = "Line Level (-10 dB)\nCD Level (+6 dB)\nMaximum (+10dB)",
      selected = 1,
    }
    local limits = { -10, 6, 10 }
    volume_chooser:onevent(lvgl.EVENT.VALUE_CHANGED, function()
      -- luavgl dropdown binding uses 0-based indexing :(
      local selection = volume_chooser:get('selected') + 1
      volume.limit_db:set(limits[selection])
    end)

    theme.set_style(self.content:Label {
      text = "Left/Right balance",
    }, "settings_title")

    local balance = self.content:Slider {
      w = lvgl.PCT(100),
      range = { min = -100, max = 100 },
      value = 0,
    }
    balance:onevent(lvgl.EVENT.VALUE_CHANGED, function()
      volume.left_bias:set(balance:value())
    end)

    local balance_label = self.content:Label {}

    self.bindings = self.bindings + {
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

local DisplaySettings = SettingsScreen:new {
  title = "Display",
  create_ui = function(self)
    SettingsScreen.create_ui(self)

    local brightness_title = self.content:Object {
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

    local brightness = self.content:Slider {
      w = lvgl.PCT(100),
      range = { min = 0, max = 100 },
      value = display.brightness:get(),
    }
    brightness:onevent(lvgl.EVENT.VALUE_CHANGED, function()
      display.brightness:set(brightness:value())
    end)

    self.bindings = self.bindings + {
      display.brightness:bind(function(b)
        brightness_pct:set { text = tostring(b) .. "%" }
      end)
    }
  end
}

local ThemeSettings = SettingsScreen:new {
  title = "Theme",
  create_ui = function(self)
    SettingsScreen.create_ui(self)

    theme.set_style(self.content:Label {
      text = "Theme",
    }, "settings_title")

    local themeOptions = {}
    themeOptions["Dark"] = "/lua/theme_dark.lua"
    themeOptions["Light"] = "/lua/theme_light.lua"
    themeOptions["High Contrast"] = "/lua/theme_hicon.lua"

    -- Parse theme directory for more themes
    local theme_dir_iter = filesystem.iterator("/.themes/")
    for dir in theme_dir_iter do
      local theme_name = tostring(dir):match("(.+).lua$")
      themeOptions[theme_name] = "/sdcard/.themes/" .. theme_name .. ".lua"
    end

    local saved_theme = theme.theme_filename();
    local saved_theme_name = saved_theme:match(".+/(.*).lua$")

    local options = ""
    local idx = 0
    local selected_idx = -1
    for i, v in pairs(themeOptions) do
      if (saved_theme == v) then
          selected_idx = idx
      end
      if idx > 0 then
        options = options .. "\n"
      end
      options = options .. i 
      idx = idx + 1
    end

    if (selected_idx == -1) then
      options = options .. "\n" .. saved_theme_name
      selected_idx = idx
    end

    local theme_chooser = self.content:Dropdown {
      options = options,
    }
    theme_chooser:set({selected = selected_idx})

    theme_chooser:onevent(lvgl.EVENT.VALUE_CHANGED, function()
      local option = theme_chooser:get('selected_str')
      local selectedTheme = themeOptions[option]
      if (selectedTheme) then
        theme.load_theme(tostring(selectedTheme)) 
        backstack.reset(main_menu:new())
      end
    end)
  end
}

local InputSettings = SettingsScreen:new {
  title = "Input Method",
  create_ui = function(self)
    SettingsScreen.create_ui(self)

    theme.set_style(self.content:Label {
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

    local controls_chooser = self.content:Dropdown {
      options = options,
    }

    self.bindings = self.bindings + {
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

    theme.set_style(self.content:Label {
      text = "Scroll Sensitivity",
    }, "settings_title")

    local slider_scale = 4; -- Power steering
    local sensitivity = self.content:Slider {
      w = lvgl.PCT(90),
      range = { min = 0, max = 255 / slider_scale },
      value = controls.scroll_sensitivity:get() / slider_scale,
    }
    sensitivity:onevent(lvgl.EVENT.VALUE_CHANGED, function()
      controls.scroll_sensitivity:set(sensitivity:value() * slider_scale)
    end)
  end
}

local MassStorageSettings = SettingsScreen:new {
  title = "USB Storage",
  create_ui = function(self)
    SettingsScreen.create_ui(self)

    local version = require("version").samd()
    if tonumber(version) < 3 then
      self.content:Label {
        w = lvgl.PCT(100),
        text = "Usb Mass Storage requires a SAMD21 firmware version >=3."
      }
      return
    end

    local enable_container = self.content:Object {
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

    local busy_text = self.content:Label {
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

    self.bindings = self.bindings + {
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
  can_pop = function()
    return not usb.msc_enabled:get()
  end
}

local DatabaseSettings = SettingsScreen:new {
  title = "Database",
  create_ui = function(self)
    SettingsScreen.create_ui(self)

    local db = require("database")
    widgets.Row(self.content, "Schema version", db.version())
    widgets.Row(self.content, "Size on disk", string.format("%.1f KiB", db.size() / 1024))

    local auto_update_container = self.content:Object {
      flex = {
        flex_direction = "row",
        justify_content = "flex-start",
        align_items = "center",
        align_content = "flex-start",
      },
      w = lvgl.PCT(100),
      h = lvgl.SIZE_CONTENT,
      pad_bottom = 4,
    }
    auto_update_container:add_style(styles.list_item)
    auto_update_container:Label { text = "Auto update", flex_grow = 1 }
    local auto_update_sw = auto_update_container:Switch {}

    auto_update_sw:onevent(lvgl.EVENT.VALUE_CHANGED, function()
      database.auto_update:set(auto_update_sw:enabled())
    end)

    local actions_container = self.content:Object {
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

    self.bindings = self.bindings + {
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

local PowerSettings = SettingsScreen:new {
  title = "Power",
  create_ui = function(self)
    SettingsScreen.create_ui(self)
    local power = require("power")

    local charge_pct = widgets.Row(self.content, "Charge").right
    local charge_volts = widgets.Row(self.content, "Voltage").right
    local charge_state = widgets.Row(self.content, "Status").right

    self.bindings = self.bindings + {
      power.battery_pct:bind(function(pct)
        charge_pct:set { text = string.format("%d%%", pct) }
      end),
      power.battery_millivolts:bind(function(mv)
        charge_volts:set { text = string.format("%.2fV", mv / 1000) }
      end),
      power.charge_state:bind(function(state)
        charge_state:set { text = state }
      end),
    }

    local fast_charge_container = self.content:Object {
      flex = {
        flex_direction = "row",
        justify_content = "flex-start",
        align_items = "center",
        align_content = "flex-start",
      },
      w = lvgl.PCT(100),
      h = lvgl.SIZE_CONTENT,
      pad_bottom = 4,
    }
    fast_charge_container:add_style(styles.list_item)
    fast_charge_container:Label { text = "Fast Charging", flex_grow = 1 }
    local fast_charge_sw = fast_charge_container:Switch {}

    fast_charge_sw:onevent(lvgl.EVENT.VALUE_CHANGED, function()
      power.fast_charge:set(fast_charge_sw:enabled())
    end)

    self.bindings = self.bindings + {
      power.fast_charge:bind(function(en)
        if en then
          fast_charge_sw:add_state(lvgl.STATE.CHECKED)
        else
          fast_charge_sw:clear_state(lvgl.STATE.CHECKED)
        end
      end),
    }
  end
}

local SamdConfirmation = SettingsScreen:new {
  title = "Are you sure?",
  create_ui = function(self)
    SettingsScreen.create_ui(self)
    self.content:Label {
      w = lvgl.PCT(100),
      text = "After selecting 'flash', copy the new UF2 file onto the USB drive that appears. The screen will be blank until the update is finished.",
      long_mode = lvgl.LABEL.LONG_WRAP,
    }

    local button_container = self.content:Object {
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
    button_container:add_style(styles.list_item)

    local update = button_container:Button {}
    update:Label { text = "Flash" }
    update:onClicked(function()
      require("version").update_samd()
    end)
  end
}

local FirmwareSettings = SettingsScreen:new {
  title = "Firmware",
  create_ui = function(self)
    SettingsScreen.create_ui(self)
    local version = require("version")
    widgets.Row(self.content, "ESP32", version.esp())
    widgets.Row(self.content, "SAMD21", version.samd())
    widgets.Row(self.content, "Collator", version.collator())

    local button_container = self.content:Object {
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
    button_container:add_style(styles.list_item)

    local update = button_container:Button {}
    update:Label { text = "Update SAMD21" }
    update:onClicked(function()
      backstack.push(SamdConfirmation:new())
    end)
  end
}

local LicensesScreen = SettingsScreen:new {
  title = "Licenses",
  create_ui = function(self)
    SettingsScreen.create_ui(self)
    require("licenses")(self)
  end
}

local FccStatementScreen = SettingsScreen:new {
  title = "FCC Statement",
  create_ui = function(self)
    SettingsScreen.create_ui(self)

    local text_part = function(text)
      self.content:Label {
        w = lvgl.PCT(100),
        h = lvgl.SIZE_CONTENT,
        text = text,
        text_font = font.fusion_10,
        long_mode = lvgl.LABEL.LONG_WRAP,
      }
    end

    text_part(
      "This device complies with part 15 of the FCC Rules. Operation is subject to the following two conditions:")
    text_part("(1) This device may not cause harmful interference, and")
    text_part(
      "(2) this device must accept any interference received, including interference that may cause undesired operation.")

    local scroller = self.content:Object { w = 1, h = 1 }
    scroller:onevent(lvgl.EVENT.FOCUSED, function()
      scroller:scroll_to_view(1);
    end)
    lvgl.group.get_default():add_obj(scroller)
  end
}

local RegulatoryScreen = SettingsScreen:new {
  title = "Regulatory",
  create_ui = function(self)
    SettingsScreen.create_ui(self)
    local version = require("version")

    local small_row = function(left, right)
      local container = self.content:Object {
        flex = {
          flex_direction = "row",
          justify_content = "flex-start",
          align_items = "flex-start",
          align_content = "flex-start"
        },
        w = lvgl.PCT(100),
        h = lvgl.SIZE_CONTENT
      }
      container:add_style(styles.list_item)
      container:Label {
        text = left,
        flex_grow = 1,
        text_font = font.fusion_10,
      }
      container:Label {
        text = right,
        text_font = font.fusion_10,
      }
    end
    small_row("Manufacturer", "cool tech zone")
    small_row("Product model", "CTZ-1")
    small_row("FCC ID", "2BG33-CTZ1")

    local button_container = self.content:Object {
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
    button_container:add_style(styles.list_item)

    local button = button_container:Button {}
    button:Label { text = "FCC Statement" }
    button:onClicked(function()
      backstack.push(FccStatementScreen:new())
    end)

    local logo_container = self.content:Object {
      w = lvgl.PCT(100),
      h = lvgl.SIZE_CONTENT,
      flex = {
        flex_direction = "row",
        justify_content = "center",
        align_items = "center",
        align_content = "center",
      },
      pad_top = 4,
      pad_column = 4,
    }
    theme.set_style(logo_container, "regulatory_icons")
    button_container:add_style(styles.list_item)

    logo_container:Image { src = "//lua/img/ce.png" }
    logo_container:Image { src = "//lua/img/weee.png" }
  end
}

return widgets.MenuScreen:new {
  show_back = true,
  title = "Settings",
  create_ui = function(self)
    widgets.MenuScreen.create_ui(self)
    local list = self.root:List {
      w = lvgl.PCT(100),
      h = lvgl.PCT(100),
      flex_grow = 1,
    }

    local function section(name)
      local elem = list:Label {
        text = name,
        pad_left = 4,
      }
      theme.set_style(elem, "settings_title")
    end

    local function submenu(name, class)
      local item = list:add_btn(nil, name)
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
    submenu("Theme", ThemeSettings)
    submenu("Input Method", InputSettings)

    section("USB")
    submenu("Storage", MassStorageSettings)

    section("System")
    submenu("Database", DatabaseSettings)
    submenu("Power", PowerSettings)

    section("About")
    submenu("Firmware", FirmwareSettings)
    submenu("Licenses", LicensesScreen)
    submenu("Regulatory", RegulatoryScreen)
  end
}
