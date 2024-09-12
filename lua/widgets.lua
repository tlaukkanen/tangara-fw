local lvgl = require("lvgl")

local power = require("power")
local bluetooth = require("bluetooth")
local font = require("font")
local backstack = require("backstack")
local styles = require("styles")
local database = require("database")
local theme = require("theme")
local screen = require("screen")
local images = require("images")

local img = {
  db = lvgl.ImgData("//lua/img/db.png"),
  chg = lvgl.ImgData("//lua/img/bat/chg.png"),
  chg_outline = lvgl.ImgData("//lua/img/bat/chg_outline.png"),
  bat_100 = lvgl.ImgData("//lua/img/bat/100.png"),
  bat_80 = lvgl.ImgData("//lua/img/bat/80.png"),
  bat_60 = lvgl.ImgData("//lua/img/bat/60.png"),
  bat_40 = lvgl.ImgData("//lua/img/bat/40.png"),
  bat_20 = lvgl.ImgData("//lua/img/bat/20.png"),
  bat_0 = lvgl.ImgData("//lua/img/bat/0.png"),
  bt_conn = lvgl.ImgData("//lua/img/bt_conn.png"),
  bt = lvgl.ImgData("//lua/img/bt.png")
}

local widgets = {}

function widgets.Description(obj, text)
  local label = obj:Label {}
  if text then
    label:set { text = text }
  end
  label:add_flag(lvgl.FLAG.HIDDEN)
  return label
end

widgets.MenuScreen = screen:new {
  show_back = false,
  title = "",
  create_ui = function(self)
    self.root = lvgl.Object(nil, {
      flex = {
        flex_direction = "column",
        flex_wrap = "nowrap",
        justify_content = "flex-start",
        align_items = "flex-start",
        align_content = "flex-start",
      },
      w = lvgl.PCT(100),
      h = lvgl.PCT(100),
    })
    self.root:center()
    self.status_bar = widgets.StatusBar(self, {
      back_cb = self.show_back and backstack.pop or nil,
      title = self.title,
      transparent_bg = true
    })
  end
}

function widgets.Row(parent, left_text, right_text)
  local container = parent:Object {
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
  local left = container:Label {
    text = left_text,
    flex_grow = 1,
  }
  local right = container:Label {
    text = right_text or "",
  }
  return {
    left = left,
    right = right,
  }
end

local bindings_meta = {}
bindings_meta["__add"] = function(a, b)
  return setmetatable(table.move(a, 1, #a, #b + 1, b), bindings_meta)
end

function widgets.StatusBar(parent, opts)
  local root = parent.root:Object {
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
    pad_right = 4,
    pad_column = 1,
    scrollbar_mode = lvgl.SCROLLBAR_MODE.OFF,
  }

  if not opts.transparent_bg then
    theme.set_subject(root, "header");
  end

  if opts.back_cb then
    local back = root:Button {
      w = lvgl.SIZE_CONTENT,
      h = lvgl.SIZE_CONTENT,
    }
    back:Image{src=images.back}
    theme.set_subject(back, "back_button")
    widgets.Description(back, "Back")
    back:onClicked(opts.back_cb)
    back:onevent(lvgl.EVENT.FOCUSED, function()
      local first_view = parent.content
      if not first_view then return end
      while first_view:get_child_cnt() > 0 do
        first_view = first_view:get_child(0)
      end
      if first_view then
        first_view:scroll_to_view_recursive(1)
      end
    end)
  end

  local title = root:Label {
    w = lvgl.PCT(100),
    h = lvgl.SIZE_CONTENT,
    text_font = font.fusion_10,
    text = "",
    align = lvgl.ALIGN.CENTER,
    flex_grow = 1,
    pad_left = 2,
  }
  if opts.title then
    title:set { text = opts.title }
  end

  local db_updating = root:Image { src = img.db }
  theme.set_subject(db_updating, "database_indicator")
  local bt_icon = root:Image {}
  local battery_icon = root:Image {}
  local charge_icon = battery_icon:Image { src = img.chg }
  local charge_icon_outline = battery_icon:Image { src = img.chg_outline }
  charge_icon_outline:center();
  charge_icon:center()

  local is_charging = nil
  local percent = nil

  local function update_battery_icon()
    if is_charging == nil or percent == nil then return end
    local src
    theme.set_subject(battery_icon, "battery")
    if percent >= 95 then
      theme.set_subject(battery_icon, "battery_100")
      src = img.bat_100
    elseif percent >= 75 then
      theme.set_subject(battery_icon, "battery_80")
      src = img.bat_80
    elseif percent >= 55 then
      theme.set_subject(battery_icon, "battery_60")
      src = img.bat_60
    elseif percent >= 35 then
      theme.set_subject(battery_icon, "battery_40")
      src = img.bat_40
    elseif percent >= 15 then
      theme.set_subject(battery_icon, "battery_20")
      src = img.bat_20
    else
      theme.set_subject(battery_icon, "battery_0")
      src = img.bat_0
    end
    if is_charging then
      theme.set_subject(battery_icon, "battery_charging")
      theme.set_subject(charge_icon, "battery_charge_icon")
      theme.set_subject(charge_icon_outline, "battery_charge_outline")
      charge_icon:clear_flag(lvgl.FLAG.HIDDEN)
      charge_icon_outline:clear_flag(lvgl.FLAG.HIDDEN)
    else
      charge_icon:add_flag(lvgl.FLAG.HIDDEN)
      charge_icon_outline:add_flag(lvgl.FLAG.HIDDEN)
    end
    battery_icon:set_src(src)
  end

  parent.bindings = {
    database.updating:bind(function(yes)
      if yes then
        db_updating:clear_flag(lvgl.FLAG.HIDDEN)
      else
        db_updating:add_flag(lvgl.FLAG.HIDDEN)
      end
    end),
    power.battery_pct:bind(function(pct)
      percent = pct
      update_battery_icon()
    end),
    power.plugged_in:bind(function(p)
      is_charging = p
      update_battery_icon()
    end),
    bluetooth.enabled:bind(function(en)
      if en then
        bt_icon:clear_flag(lvgl.FLAG.HIDDEN)
      else
        bt_icon:add_flag(lvgl.FLAG.HIDDEN)
      end
    end),
    bluetooth.connected:bind(function(connected)
      theme.set_subject(bt_icon, "bluetooth_icon")
      if connected then
        bt_icon:set_src(img.bt_conn)
      else
        bt_icon:set_src(img.bt)
      end
    end),
  }
  setmetatable(parent.bindings, bindings_meta)
end

function widgets.IconBtn(parent, icon, text)
  local btn = parent:Button {
    flex = {
      flex_direction = "row",
      justify_content = "flex-start",
      align_items = "center",
      align_content = "center"
    },
    w = lvgl.SIZE_CONTENT,
    h = lvgl.SIZE_CONTENT,
  }
  btn:Image {
    src = icon
  }
  btn:Label {
    text = text,
    text_font = font.fusion_10
  }
  return btn
end

function widgets.InfiniteList(parent, iterator, opts)
  local infinite_list = {}

  infinite_list.root = lvgl.List(parent, {
    w = lvgl.PCT(100),
    h = lvgl.PCT(100),
    flex_grow = 1,
    scrollbar_mode = lvgl.SCROLLBAR_MODE.OFF
  })

  local refreshing = false -- Used so that we can ignore focus events during this phase
  local function refresh_group()
    refreshing = true
    local group = lvgl.group.get_default()
    local focused_obj = group:get_focused()
    local num_children = infinite_list.root:get_child_cnt()
    if (focused_obj) then
      group:clear_focus()
    end
    -- remove all children from the group and re-add them
    for i = 0, num_children - 1 do
      lvgl.group.remove_obj(infinite_list.root:get_child(i))
    end
    for i = 0, num_children - 1 do
      group:add_obj(infinite_list.root:get_child(i))
    end
    if (focused_obj) then
      lvgl.group.focus_obj(focused_obj)
    end
    refreshing = false
  end

  local fwd_iterator = iterator:clone()
  local bck_iterator = iterator:clone()

  local last_selected = 0
  local last_index = 0
  local first_index = 0

  local function remove_top()
    local obj = infinite_list.root:get_child(0)
    obj:delete()
    first_index = first_index + 1
    bck_iterator:next()
  end

  local function remove_last()
    local obj = infinite_list.root:get_child(-1)
    obj:delete()
    last_index = last_index - 1
    fwd_iterator:prev()
  end

  local function add_item(item, index)
    if not item then
      return
    end
    local this_item = index
    local add_to_top = false
    if this_item < first_index then
      first_index = this_item
      add_to_top = true
    end
    if this_item > last_index then last_index = index end
    local btn = infinite_list.root:add_btn(nil, tostring(item))
    if add_to_top then
      btn:move_to_index(0)
    end
    -- opts.callback should take an item and return a function matching the arg of onClicked
    if opts.callback then
      btn:onClicked(opts.callback(item))
    end
    btn:onevent(lvgl.EVENT.FOCUSED, function()
      if refreshing then return end
      if this_item > last_selected and this_item - first_index > 3 then
        -- moving forward
        local to_add = fwd_iterator:next()
        if to_add then
          remove_top()
          add_item(to_add, last_index + 1)
        end
      end
      if this_item < last_selected then
        -- moving backward
        if (first_index > 0 and last_index - this_item > 3) then
          local to_add = bck_iterator:prev();
          if to_add then
            remove_last()
            add_item(to_add, first_index - 1)
            refresh_group()
          end
        end
      end
      last_selected = this_item
    end)
    btn:add_style(styles.list_item)
    return btn
  end

  for idx = 0, 8 do
    local val = fwd_iterator()
    if not val then
      break
    end
    add_item(val, idx)
  end

  return infinite_list
end

return widgets
