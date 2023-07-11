/*******************************************************************************
 * Size: 10 px
 * Bpp: 1
 * Opts: --font fonts/font-awesome/FontAwesome5-Solid+Brands+Regular.woff -r 61441,61448,61451,61452,61452,61453,61457,61459,61461,61465 -r 61468,61473,61478,61479,61480,61502,61512,61515,61516,61517 -r 61521,61522,61523,61524,61543,61544,61550,61552,61553,61556 -r 61559,61560,61561,61563,61587,61589,61636,61637,61639,61671 -r 61674,61683,61724,61732,61787,61931,62016,62017,62018,62019 -r 62020,62087,62099,62212,62189,62810,63426,63650 --size 10 --bpp 1 --format lvgl -o font_symbols.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef FONT_SYMBOLS
#define FONT_SYMBOLS 1
#endif

#if FONT_SYMBOLS

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+F001 "" */
    0x0, 0x40, 0xf1, 0xfc, 0x7d, 0x18, 0x44, 0x11,
    0x4, 0x47, 0x71, 0xdc, 0x0,

    /* U+F008 "" */
    0xbf, 0x78, 0x7e, 0x1e, 0xfd, 0xe1, 0xe8, 0x5f,
    0xfc,

    /* U+F00B "" */
    0xef, 0xfb, 0xf0, 0x3, 0xbf, 0xef, 0xc0, 0xe,
    0xff, 0xbf,

    /* U+F00C "" */
    0x0, 0xc0, 0x64, 0x33, 0x98, 0x7c, 0xe, 0x1,
    0x0,

    /* U+F00D "" */
    0xc7, 0xdd, 0xf1, 0xc7, 0xdd, 0xf1, 0x80,

    /* U+F011 "" */
    0xc, 0xb, 0x46, 0xdb, 0x33, 0xcc, 0xf0, 0x3c,
    0x1d, 0x86, 0x3f, 0x0,

    /* U+F013 "" */
    0xc, 0xe, 0x1f, 0xff, 0xf6, 0x33, 0x1b, 0xdf,
    0xff, 0x5c, 0xe, 0x0,

    /* U+F015 "" */
    0x4, 0x83, 0x70, 0xd6, 0x37, 0x6d, 0xf6, 0x7f,
    0xe, 0xe1, 0xdc, 0x3b, 0x80,

    /* U+F019 "" */
    0xe, 0x1, 0xc0, 0x38, 0x1f, 0xc1, 0xf0, 0x1c,
    0x3d, 0x7f, 0xfd, 0xff, 0xe0,

    /* U+F01C "" */
    0x3f, 0x8c, 0x19, 0x1, 0x60, 0x3f, 0x1f, 0xff,
    0xff, 0xf8,

    /* U+F021 "" */
    0x0, 0x47, 0xd6, 0x1d, 0xf, 0x0, 0x0, 0xf,
    0xb, 0x6, 0xe3, 0x27, 0x80,

    /* U+F026 "" */
    0x8, 0xff, 0xff, 0xfc, 0x61,

    /* U+F027 "" */
    0x8, 0x18, 0xf8, 0xf9, 0xf9, 0xfa, 0x18, 0x8,

    /* U+F028 "" */
    0x8, 0x83, 0x2b, 0xe2, 0xfc, 0xdf, 0x9b, 0xf5,
    0x46, 0x70, 0x42, 0x0, 0x80,

    /* U+F03E "" */
    0xff, 0xe7, 0xf9, 0xef, 0xf1, 0xc8, 0x60, 0x1f,
    0xfc,

    /* U+F048 "" */
    0x86, 0x7b, 0xff, 0xff, 0xfb, 0xe7, 0x84,

    /* U+F04B "" */
    0xc0, 0x70, 0x3e, 0x1f, 0xcf, 0xff, 0xff, 0xfd,
    0xf8, 0xf0, 0x70, 0x0,

    /* U+F04C "" */
    0xf7, 0xfb, 0xfd, 0xfe, 0xff, 0x7f, 0xbf, 0xdf,
    0xef, 0xf7, 0x80,

    /* U+F04D "" */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0x80,

    /* U+F051 "" */
    0x87, 0x9f, 0x7f, 0xff, 0xff, 0x79, 0x84,

    /* U+F052 "" */
    0x18, 0x1e, 0x1f, 0x8f, 0xef, 0xf8, 0x3, 0xff,
    0xff,

    /* U+F053 "" */
    0x19, 0x99, 0x8c, 0x30, 0xc3,

    /* U+F054 "" */
    0x86, 0x18, 0x63, 0xb3, 0x10,

    /* U+F067 "" */
    0x8, 0x4, 0x2, 0x1, 0xf, 0xf8, 0x40, 0x20,
    0x10, 0x8, 0x0,

    /* U+F068 "" */
    0xff, 0xff, 0xc0,

    /* U+F06E "" */
    0x1f, 0x6, 0x31, 0x9b, 0x73, 0x7e, 0xee, 0xdd,
    0x8c, 0x60, 0xf8,

    /* U+F070 "" */
    0xc0, 0x1, 0xdf, 0x1, 0xc6, 0x3, 0x4c, 0x67,
    0xb9, 0xc6, 0xe3, 0xf, 0x6, 0x18, 0xf, 0x38,
    0x0, 0x30,

    /* U+F071 "" */
    0x4, 0x1, 0xc0, 0x3c, 0xd, 0x83, 0xb8, 0x77,
    0x1f, 0xf3, 0xdf, 0xfb, 0xff, 0xfc,

    /* U+F074 "" */
    0x1, 0xb8, 0xf3, 0x58, 0x30, 0x18, 0xd, 0xee,
    0x3c, 0x6,

    /* U+F077 "" */
    0x1c, 0x1f, 0x1d, 0xdc, 0x7c, 0x18,

    /* U+F078 "" */
    0xc1, 0xb1, 0x8d, 0x83, 0x80, 0x80,

    /* U+F079 "" */
    0x20, 0x3, 0x9f, 0x3e, 0x8, 0x40, 0x42, 0x2,
    0x10, 0x7c, 0xf9, 0xc0, 0x4,

    /* U+F07B "" */
    0xf8, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xfc,

    /* U+F093 "" */
    0xc, 0x7, 0x83, 0xf0, 0xfc, 0xc, 0x3, 0xe,
    0xdf, 0xfd, 0xff, 0xc0,

    /* U+F095 "" */
    0x1, 0x80, 0x70, 0x3c, 0x7, 0x1, 0x80, 0x62,
    0x33, 0xf8, 0xfc, 0x3e, 0x0,

    /* U+F0C4 "" */
    0x0, 0x79, 0xe5, 0xcf, 0xc1, 0xc3, 0xe3, 0xd9,
    0x26, 0x61, 0x0,

    /* U+F0C5 "" */
    0x1d, 0xe, 0xf7, 0x1b, 0xfd, 0xfe, 0xff, 0x7f,
    0xbf, 0xc0, 0x7e, 0x0,

    /* U+F0C7 "" */
    0xfe, 0x41, 0xa0, 0xf0, 0x7f, 0xff, 0x3f, 0x9f,
    0xff,

    /* U+F0E7 "" */
    0x73, 0xcf, 0x3f, 0xfc, 0x63, 0xc, 0x20,

    /* U+F0EA "" */
    0x10, 0x76, 0x3f, 0x1c, 0xe, 0xd7, 0x67, 0xbf,
    0xdf, 0xf, 0x87, 0xc0,

    /* U+F0F3 "" */
    0x8, 0xe, 0xf, 0x8f, 0xe7, 0xf3, 0xf9, 0xfd,
    0xff, 0x0, 0xe, 0x0,

    /* U+F11C "" */
    0xff, 0xf5, 0x57, 0xff, 0xea, 0xbf, 0xff, 0x41,
    0x7f, 0xf8,

    /* U+F124 "" */
    0x0, 0xc0, 0xf0, 0xf8, 0xfe, 0xff, 0xbf, 0xc0,
    0xf0, 0x38, 0xe, 0x3, 0x0,

    /* U+F15B "" */
    0xf4, 0xf6, 0xf1, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff,

    /* U+F1EB "" */
    0x1f, 0xc3, 0xef, 0xb8, 0xe, 0x3f, 0x3, 0xc,
    0x0, 0x0, 0x18, 0x0, 0xc0,

    /* U+F240 "" */
    0xff, 0xe8, 0x3, 0xbf, 0xdb, 0xfd, 0x80, 0x3f,
    0xff,

    /* U+F241 "" */
    0xff, 0xe8, 0x3, 0xbf, 0x1b, 0xf1, 0x80, 0x3f,
    0xff,

    /* U+F242 "" */
    0xff, 0xe8, 0x3, 0xbc, 0x1b, 0xc1, 0x80, 0x3f,
    0xff,

    /* U+F243 "" */
    0xff, 0xe8, 0x3, 0xb0, 0x1b, 0x1, 0x80, 0x3f,
    0xff,

    /* U+F244 "" */
    0xff, 0xf8, 0x3, 0x80, 0x18, 0x1, 0x80, 0x3f,
    0xff,

    /* U+F287 "" */
    0x1, 0x0, 0x3c, 0x2, 0x1, 0xd0, 0x2f, 0xff,
    0xf2, 0x8, 0xb, 0x0, 0x38,

    /* U+F293 "" */
    0x3c, 0x76, 0xf3, 0xdb, 0xe7, 0xe7, 0xdb, 0xf3,
    0x76, 0x3c,

    /* U+F2ED "" */
    0x1c, 0x7f, 0xc0, 0xf, 0xe5, 0x52, 0xa9, 0x54,
    0xaa, 0x55, 0x3f, 0x80,

    /* U+F304 "" */
    0x1, 0x80, 0xf0, 0x5c, 0x3a, 0x1f, 0xf, 0x87,
    0xc3, 0xe0, 0xf0, 0x38, 0x0,

    /* U+F55A "" */
    0x1f, 0xf9, 0xed, 0xdf, 0xf, 0xfc, 0xf7, 0xc3,
    0x9e, 0xdc, 0x7f, 0xe0,

    /* U+F7C2 "" */
    0x3e, 0xb7, 0x6f, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xfc,

    /* U+F8A2 "" */
    0x0, 0x48, 0x36, 0xf, 0xff, 0x60, 0x8, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 13, .adv_w = 160, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 22, .adv_w = 160, .box_w = 10, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 32, .adv_w = 160, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 41, .adv_w = 110, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 48, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 60, .adv_w = 160, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 72, .adv_w = 180, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 85, .adv_w = 160, .box_w = 11, .box_h = 9, .ofs_x = -1, .ofs_y = -1},
    {.bitmap_index = 98, .adv_w = 180, .box_w = 11, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 108, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 121, .adv_w = 80, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 126, .adv_w = 120, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 134, .adv_w = 180, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 147, .adv_w = 160, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 156, .adv_w = 140, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 163, .adv_w = 140, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 175, .adv_w = 140, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 186, .adv_w = 140, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 197, .adv_w = 140, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 204, .adv_w = 140, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 213, .adv_w = 100, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 218, .adv_w = 100, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 223, .adv_w = 140, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 234, .adv_w = 140, .box_w = 9, .box_h = 2, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 237, .adv_w = 180, .box_w = 11, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 248, .adv_w = 200, .box_w = 14, .box_h = 10, .ofs_x = -1, .ofs_y = -1},
    {.bitmap_index = 266, .adv_w = 180, .box_w = 11, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 280, .adv_w = 160, .box_w = 10, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 290, .adv_w = 140, .box_w = 9, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 296, .adv_w = 140, .box_w = 9, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 302, .adv_w = 200, .box_w = 13, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 315, .adv_w = 160, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 324, .adv_w = 160, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 336, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 349, .adv_w = 140, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 360, .adv_w = 140, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 372, .adv_w = 140, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 381, .adv_w = 100, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 388, .adv_w = 140, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 400, .adv_w = 140, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 412, .adv_w = 180, .box_w = 11, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 422, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 435, .adv_w = 120, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 445, .adv_w = 200, .box_w = 13, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 458, .adv_w = 200, .box_w = 12, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 467, .adv_w = 200, .box_w = 12, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 476, .adv_w = 200, .box_w = 12, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 485, .adv_w = 200, .box_w = 12, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 494, .adv_w = 200, .box_w = 12, .box_h = 6, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 503, .adv_w = 200, .box_w = 13, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 516, .adv_w = 140, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 526, .adv_w = 140, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 538, .adv_w = 160, .box_w = 10, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 551, .adv_w = 200, .box_w = 13, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 563, .adv_w = 120, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 572, .adv_w = 161, .box_w = 10, .box_h = 6, .ofs_x = 0, .ofs_y = 1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0x7, 0xa, 0xb, 0xc, 0x10, 0x12, 0x14,
    0x18, 0x1b, 0x20, 0x25, 0x26, 0x27, 0x3d, 0x47,
    0x4a, 0x4b, 0x4c, 0x50, 0x51, 0x52, 0x53, 0x66,
    0x67, 0x6d, 0x6f, 0x70, 0x73, 0x76, 0x77, 0x78,
    0x7a, 0x92, 0x94, 0xc3, 0xc4, 0xc6, 0xe6, 0xe9,
    0xf2, 0x11b, 0x123, 0x15a, 0x1ea, 0x23f, 0x240, 0x241,
    0x242, 0x243, 0x286, 0x292, 0x2ec, 0x303, 0x559, 0x7c1,
    0x8a1
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 61441, .range_length = 2210, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 57, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LV_VERSION_CHECK(8, 0, 0)
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LV_VERSION_CHECK(8, 0, 0)
    .cache = &cache
#endif
};


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LV_VERSION_CHECK(8, 0, 0)
const lv_font_t font_symbols = {
#else
lv_font_t font_symbols = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 10,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -4,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc           /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
};



#endif /*#if FONT_SYMBOLS*/

