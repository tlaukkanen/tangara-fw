#!/bin/sh
# To install this tool:
# npm i lv_font_conv -g
fusion_12() {
  lv_font_conv \
    --font fusion/fusion-pixel-12px-proportional.ttf \
    -r 0x2000-0x206F \
    -r 0x20-0x7F,0xA0-0xFF \
    -r 0x3000-0x303f,0x3040-0x309F,0x30A0-0x30FF,0xFF00-0xFFEF,0x4E00-0x9FAF \
    --size 12 \
    --bpp 1 --format lvgl -o font_fusion_12.c
  echo "finished fusion_12"
}

fusion_10() {
  lv_font_conv \
    --font fusion/fusion-pixel-10px-proportional/fusion-pixel-10px-proportional-latin.ttf \
    -r 0x2000-0x206F \
    -r 0x20-0x7F,0xA0-0xFF \
    --font fusion/fusion-pixel-10px-proportional/fusion-pixel-10px-proportional-ja.ttf \
    -r 0x3000-0x303f,0x3040-0x309F,0x30A0-0x30FF \
    -r 0xFF00-0xFFEF,0x4E00-0x9FAF \
    --size 10 \
    --bpp 1 --format lvgl -o font_fusion_10.c
  echo "finished fusion_10"
}

fusion_8() {
  lv_font_conv \
    --font fusion/fusion-pixel-8px-monospaced/fusion-pixel-8px-monospaced-latin.ttf \
    -r 0x2000-0x206F \
    -r 0x20-0x7F,0xA0-0xFF \
    --font fusion/fusion-pixel-8px-monospaced/fusion-pixel-8px-monospaced-ja.ttf \
    -r 0x3000-0x303f,0x3040-0x309F,0x30A0-0x30FF \
    -r 0xFF00-0xFFEF,0x4E00-0x9FAF \
    --size 8 \
    --bpp 1 --format lvgl -o font_fusion_8.c
  echo "finished fusion_8"
}

echo "creating all fonts"
fusion_12&
fusion_10&
fusion_8&
wait
