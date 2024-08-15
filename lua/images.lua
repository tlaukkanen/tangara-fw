local lvgl = require("lvgl")

local img = {
  back = lvgl.ImgData("//lua/img/back.png"),
  play = lvgl.ImgData("//lua/img/play.png"),
  play_small = lvgl.ImgData("//lua/img/playcirc.png"),
  pause = lvgl.ImgData("//lua/img/pause.png"),
  pause_small = lvgl.ImgData("//lua/img/pausecirc.png"),
  next = lvgl.ImgData("//lua/img/next.png"),
  prev = lvgl.ImgData("//lua/img/prev.png"),
  shuffle = lvgl.ImgData("//lua/img/shuffle.png"),
  repeat_src = lvgl.ImgData("//lua/img/repeat.png"), -- repeat is a reserved word
  queue = lvgl.ImgData("//lua/img/queue.png"),
  files = lvgl.ImgData("//lua/img/files.png"),
  settings = lvgl.ImgData("//lua/img/settings.png"),
}

return img
