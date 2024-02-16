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

 function: subsumed libogg includes

 ********************************************************************/
#ifndef _TREMOR_OGG_H
#define _TREMOR_OGG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "os_types.h"

typedef struct tremor_ogg_buffer_state{
  struct tremor_ogg_buffer    *unused_buffers;
  struct tremor_ogg_reference *unused_references;
  int                   outstanding;
  int                   shutdown;
} tremor_ogg_buffer_state;

typedef struct tremor_ogg_buffer {
  unsigned char      *data;
  long                size;
  int                 refcount;
  
  union {
    tremor_ogg_buffer_state  *owner;
    struct tremor_ogg_buffer *next;
  } ptr;
} tremor_ogg_buffer;

typedef struct tremor_ogg_reference {
  tremor_ogg_buffer    *buffer;
  long           begin;
  long           length;

  struct tremor_ogg_reference *next;
} tremor_ogg_reference;

typedef struct tremor_oggpack_buffer {
  int            headbit;
  unsigned char *headptr;
  long           headend;

  /* memory management */
  tremor_ogg_reference *head;
  tremor_ogg_reference *tail;

  /* render the byte/bit counter API constant time */
  long              count; /* doesn't count the tail */
} tremor_oggpack_buffer;

typedef struct oggbyte_buffer {
  tremor_ogg_reference *baseref;

  tremor_ogg_reference *ref;
  unsigned char *ptr;
  long           pos;
  long           end;
} oggbyte_buffer;

typedef struct tremor_ogg_sync_state {
  /* decode memory management pool */
  tremor_ogg_buffer_state *bufferpool;

  /* stream buffers */
  tremor_ogg_reference    *fifo_head;
  tremor_ogg_reference    *fifo_tail;
  long              fifo_fill;

  /* stream sync management */
  int               unsynced;
  int               headerbytes;
  int               bodybytes;

} tremor_ogg_sync_state;

typedef struct tremor_ogg_stream_state {
  tremor_ogg_reference *header_head;
  tremor_ogg_reference *header_tail;
  tremor_ogg_reference *body_head;
  tremor_ogg_reference *body_tail;

  int            e_o_s;    /* set when we have buffered the last
                              packet in the logical bitstream */
  int            b_o_s;    /* set after we've written the initial page
                              of a logical bitstream */
  long           serialno;
  long           pageno;
  tremor_ogg_int64_t    packetno; /* sequence number for decode; the framing
                              knows where there's a hole in the data,
                              but we need coupling so that the codec
                              (which is in a seperate abstraction
                              layer) also knows about the gap */
  tremor_ogg_int64_t    granulepos;

  int            lacing_fill;
  tremor_ogg_uint32_t   body_fill;

  /* decode-side state data */
  int            holeflag;
  int            spanflag;
  int            clearflag;
  int            laceptr;
  tremor_ogg_uint32_t   body_fill_next;
  
} tremor_ogg_stream_state;

typedef struct {
  tremor_ogg_reference *packet;
  long           bytes;
  long           b_o_s;
  long           e_o_s;
  tremor_ogg_int64_t    granulepos;
  tremor_ogg_int64_t    packetno;     /* sequence number for decode; the framing
                                  knows where there's a hole in the data,
                                  but we need coupling so that the codec
                                  (which is in a seperate abstraction
                                  layer) also knows about the gap */
} tremor_ogg_packet;

typedef struct {
  tremor_ogg_reference *header;
  int            header_len;
  tremor_ogg_reference *body;
  long           body_len;
} tremor_ogg_page;

/* Ogg BITSTREAM PRIMITIVES: bitstream ************************/

extern void  tremor_oggpack_readinit(tremor_oggpack_buffer *b,tremor_ogg_reference *r);
extern long  tremor_oggpack_look(tremor_oggpack_buffer *b,int bits);
extern void  tremor_oggpack_adv(tremor_oggpack_buffer *b,int bits);
extern long  tremor_oggpack_read(tremor_oggpack_buffer *b,int bits);
extern long  tremor_oggpack_bytes(tremor_oggpack_buffer *b);
extern long  tremor_oggpack_bits(tremor_oggpack_buffer *b);
extern int   tremor_oggpack_eop(tremor_oggpack_buffer *b);

/* Ogg BITSTREAM PRIMITIVES: decoding **************************/

extern tremor_ogg_sync_state *tremor_ogg_sync_create(void);
extern int      tremor_ogg_sync_destroy(tremor_ogg_sync_state *oy);
extern int      tremor_ogg_sync_reset(tremor_ogg_sync_state *oy);

extern unsigned char *tremor_ogg_sync_bufferin(tremor_ogg_sync_state *oy, long size);
extern int      tremor_ogg_sync_wrote(tremor_ogg_sync_state *oy, long bytes);
extern long     tremor_ogg_sync_pageseek(tremor_ogg_sync_state *oy,tremor_ogg_page *og);
extern int      tremor_ogg_sync_pageout(tremor_ogg_sync_state *oy, tremor_ogg_page *og);
extern int      tremor_ogg_stream_pagein(tremor_ogg_stream_state *os, tremor_ogg_page *og);
extern int      tremor_ogg_stream_packetout(tremor_ogg_stream_state *os,tremor_ogg_packet *op);
extern int      tremor_ogg_stream_packetpeek(tremor_ogg_stream_state *os,tremor_ogg_packet *op);

/* Ogg BITSTREAM PRIMITIVES: general ***************************/

extern tremor_ogg_stream_state *tremor_ogg_stream_create(int serialno);
extern int      tremor_ogg_stream_destroy(tremor_ogg_stream_state *os);
extern int      tremor_ogg_stream_reset(tremor_ogg_stream_state *os);
extern int      tremor_ogg_stream_reset_serialno(tremor_ogg_stream_state *os,int serialno);
extern int      tremor_ogg_stream_eos(tremor_ogg_stream_state *os);

extern int      tremor_ogg_page_checksum_set(tremor_ogg_page *og);

extern int      tremor_ogg_page_version(tremor_ogg_page *og);
extern int      tremor_ogg_page_continued(tremor_ogg_page *og);
extern int      tremor_ogg_page_bos(tremor_ogg_page *og);
extern int      tremor_ogg_page_eos(tremor_ogg_page *og);
extern tremor_ogg_int64_t  tremor_ogg_page_granulepos(tremor_ogg_page *og);
extern tremor_ogg_uint32_t tremor_ogg_page_serialno(tremor_ogg_page *og);
extern tremor_ogg_uint32_t tremor_ogg_page_pageno(tremor_ogg_page *og);
extern int      tremor_ogg_page_packets(tremor_ogg_page *og);
extern int      tremor_ogg_page_getbuffer(tremor_ogg_page *og, unsigned char **buffer);

extern int      tremor_ogg_packet_release(tremor_ogg_packet *op);
extern int      tremor_ogg_page_release(tremor_ogg_page *og);

extern void     tremor_ogg_page_dup(tremor_ogg_page *d, tremor_ogg_page *s);

/* Ogg BITSTREAM PRIMITIVES: return codes ***************************/

#define  OGG_SUCCESS   0

#define  OGG_HOLE     -10
#define  OGG_SPAN     -11
#define  OGG_EVERSION -12
#define  OGG_ESERIAL  -13
#define  OGG_EINVAL   -14
#define  OGG_EEOS     -15


#ifdef __cplusplus
}
#endif

#endif  /* _TREMOR_OGG_H */
