/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>

#include "audio/audio_events.hpp"
#include "audio/audio_sink.hpp"
#include "audio/audio_source.hpp"
#include "audio/resample.hpp"
#include "codec.hpp"
#include "drivers/pcm_buffer.hpp"
#include "sample.hpp"

namespace audio {

/*
 * Handle to a persistent task that converts samples between formats (sample
 * rate, channels, bits per sample), in order to put samples in the preferred
 * format of the current output device. The resulting samples are forwarded
 * to the output device's sink stream.
 */
class SampleProcessor {
 public:
  SampleProcessor(drivers::PcmBuffer& sink);
  ~SampleProcessor();

  auto SetOutput(std::shared_ptr<IAudioOutput>) -> void;

  /*
   * Signals to the sample processor that a new discrete stream of audio is now
   * being sent. This will typically represent a new track being played.
   */
  auto beginStream(std::shared_ptr<TrackInfo>) -> void;

  /*
   * Sends a span of PCM samples to the processor. Returns a subspan of the
   * given span containing samples that were not able to be sent during this
   * call, e.g. because of congestion downstream from the processor.
   */
  auto continueStream(std::span<sample::Sample>) -> std::span<sample::Sample>;

  /*
   * Signals to the sample processor that the current stream is ending. This
   * can either be because the stream has naturally finished, or because it is
   * being interrupted.
   * If `cancelled` is false, the sample processor will ensure all previous
   * samples are processed and sent before communicating the end of the stream
   * onwards. If `cancelled` is true, any samples from the current stream that
   * have not yet been played will be discarded.
   */
  auto endStream(bool cancelled) -> void;

  SampleProcessor(const SampleProcessor&) = delete;
  SampleProcessor& operator=(const SampleProcessor&) = delete;

 private:
  auto Main() -> void;

  auto handleBeginStream(std::shared_ptr<TrackInfo>) -> void;
  auto handleEndStream(bool cancel) -> void;

  auto processSamples(bool finalise) -> bool;

  auto hasPendingWork() -> bool;
  auto flushOutputBuffer() -> bool;

  struct Args {
    std::shared_ptr<TrackInfo>* track;
    size_t samples_available;
    bool is_end_of_stream;
    bool clear_buffers;
  };
  QueueHandle_t commands_;
  std::list<Args> pending_commands_;

  auto discardCommand(Args& command) -> void;

  StreamBufferHandle_t source_;
  drivers::PcmBuffer& sink_;

  /* Internal utility for managing buffering samples between our filters. */
  class Buffer {
   public:
    Buffer();
    ~Buffer();

    /* Returns a span of the unused space within the buffer. */
    auto writeAcquire() -> std::span<sample::Sample>;
    /* Signals how many samples were just added to the writeAcquire span. */
    auto writeCommit(size_t) -> void;

    /* Returns a span of the samples stored within the buffer. */
    auto readAcquire() -> std::span<sample::Sample>;
    /* Signals how many samples from the readAcquire span were consumed. */
    auto readCommit(size_t) -> void;

    auto isEmpty() -> bool;
    auto clear() -> void;

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

   private:
    std::span<sample::Sample> buffer_;
    std::span<sample::Sample> samples_in_buffer_;
  };

  Buffer input_buffer_;
  Buffer resampled_buffer_;
  Buffer output_buffer_;

  std::unique_ptr<Resampler> resampler_;
  bool double_samples_;

  std::shared_ptr<IAudioOutput> output_;
  size_t unprocessed_samples_;
};

}  // namespace audio
