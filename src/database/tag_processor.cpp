#include "tag_processor.hpp"

#include <ff.h>
#include <tags.h>
#include <esp_log.h>

namespace database {

namespace libtags {

struct Aux {
  FIL file;
  FILINFO info;
  std::string artist;
  std::string album;
  std::string title;
};

static int read(Tagctx *ctx, void *buf, int cnt) {
	Aux *aux = reinterpret_cast<Aux*>(ctx->aux);
        UINT bytes_read;
        if (f_read(&aux->file, buf, cnt, &bytes_read) != FR_OK) {
          return -1;
        }
        return bytes_read;
}

static int seek(Tagctx *ctx, int offset, int whence) {
	Aux *aux = reinterpret_cast<Aux*>(ctx->aux);
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

static void
tag(Tagctx *ctx, int t, const char *k, const char *v, int offset, int size, Tagread f) {
  Aux *aux = reinterpret_cast<Aux*>(ctx->aux);
  if (t == Ttitle) {
    aux->title = v;
  } else if (t == Tartist) {
    aux->artist = v;
  } else if (t == Talbum) {
    aux->album = v;
  }
}

static void
toc(Tagctx *ctx, int ms, int offset) {}

} // namespace libtags

static const std::size_t kBufSize = 1024;
static const char* kTag = "TAGS";

auto GetInfo(const std::string& path, FileInfo* out) -> bool {
  libtags::Aux aux;
  if (f_stat(path.c_str(), &aux.info) != FR_OK || f_open(&aux.file, path.c_str(), FA_READ) != FR_OK) {
    ESP_LOGI(kTag, "failed to open file");
    return false;
  }
  // Fine to have this on the stack; this is only called on the leveldb task.
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
    ESP_LOGI(kTag, "failed to parse tags");
    return false;
  }

  if (ctx.format == Fmp3) {
    ESP_LOGI(kTag, "file is mp3");
    ESP_LOGI(kTag, "artist: %s", aux.artist.c_str());
    ESP_LOGI(kTag, "album: %s", aux.album.c_str());
    ESP_LOGI(kTag, "title: %s", aux.title.c_str());
    out->is_playable = true;
    out->title = aux.title;
    return true;
  }

  return false;
}

}  // namespace database
