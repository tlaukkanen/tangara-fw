#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include <stdint.h>
#define snprint snprintf
#define cistrcmp strcasecmp
#define cistrncmp strncasecmp
#define nil NULL
#define UTFmax 4
#define nelem(x) (int)(sizeof(x)/sizeof((x)[0]))
typedef uint8_t uchar;
typedef uint16_t u16int;
typedef uint32_t u32int;
typedef uint64_t uvlong;
typedef unsigned int uint;
typedef unsigned short ushort;
#include "tags.h"

enum
{
	Numgenre = 192,
};

#define beuint(d) (uint)(((uchar*)(d))[0]<<24 | ((uchar*)(d))[1]<<16 | ((uchar*)(d))[2]<<8 | ((uchar*)(d))[3]<<0)
#define leuint(d) (uint)(((uchar*)(d))[3]<<24 | ((uchar*)(d))[2]<<16 | ((uchar*)(d))[1]<<8 | ((uchar*)(d))[0]<<0)

extern const char *id3genres[Numgenre];

/*
 * Converts (to UTF-8) at most sz bytes of src and writes it to out buffer.
 * Returns the number of bytes converted.
 * You need sz*2+1 bytes for out buffer to be completely safe.
 */
int iso88591toutf8(uchar *out, int osz, const uchar *src, int sz);

/*
 * Converts (to UTF-8) at most sz bytes of src and writes it to out buffer.
 * Returns the number of bytes converted or < 0 in case of error.
 * You need sz*4+1 bytes for out buffer to be completely safe.
 * UTF-16 defaults to big endian if there is no BOM.
 */
int utf16to8(uchar *out, int osz, const uchar *src, int sz);

/*
 * Same as utf16to8, but CP437 to UTF-8.
 */
int cp437toutf8(char *o, int osz, const char *s, int sz);

/*
 * This one is common for both vorbis.c and flac.c
 * It maps a string k to tag type and executes the callback from ctx.
 * Returns 1 if callback was called, 0 otherwise.
 */
void cbvorbiscomment(Tagctx *ctx, char *k, char *v);

void tagscallcb(Tagctx *ctx, int type, const char *k, char *s, int offset, int size, Tagread f);

#define txtcb(ctx, type, k, s) tagscallcb(ctx, type, k, (char*)s, 0, 0, nil)
