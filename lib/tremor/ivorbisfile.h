/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE TremorOggVorbis 'TREMOR' CODEC SOURCE CODE.   *
 *                                                                  *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE TremorOggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2003    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: stdio-based convenience library for opening/seeking/decoding

 ********************************************************************/

#ifndef _OV_FILE_H_
#define _OV_FILE_H_

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <stdio.h>
#include "ivorbiscodec.h"

/* The function prototypes for the callbacks are basically the same as for
 * the stdio functions fread, fseek, fclose, ftell. 
 * The one difference is that the FILE * arguments have been replaced with
 * a void * - this is to be used as a pointer to whatever internal data these
 * functions might need. In the stdio case, it's just a FILE * cast to a void *
 * 
 * If you use other functions, check the docs for these functions and return
 * the right values. For seek_func(), you *MUST* return -1 if the stream is
 * unseekable
 */
typedef struct {
  size_t (*read_func)  (void *ptr, size_t size, size_t nmemb, void *datasource);
  int    (*seek_func)  (void *datasource, tremor_ogg_int64_t offset, int whence);
  int    (*close_func) (void *datasource);
  long   (*tell_func)  (void *datasource);
} ov_callbacks;

typedef struct TremorOggVorbis_File {
  void            *datasource; /* Pointer to a FILE *, etc. */
  int              seekable;
  tremor_ogg_int64_t      offset;
  tremor_ogg_int64_t      end;
  tremor_ogg_sync_state   *oy; 

  /* If the FILE handle isn't seekable (eg, a pipe), only the current
     stream appears */
  int              links;
  tremor_ogg_int64_t     *offsets;
  tremor_ogg_int64_t     *dataoffsets;
  tremor_ogg_uint32_t    *serialnos;
  tremor_ogg_int64_t     *pcmlengths;
  vorbis_info     vi;
  vorbis_comment  vc;

  /* Decoding working state local storage */
  tremor_ogg_int64_t      pcm_offset;
  int              ready_state;
  tremor_ogg_uint32_t     current_serialno;
  int              current_link;

  tremor_ogg_int64_t      bittrack;
  tremor_ogg_int64_t      samptrack;

  tremor_ogg_stream_state *os; /* take physical pages, weld into a logical
                          stream of packets */
  vorbis_dsp_state *vd; /* central working state for the packet->PCM decoder */

  ov_callbacks callbacks;

} TremorOggVorbis_File;

extern int ov_clear(TremorOggVorbis_File *vf);
extern int ov_open(FILE *f,TremorOggVorbis_File *vf,char *initial,long ibytes);
extern int ov_open_callbacks(void *datasource, TremorOggVorbis_File *vf,
		char *initial, long ibytes, ov_callbacks callbacks);

extern int ov_test(FILE *f,TremorOggVorbis_File *vf,char *initial,long ibytes);
extern int ov_test_callbacks(void *datasource, TremorOggVorbis_File *vf,
		char *initial, long ibytes, ov_callbacks callbacks);
extern int ov_test_open(TremorOggVorbis_File *vf);

extern long ov_bitrate(TremorOggVorbis_File *vf,int i);
extern long ov_bitrate_instant(TremorOggVorbis_File *vf);
extern long ov_streams(TremorOggVorbis_File *vf);
extern long ov_seekable(TremorOggVorbis_File *vf);
extern long ov_serialnumber(TremorOggVorbis_File *vf,int i);

extern tremor_ogg_int64_t ov_raw_total(TremorOggVorbis_File *vf,int i);
extern tremor_ogg_int64_t ov_pcm_total(TremorOggVorbis_File *vf,int i);
extern tremor_ogg_int64_t ov_time_total(TremorOggVorbis_File *vf,int i);

extern int ov_raw_seek(TremorOggVorbis_File *vf,tremor_ogg_int64_t pos);
extern int ov_pcm_seek(TremorOggVorbis_File *vf,tremor_ogg_int64_t pos);
extern int ov_pcm_seek_page(TremorOggVorbis_File *vf,tremor_ogg_int64_t pos);
extern int ov_time_seek(TremorOggVorbis_File *vf,tremor_ogg_int64_t pos);
extern int ov_time_seek_page(TremorOggVorbis_File *vf,tremor_ogg_int64_t pos);

extern tremor_ogg_int64_t ov_raw_tell(TremorOggVorbis_File *vf);
extern tremor_ogg_int64_t ov_pcm_tell(TremorOggVorbis_File *vf);
extern tremor_ogg_int64_t ov_time_tell(TremorOggVorbis_File *vf);

extern vorbis_info *ov_info(TremorOggVorbis_File *vf,int link);
extern vorbis_comment *ov_comment(TremorOggVorbis_File *vf,int link);

extern long ov_read(TremorOggVorbis_File *vf,void *buffer,int length,
		    int *bitstream);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif


