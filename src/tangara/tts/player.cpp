/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "tts/player.hpp"
#include <mutex>

#include "audio/audio_events.hpp"
#include "audio/processor.hpp"
#include "audio/resample.hpp"
#include "codec.hpp"
#include "esp_log.h"
#include "events/event_queue.hpp"
#include "freertos/projdefs.h"
#include "portmacro.h"
#include "sample.hpp"
#include "types.hpp"

namespace tts {

[[maybe_unused]] static constexpr char kTag[] = "ttsplay";

Player::Player(tasks::WorkerPool& worker,
               drivers::PcmBuffer& output,
               audio::FatfsStreamFactory& factory)
    : bg_(worker),
      stream_factory_(factory),
      output_(output),
      stream_playing_(false),
      stream_cancelled_(false) {}

auto Player::playFile(const std::string& path) -> void {
  ESP_LOGI(kTag, "playing '%s'", path.c_str());

  bg_.Dispatch<void>([=, this]() {
    // Interrupt current playback
    {
      std::scoped_lock<std::mutex> lock{new_stream_mutex_};
      if (stream_playing_) {
        stream_cancelled_ = true;
        stream_playing_.wait(true);
      }
      stream_cancelled_ = false;
      stream_playing_ = true;
    }

    openAndDecode(path);

    if (!stream_cancelled_) {
      events::Audio().Dispatch(audio::TtsPlaybackChanged{.is_playing = false});
    }
    stream_playing_ = false;
    stream_playing_.notify_all();
  });
}

auto Player::openAndDecode(const std::string& path) -> void {
  auto stream = stream_factory_.create(path);
  if (!stream) {
    ESP_LOGE(kTag, "creating stream failed");
    return;
  }

  // FIXME: Rather than hardcoding WAV support only, we should work out a
  // proper subset of 'low memory' decoders that can all be used for TTS
  // playback.
  if (stream->type() != codecs::StreamType::kWav) {
    ESP_LOGE(kTag, "stream was unsupported type");
    return;
  }

  auto decoder = codecs::CreateCodecForType(stream->type());
  if (!decoder) {
    ESP_LOGE(kTag, "creating decoder failed");
    return;
  }

  std::unique_ptr<codecs::ICodec> codec{*decoder};
  auto open_res = codec->OpenStream(stream, 0);
  if (open_res.has_error()) {
    ESP_LOGE(kTag, "opening stream failed");
    return;
  }

  decodeToSink(*open_res, std::move(codec));
}

auto Player::decodeToSink(const codecs::ICodec::OutputFormat& format,
                          std::unique_ptr<codecs::ICodec> codec) -> void {
  // Set up buffers to hold samples between the intermediary parts of
  // processing. We can just use the stack for these, since this method is
  // called only from background workers, which have enormous stacks.
  sample::Sample decode_storage[4096];
  audio::Buffer decode_buf(decode_storage);

  sample::Sample resample_storage[4096];
  audio::Buffer resample_buf(resample_storage);

  sample::Sample stereo_storage[4096];
  audio::Buffer stereo_buf(stereo_storage);

  // Work out what processing the codec's output needs.
  std::unique_ptr<audio::Resampler> resampler;
  if (format.sample_rate_hz != 48000) {
    resampler = std::make_unique<audio::Resampler>(format.sample_rate_hz, 48000,
                                                   format.num_channels);
  }
  bool double_samples = format.num_channels == 1;

  // Start our playback (wait for previous to end?)
  events::Audio().Dispatch(audio::TtsPlaybackChanged{.is_playing = true});

  // FIXME: This decode-and-process loop is substantially the same as the audio
  // processor's filter loop. Ideally we should refactor both of these loops to
  // reuse code, however I'm holding off on doing this until we've implemented
  // more advanced audio processing features in the audio processor (EQ, tempo
  // shifting, etc.) as it's not clear to me yet how much the two codepaths will
  // be diverging later anyway.
  while ((codec || !decode_buf.isEmpty() || !resample_buf.isEmpty() ||
          !stereo_buf.isEmpty()) &&
         !stream_cancelled_) {
    if (codec) {
      auto decode_res = codec->DecodeTo(decode_buf.writeAcquire());
      if (decode_res.has_error()) {
        ESP_LOGE(kTag, "decoding error");
        break;
      }
      decode_buf.writeCommit(decode_res->samples_written);
      if (decode_res->is_stream_finished) {
        codec.reset();
      }
    }

    if (!decode_buf.isEmpty()) {
      auto resample_input = decode_buf.readAcquire();
      auto resample_output = resample_buf.writeAcquire();

      size_t read, wrote;
      if (resampler) {
        std::tie(read, wrote) =
            resampler->Process(resample_input, resample_output, false);
      } else {
        read = wrote = std::min(resample_input.size(), resample_output.size());
        std::copy_n(resample_input.begin(), read, resample_output.begin());
      }

      decode_buf.readCommit(read);
      resample_buf.writeCommit(wrote);
    }

    if (!resample_buf.isEmpty()) {
      auto channels_input = resample_buf.readAcquire();
      auto channels_output = stereo_buf.writeAcquire();
      size_t read, wrote;
      if (double_samples) {
        wrote = channels_output.size();
        read = wrote / 2;
        if (read > channels_input.size()) {
          read = channels_input.size();
          wrote = read * 2;
        }
        for (size_t i = 0; i < read; i++) {
          channels_output[i * 2] = channels_input[i];
          channels_output[(i * 2) + 1] = channels_input[i];
        }
      } else {
        read = wrote = std::min(channels_input.size(), channels_output.size());
        std::copy_n(channels_input.begin(), read, channels_output.begin());
      }
      resample_buf.readCommit(read);
      stereo_buf.writeCommit(wrote);
    }

    // The mixin PcmBuffer should almost always be draining, so we can force
    // samples into it more aggressively than with the main music PcmBuffer.
    while (!stereo_buf.isEmpty()) {
      size_t sent = output_.send(stereo_buf.readAcquire());
      stereo_buf.readCommit(sent);
    }
  }

  while (!output_.isEmpty()) {
    if (stream_cancelled_) {
      output_.clear();
    } else {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

}  // namespace tts
