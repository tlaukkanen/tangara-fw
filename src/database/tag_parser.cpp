/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "tag_parser.hpp"

#include <esp_log.h>
#include <ff.h>
#include <tags.h>

namespace database {

namespace libtags {

struct Aux {
  FIL file;
  FILINFO info;
  TrackTags* tags;
};

static int read(Tagctx* ctx, void* buf, int cnt) {
  Aux* aux = reinterpret_cast<Aux*>(ctx->aux);
  UINT bytes_read;
  if (f_read(&aux->file, buf, cnt, &bytes_read) != FR_OK) {
    return -1;
  }
  return bytes_read;
}

static int seek(Tagctx* ctx, int offset, int whence) {
  Aux* aux = reinterpret_cast<Aux*>(ctx->aux);
  FRESULT res;
  if (whence == 0) {
    // Seek from the start of the file. This is f_lseek's behaviour.
    res = f_lseek(&aux->file, offset);
  } else if (whence == 1) {
    // Seek from current offset.
    res = f_lseek(&aux->file, aux->file.fptr + offset);
  } else if (whence == 2) {
    // Seek from the end of the file
    res = f_lseek(&aux->file, aux->info.fsize + offset);
  } else {
    return -1;
  }
  return res;
}

static void tag(Tagctx* ctx,
                int t,
                const char* k,
                const char* v,
                int offset,
                int size,
                Tagread f) {
  Aux* aux = reinterpret_cast<Aux*>(ctx->aux);
  if (t == Ttitle) {
    aux->tags->title = v;
  } else if (t == Tartist) {
    aux->tags->artist = v;
  } else if (t == Talbum) {
    aux->tags->album = v;
  }
}

static void toc(Tagctx* ctx, int ms, int offset) {}

}  // namespace libtags

static const std::size_t kBufSize = 1024;
static const char* kTag = "TAGS";

auto TagParserImpl::ReadAndParseTags(const std::string& path, TrackTags* out)
    -> bool {
  libtags::Aux aux;
  aux.tags = out;
  if (f_stat(path.c_str(), &aux.info) != FR_OK ||
      f_open(&aux.file, path.c_str(), FA_READ) != FR_OK) {
    ESP_LOGW(kTag, "failed to open file %s", path.c_str());
    return false;
  }
  // Fine to have this on the stack; this is only called on tasks with large
  // stacks anyway, due to all the string handling.
  char buf[kBufSize];
  Tagctx ctx;
  ctx.read = libtags::read;
  ctx.seek = libtags::seek;
  ctx.tag = libtags::tag;
  ctx.toc = libtags::toc;
  ctx.aux = &aux;
  ctx.buf = buf;
  ctx.bufsz = kBufSize;
  int res = tagsget(&ctx);
  f_close(&aux.file);

  if (res != 0) {
    // Parsing failed.
    ESP_LOGE(kTag, "tag parsing failed, reason %d", res);
    return false;
  }

  switch (ctx.format) {
    case Fmp3:
      out->encoding = Encoding::kMp3;
      break;
    case Fogg:
      out->encoding = Encoding::kOgg;
      break;
    case Fflac:
      out->encoding = Encoding::kFlac;
      break;
    case Fwav:
      out->encoding = Encoding::kWav;
      break;
    default:
      out->encoding = Encoding::kUnsupported;
  }

  if (ctx.channels > 0) {
    out->channels = ctx.channels;
  }
  if (ctx.samplerate > 0) {
    out->sample_rate = ctx.samplerate;
  }
  if (ctx.bitrate > 0) {
    out->bits_per_sample = ctx.bitrate;
  }

  return true;
}

}  // namespace database
