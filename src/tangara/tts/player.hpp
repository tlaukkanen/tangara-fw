/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <string>

#include "audio/fatfs_stream_factory.hpp"
#include "codec.hpp"
#include "drivers/pcm_buffer.hpp"
#include "tasks.hpp"

namespace tts {

/*
 * A TTS Player is the output stage of the TTS pipeline. It receives a stream
 * of filenames that should be played, and handles decoding these files and
 * sending them to the output buffer.
 */
class Player {
 public:
  Player(tasks::WorkerPool&, drivers::PcmBuffer&, audio::FatfsStreamFactory&);

  auto playFile(const std::string& path) -> void;

  // Not copyable or movable.
  Player(const Player&) = delete;
  Player& operator=(const Player&) = delete;

 private:
  tasks::WorkerPool& bg_;
  audio::FatfsStreamFactory& stream_factory_;
  drivers::PcmBuffer& output_;

  std::mutex new_stream_mutex_;
  std::atomic<bool> stream_playing_;
  std::atomic<bool> stream_cancelled_;

  auto openAndDecode(const std::string& path) -> void;
  auto decodeToSink(const codecs::ICodec::OutputFormat&,
                    std::unique_ptr<codecs::ICodec>) -> void;
};

}  // namespace tts
