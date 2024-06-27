local lvgl = require("lvgl")

return {
  play = lvgl.ImgData("//lua/img/play.png"),
  pause = lvgl.ImgData("//lua/img/pause.png"),
  next = lvgl.ImgData("//lua/img/next.png"),
  prev = lvgl.ImgData("//lua/img/prev.png"),
  shuffle = lvgl.ImgData("//lua/img/shuffle.png"),
  repeat_src = lvgl.ImgData("//lua/img/repeat.png"), -- repeat is a reserved word
  queue = lvgl.ImgData("//lua/img/queue.png"),
  files = lvgl.ImgData("//lua/img/files.png"),
  settings = lvgl.ImgData("//lua/img/settings.png"),
}
