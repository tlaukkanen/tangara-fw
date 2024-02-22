/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "miniflac.hpp"

#include <cstdint>
#include <cstdlib>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "miniflac.h"
#include "result.hpp"
#include "sample.hpp"

namespace codecs {

[[maybe_unused]] static const char kTag[] = "flac";

static constexpr size_t kMaxFrameSize = 4608;

MiniFlacDecoder::MiniFlacDecoder()
    : input_(),
      buffer_(),
      flac_(reinterpret_cast<miniflac_t*>(
          heap_caps_malloc(sizeof(miniflac_t),
                           MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT))),
      current_sample_() {
  miniflac_init(flac_.get(), MINIFLAC_CONTAINER_UNKNOWN);
  for (int i = 0; i < samples_by_channel_.size(); i++) {
    uint32_t caps;
    if (i == 0) {
      caps = MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL;
    } else {
      // FIXME: We can *almost* fit two channels into internal ram, but we're a
      // few KiB shy of being able to do it safely.
      caps = MALLOC_CAP_SPIRAM;
    }
    samples_by_channel_[i] = reinterpret_cast<int32_t*>(
        heap_caps_malloc(kMaxFrameSize * sizeof(int32_t), caps));
  }
}

MiniFlacDecoder::~MiniFlacDecoder() {
  for (int i = 0; i < samples_by_channel_.size(); i++) {
    heap_caps_free(samples_by_channel_[i]);
  }
}

auto MiniFlacDecoder::OpenStream(std::shared_ptr<IStream> input,uint32_t offset)
    -> cpp::result<OutputFormat, Error> {
  input_ = input;

  MINIFLAC_RESULT res;
  auto read_until_result = [&](auto fn) {
    while (true) {
      bool eof = buffer_.Refill(input_.get());
      buffer_.ConsumeBytes(fn);
      if (res == MINIFLAC_CONTINUE && !eof) {
        continue;
      }
      break;
    }
  };

  uint32_t sample_rate = 0;

  read_until_result([&](cpp::span<std::byte> buf) -> size_t {
    uint32_t bytes_used = 0;
    res = miniflac_streaminfo_sample_rate(
        flac_.get(), reinterpret_cast<const uint8_t*>(buf.data()),
        buf.size_bytes(), &bytes_used, &sample_rate);
    return bytes_used;
  });

  if (res != MINIFLAC_OK) {
    return cpp::fail(Error::kMalformedData);
  }

  uint8_t channels = 0;

  read_until_result([&](cpp::span<std::byte> buf) -> size_t {
    uint32_t bytes_used = 0;
    res = miniflac_streaminfo_channels(
        flac_.get(), reinterpret_cast<const uint8_t*>(buf.data()),
        buf.size_bytes(), &bytes_used, &channels);
    return bytes_used;
  });

  if (res != MINIFLAC_OK) {
    return cpp::fail(Error::kMalformedData);
  }

  uint64_t total_samples = 0;

  read_until_result([&](cpp::span<std::byte> buf) -> size_t {
    uint32_t bytes_used = 0;
    res = miniflac_streaminfo_total_samples(
        flac_.get(), reinterpret_cast<const uint8_t*>(buf.data()),
        buf.size_bytes(), &bytes_used, &total_samples);
    return bytes_used;
  });

  if (res != MINIFLAC_OK) {
    return cpp::fail(Error::kMalformedData);
  }

  if (channels == 0 || channels > 2) {
    return cpp::fail(Error::kMalformedData);
  }

  // Seeking
  offset = 50;
  if (offset) {
      // TODO: This assumes a constant sample_rate
      uint64_t target_sample = sample_rate * offset;
      uint32_t num_seekpoints;
      read_until_result([&](cpp::span<std::byte> buf) -> size_t {
        uint32_t bytes_used = 0;
        res = miniflac_seektable_seekpoints(
            flac_.get(), reinterpret_cast<const uint8_t*>(buf.data()),
            buf.size_bytes(), &bytes_used, &num_seekpoints);
        return bytes_used;
      });
      if (res != MINIFLAC_OK) {
        // TODO: Not having a seektable is not malformed
        // but currently seeking will not work without it.
        return cpp::fail(Error::kMalformedData);
      }
    // Loop over the seek table
    ESP_LOGI(kTag, "Found seektable with %lu points", num_seekpoints);
    uint64_t sample_number;
    uint64_t sample_offset_bytes;
    uint16_t num_samples_in_target;
    for (uint32_t i = 0; i < num_seekpoints; i++) {
      // Get sample number
      read_until_result([&](cpp::span<std::byte> buf) -> size_t {
        uint32_t bytes_used = 0;
        res = miniflac_seektable_sample_number(
            flac_.get(), reinterpret_cast<const uint8_t*>(buf.data()),
            buf.size_bytes(), &bytes_used, &sample_number);
        return bytes_used;
      });
      if (res != MINIFLAC_OK) {
        return cpp::fail(Error::kMalformedData);
      }
      read_until_result([&](cpp::span<std::byte> buf) -> size_t {
        uint32_t bytes_used = 0;
        res = miniflac_seektable_sample_offset(
            flac_.get(), reinterpret_cast<const uint8_t*>(buf.data()),
            buf.size_bytes(), &bytes_used, &sample_offset_bytes);
        return bytes_used;
      });
      if (res != MINIFLAC_OK) {
        return cpp::fail(Error::kMalformedData);
      }
      read_until_result([&](cpp::span<std::byte> buf) -> size_t {
        uint32_t bytes_used = 0;
        res = miniflac_seektable_samples(
            flac_.get(), reinterpret_cast<const uint8_t*>(buf.data()),
            buf.size_bytes(), &bytes_used, &num_samples_in_target);
        return bytes_used;
      });
      if (res != MINIFLAC_OK) {
        return cpp::fail(Error::kMalformedData);
      }

      ESP_LOGI(kTag, "Seektable entry %lu", i);

      // Check if we want to seek to this seektable position?
      if (sample_number + num_samples_in_target >= target_sample) {
        ESP_LOGI(kTag, "Break on Seektable entry %lu", i);
        break;
      }
    }

    // Seek forward to target_sample
    if (sample_number > 0) {
      ESP_LOGI(kTag, "total samples: %llu", total_samples);
      ESP_LOGI(kTag, "TARGET SAMPLE: %llu", target_sample);
      ESP_LOGI(kTag, "SAMPLE NUMBER: %llu", sample_number);
    }
    uint64_t byte_offset = sample_offset_bytes;
    ESP_LOGI(kTag, "Byte offset is forward %llu bytes", byte_offset);
    ESP_LOGI(kTag, "Decoder state pre: %d", flac_->state);
    while(flac_.get()->state == MINIFLAC_METADATA) {
        read_until_result([&](cpp::span<std::byte> buf) -> size_t {
          uint32_t bytes_used = 0;
          res = miniflac_sync(
              flac_.get(), reinterpret_cast<const uint8_t*>(buf.data()),
              buf.size_bytes(), &bytes_used);
          return bytes_used;
        });
        if (res != MINIFLAC_OK) {
          ESP_LOGI(kTag, "Decoder error 1 %d", res);
        }
    }
    ESP_LOGI(kTag, "Decoder state post: %d", flac_->state);


    ESP_LOGI(kTag, "Going to skip forward %llu bytes", byte_offset);
    if (input_.get()->CanSeek()) {
      ESP_LOGI(kTag, "Skipping forward %llu bytes", byte_offset);
      buffer_.Empty();
      input_.get()->SeekTo(byte_offset, IStream::SeekFrom::kCurrentPosition);
      ESP_LOGI(kTag, "Skipped %llu bytes", byte_offset);
    }


    ESP_LOGI(kTag, "Pre-refill");
    buffer_.Refill(input_.get());
    ESP_LOGI(kTag, "Post-refill");


    read_until_result([&](cpp::span<std::byte> buf) -> size_t {
      uint32_t bytes_used = 0;
      res = miniflac_sync(
          flac_.get(), reinterpret_cast<const uint8_t*>(buf.data()),
          buf.size_bytes(), &bytes_used);
      return bytes_used;
    });
    if (res != MINIFLAC_OK) {
      ESP_LOGI(kTag, "Decoder error 1 %d", res);
    }
    ESP_LOGI(kTag, "JOB'S DONE");
  }

  ESP_LOGI(kTag, "Decoder state: %d", flac_->state);
  ESP_LOGI(kTag, "Frame header state: %d", flac_->frame.header.state);

  // TODO: Sample number is not guaranteed, could be block index.
  ESP_LOGI(kTag, "Ended up... at sample %llu", flac_->frame.header.sample_number);
  ESP_LOGI(kTag, "and block index: %lu", flac_->frame.header.frame_number);
  ESP_LOGI(kTag, "total samples: %llu", total_samples);


  OutputFormat format{
      .num_channels = static_cast<uint8_t>(channels),
      .sample_rate_hz = static_cast<uint32_t>(sample_rate),
      .total_samples = total_samples * channels,
  };

  return format;
}

auto MiniFlacDecoder::DecodeTo(cpp::span<sample::Sample> output)
    -> cpp::result<OutputInfo, Error> {
  bool is_eof = false;

  if (!current_sample_) {
    MINIFLAC_RESULT res = MINIFLAC_CONTINUE;
    while (res == MINIFLAC_CONTINUE && !is_eof) {
      is_eof = buffer_.Refill(input_.get());
      ESP_LOGI(kTag, "EOF? %s", is_eof ? "true" : "false");
      buffer_.ConsumeBytes([&](cpp::span<std::byte> buf) -> size_t {
        // FIXME: We should do a miniflac_sync first, in order to check that
        // our sample buffers have enough space for the next frame.
        uint32_t bytes_read = 0;
        res = miniflac_decode(
            flac_.get(), reinterpret_cast<const uint8_t*>(buf.data()),
            buf.size_bytes(), &bytes_read, samples_by_channel_.data());
        return bytes_read;
      });
    }

    if (res == MINIFLAC_OK) {
      current_sample_ = 0;
    } else if (is_eof) {
      return OutputInfo{
          .samples_written = 0,
          .is_stream_finished = true,
      };
    } else {
      ESP_LOGI(kTag, "Failed: decoder result: %d", res);
      // return cpp::fail(Error::kMalformedData);
    }
  }

  size_t samples_written = 0;
  if (current_sample_) {
    while (*current_sample_ < flac_->frame.header.block_size) {
      if (samples_written + flac_->frame.header.channels >= output.size()) {
        // We can't fit the next full PCM frame into the buffer.
        return OutputInfo{.samples_written = samples_written,
                          .is_stream_finished = false};
      }

      for (int channel = 0; channel < flac_->frame.header.channels; channel++) {
        output[samples_written++] =
            sample::FromSigned(samples_by_channel_[channel][*current_sample_],
                               flac_->frame.header.bps);
      }
      (*current_sample_)++;
    }
  }

  current_sample_.reset();
  ESP_LOGI(kTag, "Samples written %lu", (uint32_t)samples_written);
  return OutputInfo{.samples_written = samples_written,
                    .is_stream_finished = samples_written == 0 && is_eof};
}

auto MiniFlacDecoder::SeekTo(size_t target) -> cpp::result<void, Error> {
  return {};
}

}  // namespace codecs
