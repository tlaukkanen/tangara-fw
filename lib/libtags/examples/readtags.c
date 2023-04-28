#ifdef __unix__
#define _DEFAULT_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#define print printf
#define seek lseek
#define nil NULL
#define OREAD O_RDONLY
#define USED(x) (void)x
#else
#include <u.h>
#include <libc.h>
#endif
#include <tags.h>

typedef struct Aux Aux;

struct Aux
{
	int fd;
};

static const char *t2s[] =
{
	[Tartist] = "artist",
	[Talbum] = "album",
	[Ttitle] = "title",
	[Tdate] = "date",
	[Ttrack] = "track",
	[Talbumgain] = "albumgain",
	[Talbumpeak] = "albumpeak",
	[Ttrackgain] = "trackgain",
	[Ttrackpeak] = "trackpeak",
	[Tgenre] = "genre",
	[Timage] = "image",
};

static void
tag(Tagctx *ctx, int t, const char *k, const char *v, int offset, int size, Tagread f)
{
	USED(ctx); USED(k); USED(f);
	if(t == Timage)
		print("%-12s %s %d %d\n", t2s[t], v, offset, size);
	else if(t != Tunknown)
		print("%-12s %s\n", t2s[t], v);
}

static void
toc(Tagctx *ctx, int ms, int offset)
{
	USED(ctx); USED(ms); USED(offset);
}

static int
ctxread(Tagctx *ctx, void *buf, int cnt)
{
	Aux *aux = ctx->aux;
	return read(aux->fd, buf, cnt);
}

static int
ctxseek(Tagctx *ctx, int offset, int whence)
{
	Aux *aux = ctx->aux;
	return seek(aux->fd, offset, whence);
}

int
main(int argc, char **argv)
{
	int i;
	char buf[256];
	Aux aux;
	Tagctx ctx =
	{
		.read = ctxread,
		.seek = ctxseek,
		.tag = tag,
		.toc = toc,
		.buf = buf,
		.bufsz = sizeof(buf),
		.aux = &aux,
	};

	if(argc < 2){
		print("usage: readtags FILE...\n");
		return -1;
	}

	for(i = 1; i < argc; i++){
		print("*** %s\n", argv[i]);
		if((aux.fd = open(argv[i], OREAD)) < 0)
			print("failed to open\n");
		else{
			if(tagsget(&ctx) != 0)
				print("no tags or failed to read tags\n");
			else{
				if(ctx.duration > 0)
					print("%-12s %d ms\n", "duration", ctx.duration);
				if(ctx.samplerate > 0)
					print("%-12s %d\n", "samplerate", ctx.samplerate);
				if(ctx.channels > 0)
					print("%-12s %d\n", "channels", ctx.channels);
				if(ctx.bitrate > 0)
					print("%-12s %d\n", "bitrate", ctx.bitrate);
			}
			close(aux.fd);
		}
		print("\n");
	}
	return 0;
}
