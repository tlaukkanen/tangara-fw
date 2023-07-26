/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "fatfs_audio_input.hpp"

#include <stdint.h>

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <variant>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "ff.h"

#include "audio_events.hpp"
#include "audio_fsm.hpp"
#include "audio_source.hpp"
#include "event_queue.hpp"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "future_fetcher.hpp"
#include "span.hpp"
#include "stream_info.hpp"
#include "tag_parser.hpp"
#include "tasks.hpp"
#include "types.hpp"

static const char* kTag = "SRC";

namespace audio {

static constexpr UINT kFileBufferSize = 4096 * 2;
static constexpr UINT kStreamerBufferSize = 1024;

static StreamBufferHandle_t sForwardDest = nullptr;

auto forward_cb(const BYTE* buf, UINT buf_length) -> UINT {
  if (buf_length == 0) {
    return !xStreamBufferIsFull(sForwardDest);
  } else {
    return xStreamBufferSend(sForwardDest, buf, buf_length, 0);
  }
}

FileStreamer::FileStreamer(StreamBufferHandle_t dest,
                           SemaphoreHandle_t data_was_read)
    : control_(xQueueCreate(1, sizeof(Command))),
      destination_(dest),
      data_was_read_(data_was_read),
      has_data_(false),
      file_(),
      next_file_() {
  assert(sForwardDest == nullptr);
  sForwardDest = dest;
  tasks::StartPersistent<tasks::Type::kFileStreamer>([this]() { Main(); });
}

FileStreamer::~FileStreamer() {
  sForwardDest = nullptr;
  Command quit = kQuit;
  xQueueSend(control_, &quit, portMAX_DELAY);
  vQueueDelete(control_);
}

auto FileStreamer::Main() -> void {
  for (;;) {
    Command cmd;
    xQueueReceive(control_, &cmd, portMAX_DELAY);

    if (cmd == kQuit) {
      break;
    } else if (cmd == kRestart) {
      CloseFile();
      xStreamBufferReset(destination_);
      file_ = std::move(next_file_);
      has_data_ = file_ != nullptr;
    } else if (cmd == kRefillBuffer && file_) {
      UINT bytes_sent = 0;  // Unused.
      // Use f_forward to push bytes directly from FATFS internal buffers into
      // the destination. This has the nice side effect of letting FATFS decide
      // the most efficient way to pull in data from disk; usually one whole
      // sector at a time. Consult the FATFS lib application notes if changing
      // this to use f_read.
      FRESULT res = f_forward(file_.get(), forward_cb, UINT_MAX, &bytes_sent);
      if (res != FR_OK || f_eof(file_.get())) {
        CloseFile();
        has_data_ = false;
      }
      if (bytes_sent > 0) {
        xSemaphoreGive(data_was_read_);
      }
    }
  }

  ESP_LOGW(kTag, "quit file streamer");
  CloseFile();
  vTaskDelete(NULL);
}

auto FileStreamer::Fetch() -> void {
  if (!has_data_.load()) {
    return;
  }
  Command refill = kRefillBuffer;
  xQueueSend(control_, &refill, portMAX_DELAY);
}

auto FileStreamer::HasFinished() -> bool {
  return !has_data_.load();
}

auto FileStreamer::Restart(std::unique_ptr<FIL> new_file) -> void {
  next_file_ = std::move(new_file);
  Command restart = kRestart;
  xQueueSend(control_, &restart, portMAX_DELAY);
  Command fill = kRefillBuffer;
  xQueueSend(control_, &fill, portMAX_DELAY);
}

auto FileStreamer::CloseFile() -> void {
  if (!file_) {
    return;
  }
  ESP_LOGI(kTag, "closing file");
  f_close(file_.get());
  file_ = {};
  events::Audio().Dispatch(internal::InputFileClosed{});
}

FatfsAudioInput::FatfsAudioInput(
    std::shared_ptr<database::ITagParser> tag_parser)
    : IAudioSource(),
      tag_parser_(tag_parser),
      has_data_(xSemaphoreCreateBinary()),
      streamer_buffer_(xStreamBufferCreate(kStreamerBufferSize, 1)),
      streamer_(new FileStreamer(streamer_buffer_, has_data_)),
      file_buffer_info_(),
      file_buffer_len_(kFileBufferSize),
      file_buffer_(reinterpret_cast<std::byte*>(
          heap_caps_malloc(file_buffer_len_,
                           MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL))),
      file_buffer_stream_(&file_buffer_info_, {file_buffer_, file_buffer_len_}),
      source_mutex_(),
      pending_path_(),
      current_format_() {}

FatfsAudioInput::~FatfsAudioInput() {
  streamer_.reset();
  vStreamBufferDelete(streamer_buffer_);
  vSemaphoreDelete(has_data_);
  free(file_buffer_);
}

auto FatfsAudioInput::SetPath(std::future<std::optional<std::string>> fut)
    -> void {
  std::lock_guard<std::mutex> lock{source_mutex_};

  CloseCurrentFile();
  pending_path_.reset(
      new database::FutureFetcher<std::optional<std::string>>(std::move(fut)));

  xSemaphoreGive(has_data_);
}

auto FatfsAudioInput::SetPath(const std::string& path) -> void {
  std::lock_guard<std::mutex> lock{source_mutex_};

  CloseCurrentFile();
  OpenFile(path);
}

auto FatfsAudioInput::SetPath() -> void {
  std::lock_guard<std::mutex> lock{source_mutex_};
  CloseCurrentFile();
}

auto FatfsAudioInput::Read(
    std::function<bool(StreamInfo::Format)> can_read,
    std::function<size_t(cpp::span<const std::byte>)> read,
    TickType_t max_wait) -> void {
  // Wait until we have data to return.
  xSemaphoreTake(has_data_, portMAX_DELAY);

  // Ensure the file doesn't change whilst we're trying to get data about it.
  std::lock_guard<std::mutex> source_lock{source_mutex_};

  // If the path is a future, then wait for it to complete.
  // TODO(jacqueline): We should really make some kind of FreeRTOS-integrated
  // way to block a task whilst awaiting a future.
  if (pending_path_) {
    while (!pending_path_->Finished()) {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    auto res = pending_path_->Result();
    pending_path_.reset();

    if (res || *res) {
      OpenFile(**res);
    }

    // Bail out now that we've resolved the future. If we end up successfully
    // readinig from the path, then has_data will be flagged again.
    return;
  }

  // Move data from the file streamer's buffer into our file buffer. We need our
  // own buffer so that we can handle concatenating smaller file chunks into
  // complete frames for the decoder.
  OutputStream writer{&file_buffer_stream_};
  std::size_t bytes_added =
      xStreamBufferReceive(streamer_buffer_, writer.data().data(),
                           writer.data().size_bytes(), pdMS_TO_TICKS(0));
  writer.add(bytes_added);

  // HACK: libmad needs at least MAD_HEADER_GUARD (= 8) extra bytes following a
  // frame, or else it refuses to decode it.
  if (IsCurrentFormatMp3() && !HasDataRemaining()) {
    ESP_LOGI(kTag, "applying MAD_HEADER_GUARD fix");
    cpp::span<std::byte> buf = writer.data();
    size_t pad_amount = std::min<size_t>(buf.size_bytes(), 8);
    std::fill_n(buf.begin(), pad_amount, static_cast<std::byte>(0));
  }

  InputStream reader{&file_buffer_stream_};
  auto data_for_cb = reader.data();
  if (!data_for_cb.empty() && std::invoke(can_read, *current_format_)) {
    reader.consume(std::invoke(read, reader.data()));
  }

  if (!HasDataRemaining()) {
    // Out of data. We're finished. Note we don't care about anything left in
    // the file buffer at this point; the callback as seen it, so if it didn't
    // consume it then presumably whatever is left isn't enough to form a
    // complete frame.
    ESP_LOGI(kTag, "finished streaming file");
    CloseCurrentFile();
  } else {
    // There is still data to be read, or sitting in the buffer.
    streamer_->Fetch();
    xSemaphoreGive(has_data_);
  }
}

auto FatfsAudioInput::OpenFile(const std::string& path) -> void {
  ESP_LOGI(kTag, "opening file %s", path.c_str());

  FILINFO info;
  if (f_stat(path.c_str(), &info) != FR_OK) {
    ESP_LOGE(kTag, "failed to stat file");
    return;
  }

  database::TrackTags tags;
  if (!tag_parser_->ReadAndParseTags(path, &tags)) {
    ESP_LOGE(kTag, "failed to read tags");
    return;
  }

  auto stream_type = ContainerToStreamType(tags.encoding());
  if (!stream_type.has_value()) {
    ESP_LOGE(kTag, "couldn't match container to stream");
    return;
  }

  if (*stream_type == codecs::StreamType::kPcm && tags.channels &&
      tags.bits_per_sample && tags.channels) {
    current_format_ = StreamInfo::Pcm{
        .channels = static_cast<uint8_t>(*tags.channels),
        .bits_per_sample = static_cast<uint8_t>(*tags.bits_per_sample),
        .sample_rate = static_cast<uint32_t>(*tags.sample_rate),
    };
  } else {
    current_format_ = StreamInfo::Encoded{
        .type = *stream_type,
        .duration_bytes = info.fsize,
    };
  }

  std::unique_ptr<FIL> file = std::make_unique<FIL>();
  FRESULT res = f_open(file.get(), path.c_str(), FA_READ);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "failed to open file! res: %i", res);
    return;
  }

  streamer_->Restart(std::move(file));

  events::Audio().Dispatch(internal::InputFileOpened{});
}

auto FatfsAudioInput::CloseCurrentFile() -> void {
  streamer_->Restart({});
  xStreamBufferReset(streamer_buffer_);
  current_format_ = {};
}

auto FatfsAudioInput::HasDataRemaining() -> bool {
  return !xStreamBufferIsEmpty(streamer_buffer_) || !streamer_->HasFinished();
}

auto FatfsAudioInput::ContainerToStreamType(database::Encoding enc)
    -> std::optional<codecs::StreamType> {
  switch (enc) {
    case database::Encoding::kMp3:
      return codecs::StreamType::kMp3;
    case database::Encoding::kWav:
      return codecs::StreamType::kPcm;
    case database::Encoding::kFlac:
      return codecs::StreamType::kFlac;
    case database::Encoding::kOgg:  // Misnamed; this is Ogg Vorbis.
      return codecs::StreamType::kVorbis;
    case database::Encoding::kUnsupported:
    default:
      return {};
  }
}

auto FatfsAudioInput::IsCurrentFormatMp3() -> bool {
  if (!current_format_) {
    return false;
  }
  if (!std::holds_alternative<StreamInfo::Encoded>(*current_format_)) {
    return false;
  }
  return std::get<StreamInfo::Encoded>(*current_format_).type ==
         codecs::StreamType::kMp3;
}

}  // namespace audio
