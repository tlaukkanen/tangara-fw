/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <deque>
#include <memory>
#include <vector>

#include "audio_events.hpp"
#include "database.hpp"
#include "display.hpp"
#include "fatfs_audio_input.hpp"
#include "gpios.hpp"
#include "i2s_audio_output.hpp"
#include "i2s_dac.hpp"
#include "storage.hpp"
#include "tinyfsm.hpp"
#include "track.hpp"

#include "system_events.hpp"

namespace audio {

class AudioState : public tinyfsm::Fsm<AudioState> {
 public:
  static auto Init(drivers::IGpios* gpio_expander,
                   std::weak_ptr<database::Database>) -> bool;

  virtual ~AudioState() {}

  virtual void entry() {}
  virtual void exit() {}

  /* Fallback event handler. Does nothing. */
  void react(const tinyfsm::Event& ev) {}

  void react(const system_fsm::StorageMounted&);

  void react(const system_fsm::KeyUpChanged&);
  void react(const system_fsm::KeyDownChanged&);

  virtual void react(const system_fsm::BootComplete&) {}
  virtual void react(const PlayTrack&) {}
  virtual void react(const PlayFile&) {}

  virtual void react(const PlaybackUpdate&) {}

  virtual void react(const InputFileOpened&) {}
  virtual void react(const InputFileFinished&) {}
  virtual void react(const AudioPipelineIdle&) {}

 protected:
  static drivers::IGpios* sIGpios;
  static std::shared_ptr<drivers::I2SDac> sDac;
  static std::weak_ptr<database::Database> sDatabase;

  static std::unique_ptr<FatfsAudioInput> sFileSource;
  static std::unique_ptr<I2SAudioOutput> sI2SOutput;
  static std::vector<std::unique_ptr<IAudioElement>> sPipeline;

  typedef std::variant<database::TrackId, std::string> EnqueuedItem;
  static std::deque<EnqueuedItem> sTrackQueue;
};

namespace states {

class Uninitialised : public AudioState {
 public:
  void react(const system_fsm::BootComplete&) override;
  using AudioState::react;
};

class Standby : public AudioState {
 public:
  void react(const InputFileOpened&) override;
  void react(const PlayTrack&) override;
  void react(const PlayFile&) override;

  using AudioState::react;
};

class Playback : public AudioState {
 public:
  void entry() override;
  void exit() override;

  void react(const PlayTrack&) override;
  void react(const PlayFile&) override;

  void react(const PlaybackUpdate&) override;

  void react(const InputFileOpened&) override;
  void react(const InputFileFinished&) override;
  void react(const AudioPipelineIdle&) override;

  using AudioState::react;
};

}  // namespace states

}  // namespace audio
