/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "audio/audio_sink.hpp"
#include "tinyfsm.hpp"

#include "database/track.hpp"
#include "types.hpp"

namespace audio {

/*
 * Struct encapsulating information about the decoder's current track.
 */
struct TrackInfo {
  /*
   * Audio tags extracted from the file. May be absent for files without any
   * parseable tags.
   */
  std::shared_ptr<database::TrackTags> tags;

  /*
   * URI that the current track was retrieved from. This is currently always a
   * file path on the SD card.
   */
  std::string uri;

  /*
   * The length of this track in seconds. This is either retrieved from the
   * track's tags, or sometimes computed. It may therefore sometimes be
   * inaccurate or missing.
   */
  std::optional<uint32_t> duration;

  /* The offset in seconds that this file's decoding started from. */
  std::optional<uint32_t> start_offset;

  /* The approximate bitrate of this track in its original encoded form. */
  std::optional<uint32_t> bitrate_kbps;

  /* The encoded format of the this track. */
  codecs::StreamType encoding;

  IAudioOutput::Format format;
};

/*
 * Event emitted by the audio FSM when the state of the audio pipeline has
 * changed. This is usually once per second while a track is playing, plus one
 * event each when a track starts or finishes.
 */
struct PlaybackUpdate : tinyfsm::Event {
  /*
   * The track that is currently being decoded by the audio pipeline. May be
   * absent if there is no current track.
   */
  std::shared_ptr<TrackInfo> current_track;

  /*
   * How long the current track has been playing for, in seconds. Will always
   * be present if current_track is present.
   */
  std::optional<uint32_t> track_position;

  /* Whether or not the current track is currently being output to a sink. */
  bool paused;
};

/*
 * Sets a new track to be decoded by the audio pipeline, replacing any
 * currently playing track.
 */
struct SetTrack : tinyfsm::Event {
  std::variant<std::string, database::TrackId, std::monostate> new_track;
  std::optional<uint32_t> seek_to_second;

  enum Transition {
    kHardCut,
    kGapless,
    // TODO: kCrossFade
  };
  Transition transition;
};

struct TogglePlayPause : tinyfsm::Event {
  std::optional<bool> set_to;
};

struct QueueUpdate : tinyfsm::Event {
  bool current_changed;

  enum Reason {
    kExplicitUpdate,
    kRepeatingLastTrack,
    kTrackFinished,
    kDeserialised,
  };
  Reason reason;
};

struct StepUpVolume : tinyfsm::Event {};
struct StepDownVolume : tinyfsm::Event {};
struct SetVolume : tinyfsm::Event {
  std::optional<uint_fast8_t> percent;
  std::optional<int32_t> db;
};
struct SetVolumeBalance : tinyfsm::Event {
  int left_bias;
};

struct VolumeChanged : tinyfsm::Event {
  uint_fast8_t percent;
  int db;
};
struct VolumeBalanceChanged : tinyfsm::Event {
  int left_bias;
};
struct VolumeLimitChanged : tinyfsm::Event {
  int new_limit_db;
};

struct SetVolumeLimit : tinyfsm::Event {
  int limit_db;
};

struct OutputModeChanged : tinyfsm::Event {};

namespace internal {

struct DecodingStarted : tinyfsm::Event {
  std::shared_ptr<TrackInfo> track;
};

struct DecodingFailedToStart : tinyfsm::Event {
  std::shared_ptr<TrackInfo> track;
};

struct DecodingFinished : tinyfsm::Event {
  std::shared_ptr<TrackInfo> track;
};

struct StreamStarted : tinyfsm::Event {
  std::shared_ptr<TrackInfo> track;
  IAudioOutput::Format src_format;
  IAudioOutput::Format dst_format;
};

struct StreamUpdate : tinyfsm::Event {
  uint32_t samples_sunk;
};

struct StreamEnded : tinyfsm::Event {};

}  // namespace internal

}  // namespace audio
