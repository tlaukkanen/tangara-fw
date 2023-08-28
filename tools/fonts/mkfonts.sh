#!/bin/sh
# To install this tool:
# npm i lv_font_conv -g
lv_font_conv \
  --font fonts/fusion/fusion-pixel-12px-proportional.ttf \
  -r 0x2000-0x206F \
  -r 0x20-0x7F,0xA0-0xFF \
  -r 0x3000-0x303f,0x3040-0x309F,0x30A0-0x30FF,0xFF00-0xFFEF,0x4E00-0x9FAF \
  --size 12 \
  --bpp 1 --format lvgl -o font_fusion.c

lv_font_conv \
  --font fonts/font-awesome/FontAwesome5-Solid+Brands+Regular.woff \
  -r 0xf244,0xf243,0xf242,0xf241,0xf240 `# battery indicators, empty->full` \
  -r 0xf104,0xf0d7 \
  -r 61441,61448,61451,61452,61452,61453,61457,61459,61461,61465 \
  -r 61468,61473,61478,61479,61480,61502,61512,61515,61516,61517 \
  -r 61521,61522,61523,61524,61543,61544,61550,61552,61553,61556 \
  -r 61559,61560,61561,61563,61587,61589,61636,61637,61639,61671 \
  -r 61674,61683,61724,61732,61787,61931,62016,62017,62018,62019 \
  -r 62020,62087,62099,62212,62189,62810,63426,63650 \
  --size 12 \
  --bpp 1 --format lvgl -o font_symbols.c
