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

#ifndef LV_ATTRIBUTE_IMG_BATTERY_EMPTY
#define LV_ATTRIBUTE_IMG_BATTERY_EMPTY
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMG_BATTERY_EMPTY uint8_t battery_empty_map[] = {
  0xfd, 0xfd, 0xfd, 0xff, 	/*Color of index 0*/
  0x00, 0x00, 0x00, 0xff, 	/*Color of index 1*/
  0x26, 0x2c, 0xfa, 0xff, 	/*Color of index 2*/
  0x00, 0x00, 0x00, 0x00, 	/*Color of index 3*/

  0x00, 0x55, 0x00, 
  0x01, 0x55, 0x40, 
  0x01, 0x00, 0x40, 
  0x01, 0x00, 0x40, 
  0x01, 0x00, 0x40, 
  0x01, 0x00, 0x40, 
  0x01, 0x00, 0x40, 
  0x01, 0x00, 0x40, 
  0x01, 0x00, 0x40, 
  0x01, 0xaa, 0x40, 
  0x01, 0xaa, 0x40, 
  0x01, 0x55, 0x40, 
};

const lv_img_dsc_t kIconBatteryEmpty = {
  .header.cf = LV_IMG_CF_INDEXED_2BIT,
  .header.always_zero = 0,
  .header.reserved = 0,
  .header.w = 12,
  .header.h = 12,
  .data_size = 52,
  .data = battery_empty_map,
};
