/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "database/tag_parser.hpp"

#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <mutex>

#include "drivers/spi.hpp"
#include "esp_log.h"
#include "ff.h"
#include "tags.h"

#include "memory_resource.hpp"

namespace database {

static auto convert_tag(int tag) -> std::optional<Tag> {
  switch (tag) {
    case Ttitle:
      return Tag::kTitle;
    case Tartist:
      return Tag::kArtist;
    case Talbumartist:
      return Tag::kAlbumArtist;
    case Talbum:
      return Tag::kAlbum;
    case Ttrack:
      return Tag::kTrack;
    case Tgenre:
      return Tag::kGenres;
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
  if (!tag) {
    return;
  }
  std::pmr::string value{v, &memory::kSpiRamResource};
  if (value.empty()) {
    return;
  }
  aux->tags->set(*tag, value);
}

static void toc(Tagctx* ctx, int ms, int offset) {}

}  // namespace libtags

static const std::size_t kBufSize = 1024;
[[maybe_unused]] static const char* kTag = "TAGS";

TagParserImpl::TagParserImpl() {}

auto TagParserImpl::ReadAndParseTags(std::string_view path)
    -> std::shared_ptr<TrackTags> {
  {
    std::lock_guard<std::mutex> lock{cache_mutex_};
    std::optional<std::shared_ptr<TrackTags>> cached =
        cache_.Get({path.data(), path.size()});
    if (cached) {
      return *cached;
    }
  }

  std::shared_ptr<TrackTags> tags = parseNew(path);
  if (!tags) {
    return {};
  }

  // There wasn't a track number found in the track's tags. Try to synthesize
  // one from the filename, which will sometimes have a track number at the
  // start.
  if (!tags->track()) {
    auto slash_pos = path.find_last_of("/");
    if (slash_pos != std::string::npos && path.size() - slash_pos > 1) {
      auto trunc = path.substr(slash_pos + 1);
      tags->track({trunc.data(), trunc.size()});
    }
  }

  {
    std::lock_guard<std::mutex> lock{cache_mutex_};
    cache_.Put({path.data(), path.size(), &memory::kSpiRamResource}, tags);
  }

  return tags;
}

auto TagParserImpl::parseNew(std::string_view p) -> std::shared_ptr<TrackTags> {
  std::string path{p};
  libtags::Aux aux;
  auto out = TrackTags::create();
  aux.tags = out.get();
  {
    auto lock = drivers::acquire_spi();

    if (f_stat(path.c_str(), &aux.info) != FR_OK ||
        f_open(&aux.file, path.c_str(), FA_READ) != FR_OK) {
      ESP_LOGW(kTag, "failed to open file %s", path.c_str());
      return {};
    }
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

  int res;
  {
    auto lock = drivers::acquire_spi();
    res = tagsget(&ctx);
    f_close(&aux.file);
  }

  if (res != 0) {
    // Parsing failed.
    ESP_LOGE(kTag, "tag parsing for %s failed, reason %d", path.c_str(), res);
    return {};
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

  return out;
}

}  // namespace database