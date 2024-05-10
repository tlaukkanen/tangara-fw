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

        local buttons = header:Object({
            flex = {
                flex_direction = "row",
                flex_wrap = "wrap",
                justify_content = "flex-end",
                align_items = "center",
                align_content = "center"
            },
            w = lvgl.PCT(100),
            h = lvgl.SIZE_CONTENT,
            pad_column = 4
        })
        local original_iterator = self.iterator:clone()
        local enqueue = widgets.IconBtn(buttons, "//lua/img/enqueue.png", "Enqueue")
        enqueue:onClicked(function()
            queue.add(original_iterator)
            playback.playing:set(true)
        end)
        -- enqueue:add_flag(lvgl.FLAG.HIDDEN)
        local play = widgets.IconBtn(buttons, "//lua/img/play_small.png", "Play")
        play:onClicked(function()
            queue.clear()
            queue.add(original_iterator)
            playback.playing:set(true)
            backstack.push(playing:new())
        end)

        local infinite_list = widgets.InfiniteList(self.root, self.iterator, {
            callback = function(item) 
                return function()
                    local contents = item:contents()
                    if type(contents) == "userdata" then
                        backstack.push(require("browser"):new{
                            title = self.title,
                            iterator = contents,
                            breadcrumb = tostring(item)
                        })
                    else
                        queue.clear()
                        queue.add(contents)
                        playback.playing:set(true)
                        backstack.push(playing:new())
                    end
                end
            end
        })
    end
}
