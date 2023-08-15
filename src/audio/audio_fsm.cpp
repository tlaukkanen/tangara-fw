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
#include "bluetooth.hpp"
#include "bt_audio_output.hpp"
#include "esp_log.h"
#include "event_queue.hpp"
#include "fatfs_audio_input.hpp"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "future_fetcher.hpp"
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

std::unique_ptr<AudioTask> AudioState::sTask;
std::unique_ptr<FatfsAudioInput> AudioState::sFileSource;
std::unique_ptr<I2SAudioOutput> AudioState::sI2SOutput;
std::unique_ptr<BluetoothAudioOutput> AudioState::sBtOutput;

TrackQueue* AudioState::sTrackQueue;
std::optional<database::TrackId> AudioState::sCurrentTrack;

auto AudioState::Init(drivers::IGpios* gpio_expander,
                      std::weak_ptr<database::Database> database,
                      std::shared_ptr<database::ITagParser> tag_parser,
                      drivers::Bluetooth* bluetooth,
                      TrackQueue* queue) -> bool {
  sIGpios = gpio_expander;
  sTrackQueue = queue;

  auto dac = drivers::I2SDac::create(gpio_expander);
  if (!dac) {
    return false;
  }
  sDac.reset(dac.value());
  sDatabase = database;

  sFileSource.reset(new FatfsAudioInput(tag_parser));
  sI2SOutput.reset(new I2SAudioOutput(sIGpios, sDac));
  // sBtOutput.reset(new BluetoothAudioOutput(bluetooth));

  AudioTask::Start(sFileSource.get(), sI2SOutput.get());
  // AudioTask::Start(sFileSource.get(), sBtOutput.get());

  return true;
}

void AudioState::react(const system_fsm::StorageMounted& ev) {
  sDatabase = ev.db;
}

void AudioState::react(const system_fsm::KeyUpChanged& ev) {
  if (ev.falling && sI2SOutput->AdjustVolumeUp()) {
    ESP_LOGI(kTag, "volume up!");
    events::Ui().Dispatch(VolumeChanged{});
  }
}

void AudioState::react(const system_fsm::KeyDownChanged& ev) {
  if (ev.falling && sI2SOutput->AdjustVolumeDown()) {
    ESP_LOGI(kTag, "volume down!");
    events::Ui().Dispatch(VolumeChanged{});
  }
}

void AudioState::react(const system_fsm::HasPhonesChanged& ev) {
  if (ev.falling) {
    // ESP_LOGI(kTag, "headphones in!");
  } else {
    // ESP_LOGI(kTag, "headphones out!");
  }
}

namespace states {

void Uninitialised::react(const system_fsm::BootComplete&) {
  transit<Standby>();
}

void Standby::react(const PlayFile& ev) {
  sFileSource->SetPath(ev.filename);
}

void Playback::react(const PlayFile& ev) {
  sFileSource->SetPath(ev.filename);
}

void Standby::react(const internal::InputFileOpened& ev) {
  transit<Playback>();
}

void Standby::react(const QueueUpdate& ev) {
  auto current_track = sTrackQueue->GetCurrent();
  if (!current_track || (sCurrentTrack && *sCurrentTrack == *current_track)) {
    return;
  }

  sCurrentTrack = current_track;

  auto db = sDatabase.lock();
  if (!db) {
    ESP_LOGW(kTag, "database not open; ignoring play request");
    return;
  }

  sFileSource->SetPath(db->GetTrackPath(*current_track));
}

void Playback::entry() {
  ESP_LOGI(kTag, "beginning playback");
  sI2SOutput->SetInUse(true);
  // sBtOutput->SetInUse(true);
}

void Playback::exit() {
  ESP_LOGI(kTag, "finishing playback");
  // TODO(jacqueline): Second case where it's useful to wait for the i2s buffer
  // to drain.
  vTaskDelay(pdMS_TO_TICKS(250));
  sI2SOutput->SetInUse(false);
  // sBtOutput->SetInUse(false);
}

void Playback::react(const QueueUpdate& ev) {
  if (!ev.current_changed) {
    return;
  }
  auto current_track = sTrackQueue->GetCurrent();
  if (!current_track) {
    sFileSource->SetPath();
    sCurrentTrack.reset();
    transit<Standby>();
    return;
  }

  sCurrentTrack = current_track;

  auto db = sDatabase.lock();
  if (!db) {
    return;
  }

  sFileSource->SetPath(db->GetTrackPath(*current_track));
}

void Playback::react(const PlaybackUpdate& ev) {
  ESP_LOGI(kTag, "elapsed: %lu, total: %lu", ev.seconds_elapsed,
           ev.seconds_total);
}

void Playback::react(const internal::InputFileOpened& ev) {}

void Playback::react(const internal::InputFileClosed& ev) {}

void Playback::react(const internal::InputFileFinished& ev) {
  ESP_LOGI(kTag, "finished playing file");
  sTrackQueue->Next();
  if (!sTrackQueue->GetCurrent()) {
    transit<Standby>();
  }
}

void Playback::react(const internal::AudioPipelineIdle& ev) {
  transit<Standby>();
}

}  // namespace states

}  // namespace audio

FSM_INITIAL_STATE(audio::AudioState, audio::states::Uninitialised)
