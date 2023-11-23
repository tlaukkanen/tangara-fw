local lvgl = require("lvgl")
local power = require("power")
local bluetooth = require("bluetooth")
local playback = require("playback")
local font = require("font")

local widgets = {}

function widgets.StatusBar(parent, opts)
  local status_bar = {}

  status_bar.root = parent:Object {
    flex = {
      flex_direction = "row",
      justify_content = "flex-start",
      align_items = "center",
      align_content = "center",
    },
    w = lvgl.HOR_RES(),
    h = lvgl.SIZE_CONTENT,
    pad_top = 1,
    pad_bottom = 1,
    pad_left = 4,
    pad_column = 1,
    scrollbar_mode = lvgl.SCROLLBAR_MODE.OFF,
  }

  if not opts.transparent_bg then
    status_bar.root:set {
      bg_opa = lvgl.OPA(100),
      bg_color = "#fafafa",
    }
  end

  if opts.back_cb then
    status_bar.back = status_bar.root:Button {
      w = lvgl.SIZE_CONTENT,
      h = 12,
    }
    status_bar.back:Label({ text = "<", align = lvgl.ALIGN.CENTER })
    status_bar.back:onClicked(opts.back_cb)
  end

  status_bar.title = status_bar.root:Label {
    w = lvgl.PCT(100),
    h = lvgl.SIZE_CONTENT,
    text_font = font.fusion_10,
    text = "",
    align = lvgl.ALIGN.CENTER,
    flex_grow = 1,
  }
  if opts.title then
    status_bar.title:set { text = opts.title }
  end

  status_bar.playing = status_bar.root:Image {}
  status_bar.bluetooth = status_bar.root:Image {}
  status_bar.battery = status_bar.root:Image {}
  status_bar.chg = status_bar.battery:Image {
    src = "//lua/img/bat/chg.png"
  }
  status_bar.chg:center()

  local is_charging = nil
  local percent = nil

  function update_battery_icon()
    if is_charging == nil or percent == nil then return end
    local src
    if percent >= 95 then
      src = "100.png"
    elseif percent >= 75 then
      src = "80.png"
    elseif percent >= 55 then
      src = "60.png"
    elseif percent >= 35 then
      src = "40.png"
    elseif percent >= 15 then
      src = "20.png"
    else
      if is_charging then
        src = "0chg.png"
      else
        src = "0.png"
      end
    end
    if is_charging then
      status_bar.chg:clear_flag(lvgl.FLAG.HIDDEN)
    else
      status_bar.chg:add_flag(lvgl.FLAG.HIDDEN)
    end
    status_bar.battery:set_src("//lua/img/bat/" .. src)
  end

  status_bar.bindings = {
    power.battery_pct:bind(function(pct)
      percent = pct
      update_battery_icon()
    end),
    power.plugged_in:bind(function(p)
      is_charging = p
      update_battery_icon()
    end),
    playback.playing:bind(function(playing)
      if playing then
        status_bar.playing:set_src("//lua/assets/play.png")
      else
        status_bar.playing:set_src("//lua/assets/pause.png")
      end
    end),
    playback.track:bind(function(track)
      if track then
        status_bar.playing:clear_flag(lvgl.FLAG.HIDDEN)
      else
        status_bar.playing:add_flag(lvgl.FLAG.HIDDEN)
      end
    end),
    bluetooth.enabled:bind(function(en)
      if en then
        status_bar.bluetooth:clear_flag(lvgl.FLAG.HIDDEN)
      else
        status_bar.bluetooth:add_flag(lvgl.FLAG.HIDDEN)
      end
    end),
    bluetooth.connected:bind(function(connected)
      if connected then
        status_bar.bluetooth:set_src("//lua/assets/bt_conn.png")
      else
        status_bar.bluetooth:set_src("//lua/assets/bt.png")
      end
    end),
  }

  return status_bar
end

function widgets.IconBtn(parent, icon, text)
  local btn = parent:Button {
    flex = {
      flex_direction = "row",
      justify_content = "flex-start",
      align_items = "center",
      align_content = "center",
    },
    w = lvgl.SIZE_CONTENT,
    h = lvgl.SIZE_CONTENT,
    pad_top = 1,
    pad_bottom = 1,
    pad_left = 1,
    pad_column = 1,
  }
  btn:Image { src = icon }
  btn:Label { text = text, text_font = font.fusion_10 }
  return btn
end

return widgets
