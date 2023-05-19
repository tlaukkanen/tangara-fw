#pragma once
#include <memory>

#include "audio_events.hpp"
#include "dac.hpp"
#include "database.hpp"
#include "display.hpp"
#include "fatfs_audio_input.hpp"
#include "gpio_expander.hpp"
#include "i2s_audio_output.hpp"
#include "storage.hpp"
#include "tinyfsm.hpp"

#include "system_events.hpp"

namespace audio {

class AudioState : public tinyfsm::Fsm<AudioState> {
 public:
  static auto Init(drivers::GpioExpander* gpio_expander,
                   std::weak_ptr<drivers::AudioDac>,
                   std::weak_ptr<database::Database>) -> void;

  virtual ~AudioState() {}

  virtual void entry() {}
  virtual void exit() {}

  /* Fallback event handler. Does nothing. */
  void react(const tinyfsm::Event& ev) {}

  virtual void react(const system_fsm::BootComplete&) {}
  virtual void react(const PlaySong&) {}

 protected:
  static drivers::GpioExpander* sGpioExpander;
  static std::weak_ptr<drivers::AudioDac> sDac;
  static std::weak_ptr<database::Database> sDatabase;

  static std::unique_ptr<FatfsAudioInput> sFileSource;
  static std::unique_ptr<I2SAudioOutput> sI2SOutput;
  static std::vector<std::unique_ptr<IAudioElement>> sPipeline;
};

namespace states {

class Uninitialised : public AudioState {
 public:
  void react(const system_fsm::BootComplete&) override;
  using AudioState::react;
};

class Standby : public AudioState {
 public:
  void react(const PlaySong&) override {}
  using AudioState::react;
};

}  // namespace states

}  // namespace audio
