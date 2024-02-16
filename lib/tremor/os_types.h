/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE TremorOggVorbis 'TREMOR' CODEC SOURCE CODE.   *
 *                                                                  *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE TremorOggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2002    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: #ifdef jail to whip a few platforms into the UNIX ideal.

 ********************************************************************/
#ifndef _TREMOR_OS_TYPES_H
#define _TREMOR_OS_TYPES_H

#include <stdint.h>
#ifdef _LOW_ACCURACY_
#  define X(n) (((((n)>>22)+1)>>1) - ((((n)>>22)+1)>>9))
#  define LOOKUP_T const unsigned char
#else
#  define X(n) (n)
#  define LOOKUP_T const tremor_ogg_int32_t
#endif

/* make it easy on the folks that want to compile the libs with a
   different malloc than stdlib */
#define _tremor_ogg_malloc  malloc
#define _tremor_ogg_calloc  calloc
#define _tremor_ogg_realloc realloc
#define _tremor_ogg_free    free

typedef int64_t tremor_ogg_int64_t;
typedef int32_t tremor_ogg_int32_t;
typedef uint32_t tremor_ogg_uint32_t;
typedef int16_t tremor_ogg_int16_t;
typedef uint16_t tremor_ogg_uint16_t;

#endif  /* _TREMOR_OS_TYPES_H */
