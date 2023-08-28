/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <deque>
#include <memory>
#include <vector>

#include "audio_sink.hpp"
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
  static auto Init(drivers::IGpios* gpio_expander,
                   std::weak_ptr<database::Database>,
                   std::shared_ptr<database::ITagParser>,
                   drivers::Bluetooth* bluetooth,
                   TrackQueue* queue) -> bool;

  virtual ~AudioState() {}

  virtual void entry() {}
  virtual void exit() {}

  /* Fallback event handler. Does nothing. */
  void react(const tinyfsm::Event& ev) {}

  void react(const system_fsm::StorageMounted&);

  void react(const system_fsm::KeyUpChanged&);
  void react(const system_fsm::KeyDownChanged&);
  void react(const system_fsm::HasPhonesChanged&);

  virtual void react(const system_fsm::BootComplete&) {}

  virtual void react(const PlayFile&) {}
  virtual void react(const QueueUpdate&) {}
  virtual void react(const PlaybackUpdate&) {}
  virtual void react(const TogglePlayPause&) {}

  virtual void react(const internal::InputFileOpened&) {}
  virtual void react(const internal::InputFileClosed&) {}
  virtual void react(const internal::InputFileFinished&) {}
  virtual void react(const internal::AudioPipelineIdle&) {}

 protected:
  static drivers::IGpios* sIGpios;
  static std::shared_ptr<drivers::I2SDac> sDac;
  static std::weak_ptr<database::Database> sDatabase;

  static std::shared_ptr<FatfsAudioInput> sFileSource;
  static std::unique_ptr<Decoder> sDecoder;
  static std::shared_ptr<SampleConverter> sSampleConverter;
  static std::shared_ptr<IAudioOutput> sOutput;

  static TrackQueue* sTrackQueue;
  static std::optional<database::TrackId> sCurrentTrack;
};

namespace states {

class Uninitialised : public AudioState {
 public:
  void react(const system_fsm::BootComplete&) override;
  using AudioState::react;
};

class Standby : public AudioState {
 public:
  void react(const PlayFile&) override;
  void react(const internal::InputFileOpened&) override;
  void react(const QueueUpdate&) override;
  void react(const TogglePlayPause&) override;

  using AudioState::react;
};

class Playback : public AudioState {
 public:
  void entry() override;
  void exit() override;

  void react(const PlayFile&) override;
  void react(const QueueUpdate&) override;
  void react(const PlaybackUpdate&) override;
  void react(const TogglePlayPause&) override;

  void react(const internal::InputFileOpened&) override;
  void react(const internal::InputFileClosed&) override;
  void react(const internal::InputFileFinished&) override;
  void react(const internal::AudioPipelineIdle&) override;

  using AudioState::react;
};

}  // namespace states

}  // namespace audio
