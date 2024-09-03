/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "database/tag_parser.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <memory>
#include <mutex>

#include "database/track.hpp"
#include "debug.hpp"
#include "drivers/spi.hpp"
#include "esp_log.h"
#include "ff.h"
#include "ogg/ogg.h"
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

TagParserImpl::TagParserImpl() {
  parsers_.emplace_back(new OggTagParser());
  parsers_.emplace_back(new GenericTagParser());
}

auto TagParserImpl::ReadAndParseTags(std::string_view path)
    -> std::shared_ptr<TrackTags> {
  if (path.empty()) {
    return {};
  }

  // Check the cache first to see if we can skip parsing this file completely.
  {
    std::lock_guard<std::mutex> lock{cache_mutex_};
    std::optional<std::shared_ptr<TrackTags>> cached =
        cache_.Get({path.data(), path.size()});
    if (cached) {
      return *cached;
    }
  }

  // Nothing in the cache; try each of our parsers.
  std::shared_ptr<TrackTags> tags;
  for (auto& parser : parsers_) {
    tags = parser->ReadAndParseTags(path);
    if (tags) {
      break;
    }
  }

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

  // Store the result in the cache for later.
  {
    std::lock_guard<std::mutex> lock{cache_mutex_};
    cache_.Put({path.data(), path.size(), &memory::kSpiRamResource}, tags);
  }

  return tags;
}

OggTagParser::OggTagParser() {
  nameToTag_["TITLE"] = Tag::kTitle;
  nameToTag_["ALBUM"] = Tag::kAlbum;
  nameToTag_["ARTIST"] = Tag::kArtist;
  nameToTag_["ALBUMARTIST"] = Tag::kAlbumArtist;
  nameToTag_["TRACKNUMBER"] = Tag::kTrack;
  nameToTag_["GENRE"] = Tag::kGenres;
  nameToTag_["DISC"] = Tag::kDisc;
}

auto OggTagParser::ReadAndParseTags(std::string_view p)
    -> std::shared_ptr<TrackTags> {
  if (!p.ends_with(".ogg") && !p.ends_with(".opus") && !p.ends_with(".ogx")) {
    return {};
  }
  ogg_sync_state sync;
  ogg_sync_init(&sync);

  ogg_page page;
  ogg_stream_state stream;
  bool stream_init = false;

  std::string path{p};
  FIL file;
  if (f_open(&file, path.c_str(), FA_READ) != FR_OK) {
    ESP_LOGW(kTag, "failed to open file '%s'", path.c_str());
    return {};
  }

  std::shared_ptr<TrackTags> tags;

  // The comments packet is the second in the stream. This is *usually* the
  // second page, sometimes overflowing onto the third page. There is no
  // guarantee of this however, so we read the first five pages before giving
  // up just in case. We don't try to read more pages than this as it could take
  // quite some time, with no likely benefit.
  for (int i = 0; i < 5; i++) {
    // Load up the sync with data until we have a complete page.
    while (ogg_sync_pageout(&sync, &page) != 1) {
      char* buffer = ogg_sync_buffer(&sync, 512);

      UINT br;
      FRESULT fres = f_read(&file, buffer, 512, &br);
      if (fres != FR_OK || br == 0) {
        goto finish;
      }

      int res = ogg_sync_wrote(&sync, br);
      if (res != 0) {
        goto finish;
      }
    }

    // Ensure the stream has the correct serialno. pagein and packetout both
    // give no results if the serialno is incorrect.
    if (ogg_page_bos(&page)) {
      ogg_stream_init(&stream, ogg_page_serialno(&page));
      stream_init = true;
    }

    if (ogg_stream_pagein(&stream, &page) < 0) {
      goto finish;
    }

    // Try to pull out a packet.
    ogg_packet packet;
    if (ogg_stream_packetout(&stream, &packet) == 1) {
      // We're interested in the second packet (packetno == 1) only.
      if (packet.packetno < 1) {
        continue;
      }
      if (packet.packetno > 1) {
        goto finish;
      }

      tags = TrackTags::create();
      if (memcmp(packet.packet, "OpusTags", 8) == 0) {
        std::span<unsigned char> data{packet.packet,
                                      static_cast<size_t>(packet.bytes)};
        tags->encoding(Container::kOpus);
        parseComments(*tags, data.subspan(8));
      } else if (packet.packet[0] == 3 &&
                 memcmp(packet.packet + 1, "vorbis", 6) == 0) {
        std::span<unsigned char> data{packet.packet,
                                      static_cast<size_t>(packet.bytes)};
        tags->encoding(Container::kOgg);
        parseComments(*tags, data.subspan(7));
      }
      break;
    }
  }

finish:
  if (stream_init) {
    ogg_stream_clear(&stream);
  }
  ogg_sync_clear(&sync);
  f_close(&file);

  return tags;
}

auto OggTagParser::parseComments(TrackTags& res, std::span<unsigned char> data)
    -> void {
  uint64_t vendor_len = parseLength(data);
  uint64_t num_tags = parseLength(data.subspan(4 + vendor_len));

  data = data.subspan(4 + vendor_len + 4);
  for (size_t i = 0; i < num_tags; i++) {
    uint64_t size = parseLength(data);

    std::string_view tag = {
        reinterpret_cast<const char*>(data.subspan(4).data()),
        static_cast<size_t>(size)};

    auto split = tag.find("=");

    if (split != std::string::npos) {
      std::string_view key = tag.substr(0, split);
      std::string_view val = tag.substr(split + 1);

      std::string key_upper{key};
      std::transform(key.begin(), key.end(), key_upper.begin(), ::toupper);

      if (nameToTag_.contains(key_upper) && !val.empty()) {
        res.set(nameToTag_[key_upper], val);
      }
    }

    data = data.subspan(4 + size);
  }
}

auto OggTagParser::parseLength(std::span<unsigned char> data) -> uint64_t {
  return static_cast<uint64_t>(data[3]) << 24 |
         static_cast<uint64_t>(data[2]) << 16 |
         static_cast<uint64_t>(data[1]) << 8 |
         static_cast<uint64_t>(data[0]) << 0;
}

auto GenericTagParser::ReadAndParseTags(std::string_view p)
    -> std::shared_ptr<TrackTags> {
  std::string path{p};
  libtags::Aux aux;
  auto out = TrackTags::create();
  aux.tags = out.get();

  if (f_stat(path.c_str(), &aux.info) != FR_OK ||
      f_open(&aux.file, path.c_str(), FA_READ) != FR_OK) {
    return {};
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
