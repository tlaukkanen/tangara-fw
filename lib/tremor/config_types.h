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
#ifndef _OS_CVTYPES_H
#define _OS_CVTYPES_H

typedef long long tremor_ogg_int64_t;
typedef int tremor_ogg_int32_t;
typedef unsigned int tremor_ogg_uint32_t;
typedef short tremor_ogg_int16_t;
typedef unsigned short tremor_ogg_uint16_t;

#endif
