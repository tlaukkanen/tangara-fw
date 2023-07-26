/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <string>

#include "ff.h"

#include "audio_source.hpp"
#include "freertos/portmacro.h"
#include "future_fetcher.hpp"
#include "stream_info.hpp"
#include "tag_parser.hpp"
#include "types.hpp"

namespace audio {

/*
 * Handles coordination with a persistent background task to asynchronously
 * read files from disk into a StreamBuffer.
 */
class FileStreamer {
 public:
  FileStreamer(StreamBufferHandle_t dest, SemaphoreHandle_t first_read);
  ~FileStreamer();

  /*
   * Continues reading data into the destination buffer until the destination
   * is full.
   */
  auto Fetch() -> void;

  /* Returns true if the streamer has run out of data from the current file. */
  auto HasFinished() -> bool;

  /*
   * Clears any remaining buffered data, and begins reading again from the
   * given file. This function respects any seeking/reading that has already
   * been done on the new source file.
   */
  auto Restart(std::unique_ptr<FIL>) -> void;

  FileStreamer(const FileStreamer&) = delete;
  FileStreamer& operator=(const FileStreamer&) = delete;

 private:
  // Note: private methods here should only be called from the streamer's task.

  auto Main() -> void;
  auto CloseFile() -> void;

  enum Command {
    kRestart,
    kRefillBuffer,
    kQuit,
  };
  QueueHandle_t control_;
  StreamBufferHandle_t destination_;
  SemaphoreHandle_t data_was_read_;

  std::atomic<bool> has_data_;
  std::unique_ptr<FIL> file_;
  std::unique_ptr<FIL> next_file_;
};

/*
 * Audio source that fetches data from a FatFs (or exfat i guess) filesystem.
 *
 * All public methods are safe to call from any task.
 */
class FatfsAudioInput : public IAudioSource {
 public:
  explicit FatfsAudioInput(std::shared_ptr<database::ITagParser> tag_parser);
  ~FatfsAudioInput();

  /*
   * Immediately cease reading any current source, and begin reading from the
   * given file path.
   */
  auto SetPath(std::future<std::optional<std::string>>) -> void;
  auto SetPath(const std::string&) -> void;
  auto SetPath() -> void;

  auto Read(std::function<void(Flags, InputStream&)>, TickType_t)
      -> void override;

  FatfsAudioInput(const FatfsAudioInput&) = delete;
  FatfsAudioInput& operator=(const FatfsAudioInput&) = delete;

 private:
  // Note: private methods assume that the appropriate locks have already been
  // acquired.

  auto OpenFile(const std::string& path) -> void;
  auto CloseCurrentFile() -> void;
  auto HasDataRemaining() -> bool;

  auto ContainerToStreamType(database::Encoding)
      -> std::optional<codecs::StreamType>;
  auto IsCurrentFormatMp3() -> bool;

  std::shared_ptr<database::ITagParser> tag_parser_;

  // Semaphore used to block when this source is out of data. This should be
  // acquired before attempting to read data, and returned after each incomplete
  // read.
  SemaphoreHandle_t has_data_;

  StreamBufferHandle_t streamer_buffer_;
  std::unique_ptr<FileStreamer> streamer_;

  std::unique_ptr<RawStream> input_buffer_;

  // Mutex guarding the current file/stream associated with this source. Must be
  // held during readings, and before altering the current file.
  std::mutex source_mutex_;

  std::unique_ptr<database::FutureFetcher<std::optional<std::string>>>
      pending_path_;
  bool is_first_read_;
};

}  // namespace audio
