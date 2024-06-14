---@meta
---
--- lvgl comments
---

--- The `lvgl` module provides bindings to the LVGL graphics library. These
--- bindings were originally based on [luavgl](https://github.com/XuNeo/luavgl/),
--- but have diverged somewhat. These bindings are also largely a very thin
--- abstraction around LVGL's C API, so [LVGL's documentation](https://docs.lvgl.io/8.3/)
--- may also be useful to reference.
--- This module is currently only available from the UI Lua context; i.e. the
--- `main.lua` script run after boot on-device.
---@class lvgl
lvgl = {}

--- Event codes for use with `obj:onevent`. See the [LVGL docs](https://docs.lvgl.io/8.3/overview/event.html#event-codes)
--- for a description of each event type.
--- @enum ObjEventCode
lvgl.EVENT = {
  ALL = 0,
  PRESSED = 0,
  PRESSING = 0,
  PRESS_LOST = 0,
  SHORT_CLICKED = 0,
  LONG_PRESSED = 0,
  LONG_PRESSED_REPEAT = 0,
  CLICKED = 0,
  RELEASED = 0,
  SCROLL_BEGIN = 0,
  SCROLL_END = 0,
  SCROLL = 0,
  GESTURE = 0,
  KEY = 0,
  FOCUSED = 0,
  DEFOCUSED = 0,
  LEAVE = 0,
  HIT_TEST = 0,
  COVER_CHECK = 0,
  REFR_EXT_DRAW_SIZE = 0,
  DRAW_MAIN_BEGIN = 0,
  DRAW_MAIN = 0,
  DRAW_MAIN_END = 0,
  DRAW_POST_BEGIN = 0,
  DRAW_POST = 0,
  DRAW_POST_END = 0,
  DRAW_PART_BEGIN = 0,
  DRAW_PART_END = 0,
  VALUE_CHANGED = 0,
  INSERT = 0,
  REFRESH = 0,
  READY = 0,
  CANCEL = 0,
  DELETE = 0,
  CHILD_CHANGED = 0,
  CHILD_CREATED = 0,
  CHILD_DELETED = 0,
  SCREEN_UNLOAD_START = 0,
  SCREEN_LOAD_START = 0,
  SCREEN_LOADED = 0,
  SCREEN_UNLOADED = 0,
  SIZE_CHANGED = 0,
  STYLE_CHANGED = 0,
  LAYOUT_CHANGED = 0,
  GET_SELF_SIZE = 0,
}

--- Flags that can be set for each LVGL object.
--- @enum ObjFlag
lvgl.FLAG = {
  PRESSED = 0,
  HIDDEN = 0,
  CLICKABLE = 0,
  CLICK_FOCUSABLE = 0,
  CHECKABLE = 0,
  SCROLLABLE = 0,
  SCROLL_ELASTIC = 0,
  SCROLL_MOMENTUM = 0,
  SCROLL_ONE = 0,
  SCROLL_CHAIN_HOR = 0,
  SCROLL_CHAIN_VER = 0,
  SCROLL_CHAIN = 0,
  SCROLL_ON_FOCUS = 0,
  SCROLL_WITH_ARROW = 0,
  SNAPPABLE = 0,
  PRESS_LOCK = 0,
  EVENT_BUBBLE = 0,
  GESTURE_BUBBLE = 0,
  ADV_HITTEST = 0,
  IGNORE_LAYOUT = 0,
  FLOATING = 0,
  OVERFLOW_VISIBLE = 0,
  LAYOUT_1 = 0,
  LAYOUT_2 = 0,
  WIDGET_1 = 0,
  WIDGET_2 = 0,
  USER_1 = 0,
  USER_2 = 0,
  USER_3 = 0,
  USER_4 = 0,
}

--- States that can be set on each LVGL object. See the [LVGL docs](https://docs.lvgl.io/8.3/overview/object.html#states)
--- for an explanation of what each state means.
--- @enum ObjState
lvgl.STATE = {
  DEFAULT = 0,
  CHECKED = 0,
  FOCUSED = 0,
  FOCUS_KEY = 0,
  EDITED = 0,
  HOVERED = 0,
  PRESSED = 0,
  SCROLLED = 0,
  DISABLED = 0,
  USER_1 = 0,
  USER_2 = 0,
  USER_3 = 0,
  USER_4 = 0,
  ANY = 0,
}

--- Enum for specifying what kind of alignment to use for an LVGL object. See
--- the [LVGL docs](https://docs.lvgl.io/8.3/overview/style-props.html#align)
--- for an explanation of each value.
--- @enum ObjAlignType
lvgl.ALIGN = {
  DEFAULT = 0,
  TOP_LEFT = 0,
  TOP_MID = 0,
  TOP_RIGHT = 0,
  BOTTOM_LEFT = 0,
  BOTTOM_MID = 0,
  BOTTOM_RIGHT = 0,
  LEFT_MID = 0,
  RIGHT_MID = 0,
  CENTER = 0,
  OUT_TOP_LEFT = 0,
  OUT_TOP_MID = 0,
  OUT_TOP_RIGHT = 0,
  OUT_BOTTOM_LEFT = 0,
  OUT_BOTTOM_MID = 0,
  OUT_BOTTOM_RIGHT = 0,
  OUT_LEFT_TOP = 0,
  OUT_LEFT_MID = 0,
  OUT_LEFT_BOTTOM = 0,
  OUT_RIGHT_TOP = 0,
  OUT_RIGHT_MID = 0,
  OUT_RIGHT_BOTTOM = 0,
}

--- Long modes for use with labels.
--- @enum LABEL
lvgl.LABEL = {
  LONG_WRAP = 0,
  LONG_DOT = 0,
  LONG_SCROLL = 0,
  LONG_SCROLL_CIRCULAR = 0,
  LONG_CLIP = 0,
}

--- Scroll modes
--- @enum ScrollbarMode
lvgl.SCROLLBAR_MODE = {
  OFF = 0,
  ON = 0,
  ACTIVE = 0,
  AUTO = 0,
}

--- Directions values for dropdown and scroll directions
--- @enum Dir
lvgl.DIR = {
  NONE = 0,
  LEFT = 0,
  RIGHT = 0,
  TOP = 0,
  BOTTOM = 0,
  HOR = 0,
  VER = 0,
  ALL = 0,
}

--- Keyboard modes
--- @enum KeyboardMode
lvgl.KEYBOARD_MODE = {
  TEXT_LOWER = 0,
  TEXT_UPPER = 0,
  SPECIAL = 0,
  NUMBER = 0,
  USER_1 = 0,
  USER_2 = 0,
  USER_3 = 0,
  USER_4 = 0,
  TEXT_ARABIC = 0,
}

--- Flow direction for flex layouts. See the [LVGL docs](https://docs.lvgl.io/8.3/layouts/flex.html#flex-flow)
--- for more details.
--- @enum FlexFlow
lvgl.FLEX_FLOW = {
  ROW = 0,
  COLUMN = 0,
  ROW_WRAP = 0,
  ROW_REVERSE = 0,
  ROW_WRAP_REVERSE = 0,
  COLUMN_WRAP = 0,
  COLUMN_REVERSE = 0,
  COLUMN_WRAP_REVERSE = 0,
}

--- Alignment values for flex layouts. See the [LVGL docs](https://docs.lvgl.io/8.3/layouts/flex.html#flex-align)
--- for more details.
--- @enum FlexAlign
lvgl.FLEX_ALIGN = {
  START = 0,
  END = 0,
  CENTER = 0,
  SPACE_EVENLY = 0,
  SPACE_AROUND = 0,
  SPACE_BETWEEN = 0,
}

--- Alignment values for grid layouts. See the [LVGL docs](https://docs.lvgl.io/8.3/layouts/grid.html#grid-align)
--- for more details.
--- @enum GridAlign
lvgl.GRID_ALIGN = {
  START = 0,
  CENTER = 0,
  END = 0,
  STRETCH = 0,
  SPACE_EVENLY = 0,
  SPACE_AROUND = 0,
  SPACE_BETWEEN = 0,
}

--- Values for KEY events.
--- @enum KEY
lvgl.KEY = {
  UP = 0,
  DOWN = 0,
  RIGHT = 0,
  LEFT = 0,
  ESC = 0,
  DEL = 0,
  BACKSPACE = 0,
  ENTER = 0,
  NEXT = 0,
  PREV = 0,
  HOME = 0,
  END = 0,
}

lvgl.ANIM_REPEAT_INFINITE = 0
lvgl.ANIM_PLAYTIME_INFINITE = 0
lvgl.SIZE_CONTENT = 0
lvgl.RADIUS_CIRCLE = 0
lvgl.COORD_MAX = 0
lvgl.COORD_MIN = 0
lvgl.IMG_ZOOM_NONE = 0
lvgl.BTNMATRIX_BTN_NONE = 0
lvgl.CHART_POINT_NONE = 0
lvgl.DROPDOWN_POS_LAST = 0
lvgl.LABEL_DOT_NUM = 0
lvgl.LABEL_POS_LAST = 0
lvgl.LABEL_TEXT_SELECTION_OFF = 0
lvgl.TABLE_CELL_NONE = 0
lvgl.TEXTAREA_CURSOR_LAST = 0
lvgl.LAYOUT_FLEX = 0
lvgl.LAYOUT_GRID = 0

--- Converts an opacity value as a percentage into the LVGL opacity range of 0
--- to 255.
---@param p integer opacity value in range of 0..100
---@return integer opacity value in the range of 0..255
function lvgl.OPA(p)
end

--- Converts a size in percent into an LVGL size value.
---@param p integer size percentage
---@return integer size in LVGL units
function lvgl.PCT(p)
end

--- Returns the horizontal resolution of the display.
---@return integer
function lvgl.HOR_RES()
end

--- Returns the vertical resolution of the display.
---@return integer
function lvgl.VER_RES()
end

--- Creates a new base LVGL object. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/obj.html)
--- for details about this widget.
--- @param parent? Object The parent for this object, or nil to add to the screen root.
--- @param property? StyleProp Style properties to apply to this object
--- @return Object
function lvgl.Object(parent, property)
end

--- Create a new Bar widget. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/bar.html)
--- for details about this widget.
--- @param parent? Object The parent for this object, or nil to add to the screen root.
--- @param property? BarStyle Style properties to apply to this object
--- @return Bar
function lvgl.Bar(parent, property)
end

--- Create a new Button widget. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/btn.html)
--- for details about this widget.
--- @param parent? Object The parent for this object, or nil to add to the screen root.
--- @param property? StyleProp Style properties to apply to this object
--- @return Button
function lvgl.Button(parent, property)
end

--- Create a new Calendar widget. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/extra/calendar.html)
--- for details about this widget.
--- @param parent? Object The parent for this object, or nil to add to the screen root.
--- @param property? CalendarStyle Style properties to apply to this object
--- @return Calendar
function lvgl.Calendar(parent, property)
end

--- Create a new Checkbox widget. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/checkbox.html)
--- for details about this widget.
--- @param parent? Object The parent for this object, or nil to add to the screen root.
--- @param property? CheckboxStyle Style properties to apply to this object
--- @return Checkbox
function lvgl.Checkbox(parent, property)
end

--- Create a new Dropdown widget. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/dropdown.html)
--- for details about this widget.
--- @param parent? Object The parent for this object, or nil to add to the screen root.
--- @param property? DropdownStyle Style properties to apply to this object
--- @return Dropdown
function lvgl.Dropdown(parent, property)
end

--- Create a new Image widget. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/img.html)
--- for details about this widget.
--- @param parent? Object The parent for this object, or nil to add to the screen root.
--- @param property? ImageStyle Style properties to apply to this object
--- @return Image
function lvgl.Image(parent, property)
end

--- Create a new Label widget. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/label.html)
--- for details about this widget.
--- @param parent? Object The parent for this object, or nil to add to the screen root.
--- @param property? LabelStyle Style properties to apply to this object
--- @return Label
function lvgl.Label(parent, property)
end

--- Create a new Text Area widget. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/textarea.html)
--- for details about this widget.
--- @param parent? Object The parent for this object, or nil to add to the screen root.
--- @param property? TextareaStyle Style properties to apply to this object
--- @return Textarea
function lvgl.Textarea(parent, property)
end

--- Create a new Keyboard widget. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/extra/keyboard.html)
--- for details about this widget.
--- @param parent? Object The parent for this object, or nil to add to the screen root.
--- @param property? KeyboardStyle Style properties to apply to this object
--- @return Keyboard
function lvgl.Keyboard(parent, property)
end

--- Create a new LED widget. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/extra/led.html)
--- for details about this widget.
--- @param parent? Object The parent for this object, or nil to add to the screen root.
--- @param property? LedStyle Style properties to apply to this object
--- @return Led
function lvgl.Led(parent, property)
end

--- Create a new List widget. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/extra/list.html)
--- for details about this widget.
--- @param parent? Object The parent for this object, or nil to add to the screen root.
--- @param property? ListStyle Style properties to apply to this object
--- @return List
function lvgl.List(parent, property)
end

--- Create a new Roller widget. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/roller.html)
--- for details about this widget.
--- @param parent? Object The parent for this object, or nil to add to the screen root.
--- @param property? RollerStyle Style properties to apply to this object
--- @return Roller
function lvgl.Roller(parent, property)
end

--- Create a new Slider widget. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/slider.html)
--- for details about this widget.
--- @param parent? Object The parent for this object, or nil to add to the screen root.
--- @param property? BarStyle Style properties to apply to this object. Sliders use the same style properties as Bars.
--- @return Slider
function lvgl.Slider(parent, property)
end

--- Create a new Switch widget. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/switch.html)
--- for details about this widget.
--- @param parent? Object The parent for this object, or nil to add to the screen root.
--- @param property? StyleProp Style properties to apply to this object
--- @return Switch
function lvgl.Switch(parent, property)
end

--- Create a new Timer. See the [LVGL docs](https://docs.lvgl.io/8.3/overview/timer.html)
--- for more details on LVGL's Timers system.
--- @param p TimerPara Parameters to use for configuring the timer.
--- @return Timer
function lvgl.Timer(p)
end

--- Create a new a font. Fonts can be located on internal flash or the SD card,
--- and must be in LVGL's binary font format.
--- @param path string location of the binary font file
--- @return Font
function lvgl.Font(path)
end

--- Decodes an image from the filesystem and pins it into RAM, returning a
--- lightuserdata that can be passed to `img:set_src`.
--- @param path? string path to the encoded image
--- @return ImgData
function lvgl.ImgData(path)
end

--- Create a new style that can be applied to objects via `obj:add_style`.
--- @param p? StyleProp Style properties that will be applied by this style.
--- @return Style
function lvgl.Style(p)
end

---
--- Base LVGL object. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/obj.html)
--- for additional details.
--- @class Object
obj = {}

--- Creates a new base LVGL object as a child of this object. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/obj.html)
--- for details about this widget.
--- @param property? StyleProp Style properties to apply to this object
--- @return Object
function obj:Object(property)
end

--- Create a new Bar widget as a child of this object. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/bar.html)
--- for details about this widget.
--- @param property? BarStyle Style properties to apply to this object
--- @return Bar
function obj:Bar(property)
end

--- Create a new Button widget as a child of this object. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/btn.html)
--- for details about this widget.
--- @param property? StyleProp Style properties to apply to this object
--- @return Button
function obj:Button(property)
end

--- Create a new Calendar widget as a child of this object. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/extra/calendar.html)
--- for details about this widget.
--- @param property? CalendarStyle Style properties to apply to this object
--- @return Calendar
function obj:Calendar(property)
end

--- Create a new Checkbox widget as a child of this object. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/checkbox.html)
--- for details about this widget.
--- @param property? CheckboxStyle Style properties to apply to this object
--- @return Checkbox
function obj:Checkbox(property)
end

--- Create a new Dropdown widget as a child of this object. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/dropdown.html)
--- for details about this widget.
--- @param property? DropdownStyle Style properties to apply to this object
--- @return Dropdown
function obj:Dropdown(parent, property)
end

--- Create a new Image widget as a child of this object. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/img.html)
--- for details about this widget.
--- @param property? ImageStyle Style properties to apply to this object
--- @return Image
function obj:Image(property)
end

--- Create a new Label widget as a child of this object. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/label.html)
--- for details about this widget.
--- @param property? LabelStyle Style properties to apply to this object
--- @return Label
function obj:Label(property)
end

--- Create a new Text Area widget as a child of this object. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/textarea.html)
--- for details about this widget.
--- @param property? TextareaStyle Style properties to apply to this object
--- @return Textarea
function obj:Textarea(property)
end

--- Create a new Keyboard widget as a child of this object. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/extra/keyboard.html)
--- for details about this widget.
--- @param property? KeyboardStyle Style properties to apply to this object
--- @return Keyboard
function obj:Keyboard(property)
end

--- Create a new LED widget as a child of this object. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/extra/led.html)
--- for details about this widget.
--- @param property? LedStyle Style properties to apply to this object
--- @return Led
function obj:Led(property)
end

--- Create a new List widget as a child of this object. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/extra/list.html)
--- for details about this widget.
--- @param property? ListStyle Style properties to apply to this object
--- @return List
function obj:List(property)
end

--- Create a new Roller widget as a child of this object. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/roller.html)
--- for details about this widget.
--- @param property? RollerStyle Style properties to apply to this object
--- @return Roller
function obj:Roller(parent, property)
end

--- Create a new Slider widget as a child of this object. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/slider.html)
--- for details about this widget.
--- @param property? BarStyle Style properties to apply to this object. Sliders use the same style properties as Bars.
--- @return Slider
function obj:Slider(property)
end

--- Create a new Switch widget as a child of this object. See the [LVGL docs](https://docs.lvgl.io/8.3/widgets/core/switch.html)
--- for details about this widget.
--- @param property? StyleProp Style properties to apply to this object
--- @return Switch
function obj:Switch(property)
end

--- Sets new style properties on this object.
--- @param p StyleProp Style properties to be applied.
function obj:set(p)
end

--- Sets new style properties on this object.
--- @param p StyleProp Style properties to be applied.
--- @param selector integer Selector to detemine when the style is used
---
function obj:set_style(p, selector)
end

--- Sets this object's position relative to another object.
--- @param p AlignToPara
function obj:align_to(p)
end

--- Deletes this object, removing it from the view hierarchy.
function obj:delete()
end

--- Delete all children of this object
function obj:clean()
end

--- Sets the parent of this object, detaching it from any existing parent.
--- @param p Object The new parent object
function obj:set_parent(p)
end

--- Gets this object's parent
--- @return Object Parent
function obj:get_parent()
end

---
--- set and/or get object's parent
--- @param p Object
--- @return Object
function obj:parent(p)
end

---
--- get child object
--- @param id integer 0 the first child, -1 the lastly created child
--- @return Object
function obj:get_child(id)
end

---
--- get object children count
--- @return integer
function obj:get_child_cnt()
end

---
--- get the state of this object
--- @return ObjState
function obj:get_state(p)
end

---
--- Scroll to a given coordinate on an object.
--- @class ScrollToPara
--- @field x    integer position x
--- @field y    integer
--- @field anim boolean
---
--- @param p ScrollToPara
--- @return ObjState
function obj:scroll_to(p)
end

---
--- Tell whether an object is being scrolled or not at this moment
--- @return boolean
function obj:is_scrolling()
end

---
--- Tell whether an object is visible (even partially) now or not
--- @return boolean
function obj:is_visible()
end

---
--- add flag to object
--- @param p ObjFlag
--- @return nil
function obj:add_flag(p)
end

---
--- clear object flag
--- @param p ObjFlag
--- @return nil
function obj:clear_flag(p)
end

---
--- add state to object
--- @param p ObjState
--- @return nil
function obj:add_state(p)
end

---
--- clear object state
--- @param p ObjState
--- @return nil
function obj:clear_state(p)
end

---
--- add style to object
--- @param s Style
--- @param selector? integer
--- @return nil
function obj:add_style(s, selector)
end

---
--- remove style from object
--- @param s Style
--- @param selector? integer
--- @return nil
function obj:remove_style(s, selector)
end

---
--- remove all style from object
--- @return nil
function obj:remove_style_all()
end

---scroll obj by x,y
---@param x integer
---@param y integer
---@param anim_en? boolean
function obj:scroll_by(x, y, anim_en)
end

---scroll obj by x,y
---@param x integer
---@param y integer
---@param anim_en boolean
function obj:scroll_by_bounded(x, y, anim_en)
end

--- Scroll to an object until it becomes visible on its parent
---@param anim_en? boolean
function obj:scroll_to_view(anim_en)
end

--- Scroll to an object until it becomes visible on its parent
--- Do the same on the parent's parent, and so on.
--- Therefore the object will be scrolled into view even it has nested scrollable parents
---@param anim_en? boolean
function obj:scroll_to_view_recursive(anim_en)
end

---scroll obj by x,y, low level APIs
---@param x integer
---@param y integer
---@param anim_en boolean
function obj:scroll_by_raw(x, y, anim_en)
end

---Invalidate the area of the scrollbars
function obj:scrollbar_invalidate()
end

---Checked if the content is scrolled "in" and adjusts it to a normal position.
---@param anim_en boolean
function obj:readjust_scroll(anim_en)
end

---If object is editable
---@return boolean
function obj:is_editable()
end

--- class group def
---@return boolean
function obj:is_group_def()
end

--- Test whether the and object is positioned by a layout or not
---@return boolean
function obj:is_layout_positioned()
end

--- Mark the object for layout update.
---@return nil
function obj:mark_layout_as_dirty()
end

--- Align an object to the center on its parent. same as obj:set{align={type = lvgl.ALIGN.CENTER}}
---@return nil
function obj:center()
end

--- Align an object to the center on its parent. same as obj:set{align={type = lvgl.ALIGN.CENTER}}
---@return nil
function obj:invalidate()
end

--- Sets this object as the current selection of the object's group.
---@return nil
function obj:focus()
end

---
--- Object event callback. `para` is not used for now.
--- @alias EventCallback fun(obj:Object, code: ObjEventCode): nil
---
--- set object event callback
--- @param code ObjEventCode
--- @param cb EventCallback
--- @return nil
function obj:onevent(code, cb)
end

---
--- set object pressed event callback, same as obj:onevent(lvgl.EVENT.PRESSED, cb)
--- @param cb EventCallback
--- @return nil
function obj:onPressed(cb)
end

---
--- set object clicked event callback, same as obj:onevent(lvgl.EVENT.CLICKED, cb)
--- @param cb EventCallback
--- @return nil
function obj:onClicked(cb)
end

---
--- set object short clicked event callback, same as obj:onevent(lvgl.EVENT.SHORT_CLICKED, cb)
--- @param cb EventCallback
--- @return nil
function obj:onShortClicked(cb)
end

---
--- Create anim for object
--- @param p AnimPara
--- @return Anim
function obj:Anim(p)
end

---
--- Get coords of object
--- @return Coords coords
function obj:get_coords()
end

---
--- Get real postion of object relative to its parent
--- @return Coords coords
function obj:get_pos()
end

---
--- Calendar widget
---@class Calendar:Object
---
local calendar = {}

--- set method for calendar widget
--- @param p CalendarStyle
--- @return nil
function calendar:set(p)
end

--- get today para setting from calendar widget
--- @return CalendarDatePara
function calendar:get_today(p)
end

--- get the currently showed date
--- @return CalendarDatePara
function calendar:get_showed(p)
end

--- get the currently pressed day
--- @return CalendarDatePara
function calendar:get_pressed(p)
end

--- get the button matrix object of the calendar.
--- @return Object
function calendar:get_btnm(p)
end

--- create a calendar header with drop-drowns to select the year and month.
--- @return Object
function calendar:Arrow(p)
end

--- create a calendar header with drop-drowns to select the year and month
--- @return Object
function calendar:Dropdown(p)
end

---
--- Bar widget
---@class Bar:Object
---
local bar = {}

--- set method for bar widget
--- @param p BarStyle
--- @return nil
function bar:set(p)
end

---
--- Button widget
---@class Button:Object
---
local button = {}

--- set method for button widget
--- @param p ButtonStyle
--- @return nil
function button:set(p)
end

---
--- Checkbox widget
---@class Checkbox:Object
---
local checkbox = {}

--- set method
--- @param p CheckboxStyle
--- @return nil
function checkbox:set(p)
end

---
--- Get the text of a label
--- @return string
function checkbox:get_text()
end

---
--- Dropdown widget
---@class Dropdown:Object
---
local dropdown = {}

--- set method
--- @param p DropdownStyle
--- @return nil
function dropdown:set(p)
end

--- Gets an attribute of the dropdown.
--- @param which string Which property to retrieve. Valid values are "list", "options", "selected", "option_cnt", "selected_str", "option_index", "symbol", or "dir"
--- @param arg ? string
--- @return string | Dir | Object
function dropdown:get(which, arg)
end

--- Open the drop down list
function dropdown:open()
end

--- Close (Collapse) the drop-down list
function dropdown:close()
end

--- Tells whether the list is opened or not
function dropdown:is_open()
end

--- Add an options to a drop-down list from a string
--- @param option string
--- @param pos integer
function dropdown:add_option(option, pos)
end

--- Tells whether the list is opened or not
function dropdown:clear_option()
end

---
--- Image widget
---@class Image:Object
---
local img = {}

--- Image set method
--- @param p ImageStyle
--- @return nil
function img:set(p)
end

--- set image source
--- @param src string image source path
--- @return nil
function img:set_src(src)
end

--- set image offset
--- img:set_offset{x = 0, y = 100}
--- @param p table
--- @return nil
function img:set_offset(p)
end

--- set image pivot
--- img:set_pivot{x = 0, y = 100}
--- @param p table
--- @return nil
function img:set_pivot(p)
end

--- get image size, return w,h
--- w, h = img:get_img_size()
--- w, h = img:get_img_size("/path/to/this/image.png")
--- @param src ? string
--- @return integer, integer
function img:get_img_size(src)
end

---
--- Label widget
---@class Label: Object
---
local label = {}

--- Image set method
--- @param p LabelStyle
--- @return nil
function label:set(p)
end

---
--- Get the text of a label
--- @return string
function label:get_text()
end

---
--- Get the long mode of a label
--- @return string
function label:get_long_mode()
end

---
--- Get the recoloring attribute
--- @return string
function label:get_recolor()
end

---
--- Insert a text to a label.
--- @param pos integer
--- @param txt string
--- @return nil
function label:ins_text(pos, txt)
end

---
--- Delete characters from a label.
--- @param pos integer
--- @param cnt integer
--- @return nil
function label:cut_text(pos, cnt)
end

---
--- Slider widget
---@class Slider:Object
---
local slider = {}

--- set method for slider widget. Uses Bar widget's properties.
--- @param p BarStyle
--- @return nil
function slider:set(p)
end

--- get value of slider
--- @return integer
function slider:value()
end

--- get whether slider is dragged or not
--- @return boolean
function slider:is_dragged()
end

---
--- Switch widget
---@class Switch:Object
---
local switch = {}

--- set method for switch widget
--- @param p StyleProp
--- @return nil
function switch:set(p)
end

--- get checked state of switch
--- @return boolean
function switch:enabled()
end

---
--- Textarea widget
---@class Textarea: Object
---
local textarea = {}

--- Textarea set method
--- @param p TextareaStyle
--- @return nil
function textarea:set(p)
end

--- get textarea text
--- @return string
function textarea:get_text(p)
end

---
--- Keyboard widget
---@class Keyboard: Object based on btnmatrix object
---
local keyboard = {}

--- Keyboard set method
--- @param p KeyboardStyle
--- @return nil
function keyboard:set(p)
end

---
--- LED widget
---@class Led: Object
---
local led = {}

--- LED set method
--- @param p LedStyle
--- @return nil
function led:set(p)
end

--- LED set to ON
--- @return nil
function led:on()
end

--- LED set to OFF
--- @return nil
function led:off()
end

--- toggle LED status
--- @return nil
function led:toggle()
end

--- get LED brightness
--- @return integer
function led:get_brightness()
end

---
--- List widget
---@class List: Object
---
local list = {}

--- List set method
--- @param p ListStyle
--- @return nil
function list:set(p)
end

--- add text to list
--- @param text string
--- @return Label
function list:add_text(text)
end

--- add button to list
--- @param icon ImgSrc | nil
--- @param text? string
--- @return Object a button object
function list:add_btn(icon, text)
end

--- get list button text
--- @param btn Object
--- @return string
function list:get_btn_text(btn)
end

---
--- Roller widget
---@class Roller: Object
---
local roller = {}

--- Roller set method
--- @param p RollerStyle
--- @return nil
function roller:set(p)
end

--- Get the options of a roller
--- @return string
function roller:get_options()
end

--- Get the index of the selected option
--- @return integer
function roller:get_selected()
end

--- Get the current selected option as a string.
--- @return string
function roller:get_selected_str()
end

--- Get the total number of options
--- @return integer
function roller:get_options_cnt()
end

---
--- Anim
---@class Anim
---
local Anim = {}

--- start animation
--- @return nil
function Anim:start()
end

--- set animation new parameters
--- @param para AnimPara new animation parameters
--- @return nil
function Anim:set(para)
end

--- stop animation
--- @return nil
function Anim:stop()
end

--- delete animation
--- @return nil
function Anim:delete()
end

---
--- Timer
---@class Timer
---
local timer = {}

--- set timer property
--- @param p TimerPara
--- @return nil
function timer:set(p)
end

--- resume timer
--- @return nil
function timer:resume()
end

--- pause timer
--- @return nil
function timer:pause()
end

--- delete timer
--- @return nil
function timer:delete()
end

--- make timer ready now, cb will be made soon on next loop
--- @return nil
function timer:ready()
end

--[[
Font is a light userdata that can be used to set style text_font.
]]
--- @class Font
---

local font = {}


--- Decoded image data that is pinned to memory.
--- @class ImgData

local ImgData = {}

---
--- @class Style : lightuserdata
---
local style = {}

--- update style properties
--- @param p StyleProp
--- @return nil
function style:set(p)
end

--- delete style, only delted style could be gc'ed
--- @return nil
function style:delete()
end

--- remove specified property from style
--- @param p string property name from field of StyleProp
--- @return nil
function style:remove_prop(p)
end

---
--- Align parameter
--- @class Align
--- @field type ObjAlignType
--- @field x_ofs integer
--- @field y_ofs integer

--- AlignTo parameter
--- @class AlignToPara
--- @field type ObjAlignType
--- @field base Object
--- @field x_ofs integer

--- Style properties
--- @class StyleProp
--- @field w? integer
--- @field width? integer
--- @field min_width? integer
--- @field max_width? integer
--- @field height? integer
--- @field min_height? integer
--- @field max_height? integer
--- @field x? integer
--- @field y? integer
--- @field size? integer set size is equivalent to set w/h to same value
--- @field align? Align | ObjAlignType
--- @field transform_width? integer
--- @field transform_height? integer
--- @field translate_x? integer
--- @field translate_y? integer
--- @field transform_zoom? integer
--- @field transform_angle? integer
--- @field transform_pivot_x? integer
--- @field transform_pivot_y? integer
--- @field pad_all? integer
--- @field pad_top? integer
--- @field pad_bottom? integer
--- @field pad_ver? integer
--- @field pad_left? integer
--- @field pad_right? integer
--- @field pad_hor? integer
--- @field pad_row? integer
--- @field pad_column? integer
--- @field pad_gap? integer
--- @field bg_color? integer | string text color in hex integer or #RGB or #RRGGBB format
--- @field bg_opa? integer
--- @field bg_grad_color? integer
--- @field bg_grad_dir? integer
--- @field bg_main_stop? integer
--- @field bg_grad_stop? integer
--- @field bg_dither_mode? integer
--- @field bg_img_src? integer
--- @field bg_img_opa? integer
--- @field bg_img_recolor? integer
--- @field bg_img_recolor_opa? integer
--- @field bg_img_tiled? integer
--- @field border_color? integer | string
--- @field border_opa? integer
--- @field border_width? integer
--- @field border_side? integer
--- @field border_post? integer
--- @field outline_width? integer
--- @field outline_color? integer | string
--- @field outline_opa? integer
--- @field outline_pad? integer
--- @field shadow_width? integer
--- @field shadow_ofs_x? integer
--- @field shadow_ofs_y? integer
--- @field shadow_spread? integer
--- @field shadow_color? integer | string
--- @field shadow_opa? integer
--- @field img_opa? integer
--- @field img_recolor? integer
--- @field img_recolor_opa? integer
--- @field line_width? integer
--- @field line_dash_width? integer
--- @field line_dash_gap? integer
--- @field line_rounded? integer
--- @field line_color? integer | string
--- @field line_opa? integer
--- @field arc_width? integer
--- @field arc_rounded? integer
--- @field arc_color? integer | string
--- @field arc_opa? integer
--- @field arc_img_src? integer
--- @field text_color? integer | string
--- @field text_opa? integer
--- @field text_font? Font
--- @field text_letter_space? integer
--- @field text_line_space/ integer
--- @field text_decor? integer
--- @field text_align? integer
--- @field radius? integer
--- @field clip_corner? integer
--- @field opa? integer
--- @field color_filter_opa? integer
--- @field anim_time? integer
--- @field anim_speed? integer
--- @field blend_mode? integer
--- @field layout? integer
--- @field base_dir? integer
--- @field flex_flow? FlexFlow
--- @field flex_main_place? FlexAlign
--- @field flex_cross_place? FlexAlign
--- @field flex_track_place/ FlexAlign
--- @field flex_grow? integer 0..255
--- @field flex? FlexLayoutPara

---

--- Object style
--- @class ObjectStyle :StyleProp
--- @field x integer
--- @field y integer
--- @field w integer
--- @field h integer
--- @field align Align | integer
--- @field align_to AlignToPara
--- @field scrollbar_mode ScrollbarMode
--- @field scroll_dir Dir
--- @field scroll_snap_x integer
--- @field scroll_snap_y integer
---

--- Image style
--- @class ImageStyle :StyleProp
--- @field src string
--- @field offset_x integer offset of image
--- @field offset_y integer
--- @field angle integer
--- @field zoom integer
--- @field antialias boolean
--- @field pivot table
---

--- Label style
--- @class LabelStyle :StyleProp
--- @field text string

--- Bar style
--- @class BarStyle :StyleProp
--- @field range BarRangePara
--- @field value integer

--- Button style
--- @class ButtonStyle :StyleProp

--- Checkbox style
--- @class CalendarStyle :StyleProp
--- @field today CalendarDatePara
--- @field showed CalendarDatePara

--- Checkbox style
--- @class CheckboxStyle :StyleProp
--- @field text string

--- Dropdown style
--- @class DropdownStyle :StyleProp
--- @field text string | nil
--- @field options string
--- @field selected integer
--- @field dir Dir
--- @field symbol lightuserdata | string
--- @field highlight boolean

--- Textarea style
--- @class TextareaStyle :StyleProp
--- @field text string
--- @field placeholder string
--- @field cursor integer cursor position
--- @field password_mode boolean enable password
--- @field one_line boolean enable one line mode
--- @field password_bullet string Set the replacement characters to show in password mode
--- @field accepted_chars string DO NOT USE. Set a list of characters. Only these characters will be accepted by the text area E.g. "+-.,0123456789"
--- @field max_length integer Set max length of a Text Area.
--- @field password_show_time integer Set how long show the password before changing it to '*'

--- Keyboard style
--- @class KeyboardStyle :StyleProp
--- @field textarea Textarea textarea object
--- @field mode KeyboardMode
--- @field popovers boolean Show the button title in a popover when pressed.

--- Led style
--- @class LedStyle :StyleProp
--- @field color integer|string color of led
--- @field brightness integer brightness in range of 0..255

--- List style
--- @class ListStyle :StyleProp

--- Roller style
--- @class RollerStyle :StyleProp
--- @field options table | string
--- @field selected table | integer
--- @field visible_cnt integer

---
--- Anim(for object) parameter
--- @alias AnimExecCb fun(obj:any, value:integer): nil
--- @alias AnimDoneCb fun(anim:Anim, var:any): nil

--- @class AnimPara
--- @field run boolean run this anim right now, or later using anim:start(). default: false
--- @field start_value integer start value
--- @field end_value integer
--- @field duration integer Anim duration in milisecond
--- @field delay integer Set a delay before starting the animation
--- @field repeat_count integer Anim repeat count, default: 1, set to 0 to disable repeat, set to lvgl.ANIM_REPEAT_INFINITE for infinite repeat, set to any other integer for specified repeate count
--- @field playback_delay integer
--- @field playback_time integer
--- @field early_apply boolean set start_value right now or not. default: true
--- @field path string |  "linear" |  "ease_in" |  "ease_out" |  "ease_in_out" |  "overshoot" |  "bounce" |  "step"
--- @field exec_cb AnimExecCb
--- @field done_cb AnimDoneCb


---
--- Timer para
--- @alias TimerCallback fun(t:Timer): nil
--- @class TimerPara
--- @field paused boolean Do not start timer immediaely
--- @field period integer timer period in ms unit
--- @field repeat_count integer | -1 |
--- @field cb TimerCallback
---


---
--- @alias ImgSrc string | lightuserdata

--- Alignment values for flex layouts. See the [LVGL docs](https://docs.lvgl.io/8.3/layouts/flex.html#flex-align)
--- for more details.
--- @alias flexAlignOptions
---| "flex-start"
---| "flex-end"
---| "center" wow
---| "space-between"
---| "space-around"
---| "space-evenly"

--- @class FlexLayoutPara
--- @field flex_direction? "row" | "column" | "row-reverse" | "column-reverse"
--- @field flex_wrap? "nowrap" | "wrap" | "wrap-reverse"
--- @field justify_content? flexAlignOptions
--- @field align_items? flexAlignOptions
--- @field align_content? flexAlignOptions


---
--- BarRange para
--- @class BarRangePara
--- @field min integer
--- @field max integer
---

---
--- CalendarToday para
--- @class CalendarDatePara
--- @field year integer
--- @field month integer
--- @field day integer
---

---
--- Coordinates
--- @class Coords
--- @field x1 integer
--- @field y1 integer
--- @field x2 integer
--- @field y2 integer
---

return lvgl
