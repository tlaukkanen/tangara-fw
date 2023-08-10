/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "tag_parser.hpp"

#include <esp_log.h>
#include <ff.h>
#include <stdint.h>
#include <tags.h>
#include <cstdlib>
#include <iomanip>
#include <mutex>

namespace database {

auto convert_tag(int tag) -> std::optional<Tag> {
  switch (tag) {
    case Ttitle:
      return Tag::kTitle;
    case Tartist:
      return Tag::kArtist;
    case Talbum:
      return Tag::kAlbum;
    case Ttrack:
      return Tag::kAlbumTrack;
    case Tgenre:
      return Tag::kGenre;
    default:
      return {};
  }
}

namespace libtags {

struct Aux {
  FIL file;
  FILINFO info;
  TrackTags* tags;
};

static int read(Tagctx* ctx, void* buf, int cnt) {
  Aux* aux = reinterpret_cast<Aux*>(ctx->aux);
  if (f_eof(&aux->file)) {
    return 0;
  }
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
  if (res != FR_OK) {
    return -1;
  }
  return aux->file.fptr;
}

static void tag(Tagctx* ctx,
                int t,
                const char* k,
                const char* v,
                int offset,
                int size,
                Tagread f) {
  Aux* aux = reinterpret_cast<Aux*>(ctx->aux);
  auto tag = convert_tag(t);
  if (tag) {
    std::string value{v};
    if (*tag == Tag::kAlbumTrack) {
      uint32_t as_int = std::atoi(v);
      std::ostringstream oss;
      oss << std::setw(4) << std::setfill('0') << as_int;
      value = oss.str();
    }
    aux->tags->set(*tag, value);
  }
}

static void toc(Tagctx* ctx, int ms, int offset) {}

}  // namespace libtags

static const std::size_t kBufSize = 1024;
static const char* kTag = "TAGS";

auto TagParserImpl::ReadAndParseTags(const std::string& path, TrackTags* out)
    -> bool {
  {
    std::lock_guard<std::mutex> lock{cache_mutex_};
    std::optional<TrackTags> cached = cache_.Get(path);
    if (cached) {
      *out = *cached;
      return true;
    }
  }

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
      out->encoding(Container::kMp3);
      break;
    case Fogg:
      out->encoding(Container::kOgg);
      break;
    case Fflac:
      out->encoding(Container::kFlac);
      break;
    case Fwav:
      out->encoding(Container::kWav);
      break;
    case Fopus:
      out->encoding(Container::kOpus);
      break;
    default:
      out->encoding(Container::kUnsupported);
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
  if (ctx.duration > 0) {
    out->duration = ctx.duration;
  }

  {
    std::lock_guard<std::mutex> lock{cache_mutex_};
    cache_.Put(path, *out);
  }
  return true;
}

}  // namespace database
