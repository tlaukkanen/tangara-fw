local lvgl = require("lvgl")
local widgets = require("widgets")
local backstack = require("backstack")
local font = require("font")
local queue = require("queue")
local playing = require("playing")
local styles = require("styles")
local playback = require("playback")
local theme = require("theme")
local screen = require("screen")
local filesystem = require("filesystem")

return screen:new{
    createUi = function(self)
        self.root = lvgl.Object(nil, {
            flex = {
                flex_direction = "column",
                flex_wrap = "wrap",
                justify_content = "flex-start",
                align_items = "flex-start",
                align_content = "flex-start"
            },
            w = lvgl.HOR_RES(),
            h = lvgl.VER_RES()
        })
        self.root:center()

        self.status_bar = widgets.StatusBar(self, {
            back_cb = backstack.pop,
            title = self.title
        })

        local header = self.root:Object{
            flex = {
                flex_direction = "column",
                flex_wrap = "wrap",
                justify_content = "flex-start",
                align_items = "flex-start",
                align_content = "flex-start"
            },
            w = lvgl.HOR_RES(),
            h = lvgl.SIZE_CONTENT,
            pad_left = 4,
            pad_right = 4,
            pad_bottom = 2,
            bg_opa = lvgl.OPA(100),
            scrollbar_mode = lvgl.SCROLLBAR_MODE.OFF
        }
        theme.set_style(header, "header")

        if self.breadcrumb then
            header:Label{
                text = self.breadcrumb,
                text_font = font.fusion_10
            }
        end

        local infinite_list = widgets.InfiniteList(self.root, self.iterator, {
            callback = function(item) 
                return function()
                    local is_dir = item:is_directory()
                    if is_dir then
                        backstack.push(require("file_browser"):new{
                            title = self.title,
                            iterator = filesystem.iterator(tostring(item)),
                            breadcrumb = tostring(item)
                        })
                    end
                end
            end
        })
    end
}
