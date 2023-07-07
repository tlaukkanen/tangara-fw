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
#include "esp_log.h"
#include "event_queue.hpp"
#include "fatfs_audio_input.hpp"
#include "i2s_audio_output.hpp"
#include "i2s_dac.hpp"
#include "pipeline.hpp"
#include "system_events.hpp"
#include "track.hpp"
#include "track_queue.hpp"

namespace audio {

static const char kTag[] = "audio_fsm";

drivers::IGpios* AudioState::sIGpios;
std::shared_ptr<drivers::I2SDac> AudioState::sDac;
std::weak_ptr<database::Database> AudioState::sDatabase;

std::unique_ptr<FatfsAudioInput> AudioState::sFileSource;
std::unique_ptr<I2SAudioOutput> AudioState::sI2SOutput;
std::vector<std::unique_ptr<IAudioElement>> AudioState::sPipeline;

TrackQueue* AudioState::sTrackQueue;

auto AudioState::Init(drivers::IGpios* gpio_expander,
                      std::weak_ptr<database::Database> database,
                      TrackQueue* queue) -> bool {
  sIGpios = gpio_expander;
  sTrackQueue = queue;

  auto dac = drivers::I2SDac::create(gpio_expander);
  if (!dac) {
    return false;
  }
  sDac.reset(dac.value());
  sDatabase = database;

  sFileSource.reset(new FatfsAudioInput());
  sI2SOutput.reset(new I2SAudioOutput(sIGpios, sDac));

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

void AudioState::react(const system_fsm::KeyUpChanged& ev) {
  if (ev.falling && sI2SOutput->AdjustVolumeUp()) {
    ESP_LOGI(kTag, "volume up!");
    events::Dispatch<VolumeChanged, ui::UiState>({});
  }
}

void AudioState::react(const system_fsm::KeyDownChanged& ev) {
  if (ev.falling && sI2SOutput->AdjustVolumeDown()) {
    ESP_LOGI(kTag, "volume down!");
    events::Dispatch<VolumeChanged, ui::UiState>({});
  }
}

void AudioState::react(const system_fsm::HasPhonesChanged& ev) {
  if (ev.falling) {
    ESP_LOGI(kTag, "headphones in!");
  } else {
    ESP_LOGI(kTag, "headphones out!");
  }
}

namespace states {

void Uninitialised::react(const system_fsm::BootComplete&) {
  transit<Standby>();
}

void Standby::react(const internal::InputFileOpened& ev) {
  transit<Playback>();
}

void Standby::react(const QueueUpdate& ev) {
  auto current_track = sTrackQueue->GetCurrent();
  if (!current_track) {
    return;
  }

  auto db = sDatabase.lock();
  if (!db) {
    ESP_LOGW(kTag, "database not open; ignoring play request");
    return;
  }

  sFileSource->OpenFile(db->GetTrackPath(*current_track));
}

void Playback::entry() {
  ESP_LOGI(kTag, "beginning playback");
  sI2SOutput->SetInUse(true);
}

void Playback::exit() {
  ESP_LOGI(kTag, "finishing playback");
  sI2SOutput->SetInUse(false);
}

void Playback::react(const QueueUpdate& ev) {
  auto current_track = sTrackQueue->GetCurrent();
  if (!current_track) {
    // TODO: return to standby?
    return;
  }

  auto db = sDatabase.lock();
  if (!db) {
    return;
  }

  // TODO: what if we just finished this, and are preemptively loading the next
  // one?
  sFileSource->OpenFile(db->GetTrackPath(*current_track));
}

void Playback::react(const PlaybackUpdate& ev) {
  // ESP_LOGI(kTag, "elapsed: %lu, total: %lu", ev.seconds_elapsed,
  // ev.seconds_total);
}

void Playback::react(const internal::InputFileOpened& ev) {}

void Playback::react(const internal::InputFileClosed& ev) {
  ESP_LOGI(kTag, "finished reading file");
  auto upcoming = sTrackQueue->GetUpcoming(1);
  if (upcoming.empty()) {
    return;
  }
  auto db = sDatabase.lock();
  if (!db) {
    return;
  }
  ESP_LOGI(kTag, "preemptively opening next file");
  sFileSource->OpenFile(db->GetTrackPath(upcoming.front()));
}

void Playback::react(const internal::InputFileFinished& ev) {
  ESP_LOGI(kTag, "finished playing file");
  sTrackQueue->Next();
}

void Playback::react(const internal::AudioPipelineIdle& ev) {
  transit<Standby>();
}

}  // namespace states

}  // namespace audio

FSM_INITIAL_STATE(audio::AudioState, audio::states::Uninitialised)
