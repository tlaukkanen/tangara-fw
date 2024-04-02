/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <deque>
#include <memory>
#include <vector>

#include "audio_sink.hpp"
#include "service_locator.hpp"
#include "tinyfsm.hpp"

#include "audio_decoder.hpp"
#include "audio_events.hpp"
#include "bt_audio_output.hpp"
#include "database.hpp"
#include "display.hpp"
#include "fatfs_audio_input.hpp"
#include "gpios.hpp"
#include "i2s_audio_output.hpp"
#include "i2s_dac.hpp"
#include "storage.hpp"
#include "system_events.hpp"
#include "tag_parser.hpp"
#include "track.hpp"
#include "track_queue.hpp"

namespace audio {

class AudioState : public tinyfsm::Fsm<AudioState> {
 public:
  virtual ~AudioState() {}

  virtual void entry() {}
  virtual void exit() {}

  /* Fallback event handler. Does nothing. */
  void react(const tinyfsm::Event& ev) {}

  void react(const QueueUpdate&);
  void react(const SetTrack&);
  void react(const TogglePlayPause&);

  void react(const internal::StreamStarted&);
  void react(const internal::StreamUpdate&);
  void react(const internal::StreamEnded&);

  void react(const StepUpVolume&);
  void react(const StepDownVolume&);
  virtual void react(const system_fsm::HasPhonesChanged&);

  void react(const SetVolume&);
  void react(const SetVolumeLimit&);
  void react(const SetVolumeBalance&);

  void react(const OutputModeChanged&);

  virtual void react(const system_fsm::BootComplete&) {}
  virtual void react(const system_fsm::KeyLockChanged&){};
  virtual void react(const system_fsm::StorageMounted&) {}
  virtual void react(const system_fsm::BluetoothEvent&);

 protected:
  auto clearDrainBuffer() -> void;
  auto awaitEmptyDrainBuffer() -> void;

  auto playTrack(database::TrackId id) -> void;
  auto commitVolume() -> void;

  static std::shared_ptr<system_fsm::ServiceLocator> sServices;

  static std::shared_ptr<FatfsAudioInput> sFileSource;
  static std::unique_ptr<Decoder> sDecoder;
  static std::shared_ptr<SampleConverter> sSampleConverter;
  static std::shared_ptr<I2SAudioOutput> sI2SOutput;
  static std::shared_ptr<BluetoothAudioOutput> sBtOutput;
  static std::shared_ptr<IAudioOutput> sOutput;

  static StreamBufferHandle_t sDrainBuffer;

  static std::shared_ptr<TrackInfo> sCurrentTrack;
  static uint64_t sCurrentSamples;
  static std::optional<IAudioOutput::Format> sDrainFormat;
  static bool sCurrentTrackIsFromQueue;

  static std::shared_ptr<TrackInfo> sNextTrack;
  static uint64_t sNextTrackCueSamples;
  static bool sNextTrackIsFromQueue;

  static bool sIsResampling;
  static bool sIsPaused;

  auto currentPositionSeconds() -> std::optional<uint32_t>;
};

namespace states {

class Uninitialised : public AudioState {
 public:
  void react(const system_fsm::BootComplete&) override;
  void react(const system_fsm::BluetoothEvent&) override{};

  using AudioState::react;
};

class Standby : public AudioState {
 public:
  void react(const system_fsm::KeyLockChanged&) override;
  void react(const system_fsm::StorageMounted&) override;

  using AudioState::react;
};

class Playback : public AudioState {
 public:
  void entry() override;
  void exit() override;

  using AudioState::react;
};

}  // namespace states

}  // namespace audio
