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

#ifndef LV_ATTRIBUTE_IMG_BATTERY_40
#define LV_ATTRIBUTE_IMG_BATTERY_40
#endif

static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_BATTERY_40 uint8_t battery_40_map[] = {
  0x00, 0x00, 0x00, 0xff, 	/*Color of index 0*/
  0xfd, 0xfe, 0xfd, 0xff, 	/*Color of index 1*/
  0x26, 0xc1, 0x38, 0xff, 	/*Color of index 2*/
  0x01, 0xbe, 0x37, 0xff, 	/*Color of index 3*/

  0x55, 0x00, 0x55, 
  0x54, 0x00, 0x15, 
  0x54, 0x55, 0x15, 
  0x54, 0x55, 0x15, 
  0x54, 0x55, 0x15, 
  0x54, 0x55, 0x15, 
  0x54, 0x55, 0x15, 
  0x54, 0x55, 0x15, 
  0x54, 0xaa, 0x15, 
  0x54, 0xff, 0x15, 
  0x54, 0xff, 0x15, 
  0x54, 0x00, 0x15, 
};

const lv_img_dsc_t kIconBattery40 = {
  .header.cf = LV_IMG_CF_INDEXED_2BIT,
  .header.always_zero = 0,
  .header.reserved = 0,
  .header.w = 12,
  .header.h = 12,
  .data_size = 52,
  .data = battery_40_map,
};
