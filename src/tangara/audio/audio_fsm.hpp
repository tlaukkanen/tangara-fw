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

#include "audio/stream_cues.hpp"
#include "drivers/pcm_buffer.hpp"
#include "tinyfsm.hpp"

#include "audio/audio_decoder.hpp"
#include "audio/audio_events.hpp"
#include "audio/audio_sink.hpp"
#include "audio/bt_audio_output.hpp"
#include "audio/fatfs_stream_factory.hpp"
#include "audio/i2s_audio_output.hpp"
#include "audio/track_queue.hpp"
#include "database/database.hpp"
#include "database/tag_parser.hpp"
#include "database/track.hpp"
#include "drivers/display.hpp"
#include "drivers/gpios.hpp"
#include "drivers/i2s_dac.hpp"
#include "drivers/storage.hpp"
#include "system_fsm/service_locator.hpp"
#include "system_fsm/system_events.hpp"

namespace audio {

class AudioState : public tinyfsm::Fsm<AudioState> {
 public:
  virtual ~AudioState() {}

  virtual void entry() {}
  virtual void exit() {}

  /* Fallback event handler. Does nothing. */
  void react(const tinyfsm::Event& ev) {}

  void react(const QueueUpdate&);
  void react(const PlaySineWave&);
  void react(const SetTrack&);
  void react(const TogglePlayPause&);

  void react(const internal::DecodingFinished&);
  void react(const internal::StreamStarted&);
  void react(const internal::StreamEnded&);
  virtual void react(const internal::StreamHeartbeat&) {}

  void react(const StepUpVolume&);
  void react(const StepDownVolume&);

  void react(const SetVolume&);
  void react(const SetVolumeLimit&);
  void react(const SetVolumeBalance&);

  void react(const OutputModeChanged&);

  virtual void react(const system_fsm::BootComplete&) {}
  virtual void react(const system_fsm::KeyLockChanged&){};
  virtual void react(const system_fsm::SdStateChanged&) {}
  virtual void react(const system_fsm::BluetoothEvent&);

 protected:
  auto emitPlaybackUpdate(bool paused) -> void;
  auto commitVolume() -> void;

  static std::shared_ptr<system_fsm::ServiceLocator> sServices;

  static std::shared_ptr<FatfsStreamFactory> sStreamFactory;
  static std::unique_ptr<Decoder> sDecoder;
  static std::shared_ptr<SampleProcessor> sSampleProcessor;
  static std::shared_ptr<I2SAudioOutput> sI2SOutput;
  static std::shared_ptr<BluetoothAudioOutput> sBtOutput;
  static std::shared_ptr<IAudioOutput> sOutput;

  static std::unique_ptr<drivers::PcmBuffer> sDrainBuffer;

  static StreamCues sStreamCues;
  static std::optional<IAudioOutput::Format> sDrainFormat;

  static bool sIsPaused;
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
  void react(const system_fsm::SdStateChanged&) override;

  using AudioState::react;
};

class Playback : public AudioState {
 public:
  void entry() override;
  void exit() override;

  void react(const system_fsm::SdStateChanged&) override;
  void react(const internal::StreamHeartbeat&) override;

  using AudioState::react;
};

}  // namespace states

}  // namespace audio
