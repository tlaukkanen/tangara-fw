#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif


#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMG_PLAY
#define LV_ATTRIBUTE_IMG_PLAY
#endif

static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_PLAY uint8_t play_map[] = {
  0xfe, 0xfe, 0xfe, 0xff, 	/*Color of index 0*/
  0x00, 0x00, 0x00, 0xff, 	/*Color of index 1*/
  0x00, 0x00, 0x00, 0x00, 	/*Color of index 2*/
  0x00, 0x00, 0x00, 0x00, 	/*Color of index 3*/

  0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 
  0x00, 0x50, 0x00, 0x00, 
  0x00, 0x55, 0x00, 0x00, 
  0x00, 0x55, 0x50, 0x00, 
  0x00, 0x55, 0x54, 0x00, 
  0x00, 0x55, 0x54, 0x00, 
  0x00, 0x55, 0x50, 0x00, 
  0x00, 0x55, 0x00, 0x00, 
  0x00, 0x50, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 
};

const lv_img_dsc_t kIconPlay = {
  .header.cf = LV_IMG_CF_INDEXED_2BIT,
  .header.always_zero = 0,
  .header.reserved = 0,
  .header.w = 14,
  .header.h = 14,
  .data_size = 72,
  .data = play_map,
};
