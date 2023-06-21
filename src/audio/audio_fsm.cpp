/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio_fsm.hpp"
#include <future>
#include <memory>
#include <variant>
#include "audio_decoder.hpp"
#include "audio_events.hpp"
#include "audio_task.hpp"
#include "fatfs_audio_input.hpp"
#include "i2s_audio_output.hpp"
#include "i2s_dac.hpp"
#include "pipeline.hpp"
#include "track.hpp"

namespace audio {

static const char kTag[] = "audio_fsm";

drivers::GpioExpander* AudioState::sGpioExpander;
std::shared_ptr<drivers::I2SDac> AudioState::sDac;
std::shared_ptr<drivers::DigitalPot> AudioState::sPots;
std::weak_ptr<database::Database> AudioState::sDatabase;

std::unique_ptr<FatfsAudioInput> AudioState::sFileSource;
std::unique_ptr<I2SAudioOutput> AudioState::sI2SOutput;
std::vector<std::unique_ptr<IAudioElement>> AudioState::sPipeline;

std::deque<AudioState::EnqueuedItem> AudioState::sTrackQueue;

auto AudioState::Init(drivers::GpioExpander* gpio_expander,
                      std::weak_ptr<database::Database> database) -> bool {
  sGpioExpander = gpio_expander;

  auto dac = drivers::I2SDac::create(gpio_expander);
  if (!dac) {
    return false;
  }
  sDac.reset(dac.value());
  sPots.reset(new drivers::DigitalPot(gpio_expander));
  sDatabase = database;

  sFileSource.reset(new FatfsAudioInput());
  sI2SOutput.reset(new I2SAudioOutput(sGpioExpander, sDac, sPots));

  // Perform initial pipeline configuration.
  // TODO(jacqueline): Factor this out once we have any kind of dynamic
  // reconfiguration.
  AudioDecoder* codec = new AudioDecoder();
  sPipeline.emplace_back(codec);

  Pipeline* pipeline = new Pipeline(sPipeline.front().get());
  pipeline->AddInput(sFileSource.get());

  task::StartPipeline(pipeline, sI2SOutput.get());

  return true;
}

void AudioState::react(const system_fsm::StorageMounted& ev) {
  sDatabase = ev.db;
}

namespace states {

void Uninitialised::react(const system_fsm::BootComplete&) {
  transit<Standby>();
}

void Standby::react(const InputFileOpened& ev) {
  transit<Playback>();
}

void Standby::react(const PlayTrack& ev) {
  auto db = sDatabase.lock();
  if (!db) {
    ESP_LOGW(kTag, "database not open; ignoring play request");
    return;
  }

  if (ev.data) {
    sFileSource->OpenFile(ev.data->filepath());
  } else {
    sFileSource->OpenFile(db->GetTrackPath(ev.id));
  }
}

void Standby::react(const PlayFile& ev) {
  sFileSource->OpenFile(ev.filename);
}

void Playback::entry() {
  ESP_LOGI(kTag, "beginning playback");
  sI2SOutput->SetInUse(true);
}

void Playback::exit() {
  ESP_LOGI(kTag, "finishing playback");
  sI2SOutput->SetInUse(false);
}

void Playback::react(const PlayTrack& ev) {
  sTrackQueue.push_back(EnqueuedItem(ev.id));
}

void Playback::react(const PlayFile& ev) {
  sTrackQueue.push_back(EnqueuedItem(ev.filename));
}

void Playback::react(const PlaybackUpdate& ev) {
  ESP_LOGI(kTag, "elapsed: %lu", ev.seconds_elapsed);
}

void Playback::react(const InputFileOpened& ev) {}

void Playback::react(const InputFileFinished& ev) {
  ESP_LOGI(kTag, "finished file");
  if (sTrackQueue.empty()) {
    return;
  }
  EnqueuedItem next_item = sTrackQueue.front();
  sTrackQueue.pop_front();

  if (std::holds_alternative<std::string>(next_item)) {
    sFileSource->OpenFile(std::get<std::string>(next_item));
  } else if (std::holds_alternative<database::TrackId>(next_item)) {
    auto db = sDatabase.lock();
    if (!db) {
      ESP_LOGW(kTag, "database not open; ignoring play request");
      return;
    }
    sFileSource->OpenFile(
        db->GetTrackPath(std::get<database::TrackId>(next_item)));
  }
}

void Playback::react(const AudioPipelineIdle& ev) {
  transit<Standby>();
}

}  // namespace states

}  // namespace audio

FSM_INITIAL_STATE(audio::AudioState, audio::states::Uninitialised)
