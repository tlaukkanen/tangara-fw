local lvgl = require("lvgl")

local widgets = {}

function widgets.StatusBar(parent)
  local container = parent:Object {
    flex = {
        flex_direction = "row",
        justify_content = "flex-start",
        align_items = "center",
        align_content = "center",
    },
    w = lvgl.HOR_RES(),
    h = 16,
  }

  container:Label {
    w = lvgl.SIZE_CONTENT,
    h = 12,
    text = "<",
  }

  container:Label {
    w = lvgl.PCT(100),
    h = 16,
    text = "cool title",
    flex_grow = 1,
  }

  container:Label {
    w = lvgl.SIZE_CONTENT,
    h = 16,
    text = "69%",
  }
end

return widgets
