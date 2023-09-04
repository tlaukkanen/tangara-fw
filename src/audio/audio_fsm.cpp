/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio_fsm.hpp"
#include <stdint.h>

#include <future>
#include <memory>
#include <variant>

#include "audio_sink.hpp"
#include "esp_log.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"

#include "audio_converter.hpp"
#include "audio_decoder.hpp"
#include "audio_events.hpp"
#include "bluetooth.hpp"
#include "bt_audio_output.hpp"
#include "event_queue.hpp"
#include "fatfs_audio_input.hpp"
#include "future_fetcher.hpp"
#include "i2s_audio_output.hpp"
#include "i2s_dac.hpp"
#include "service_locator.hpp"
#include "system_events.hpp"
#include "track.hpp"
#include "track_queue.hpp"
#include "wm8523.hpp"

namespace audio {

static const char kTag[] = "audio_fsm";

std::shared_ptr<system_fsm::ServiceLocator> AudioState::sServices;

std::shared_ptr<FatfsAudioInput> AudioState::sFileSource;
std::unique_ptr<Decoder> AudioState::sDecoder;
std::shared_ptr<SampleConverter> AudioState::sSampleConverter;
std::shared_ptr<I2SAudioOutput> AudioState::sI2SOutput;
std::shared_ptr<IAudioOutput> AudioState::sOutput;

std::optional<database::TrackId> AudioState::sCurrentTrack;

void AudioState::react(const system_fsm::KeyUpChanged& ev) {
  if (ev.falling && sOutput->AdjustVolumeUp()) {
    ESP_LOGI(kTag, "volume up!");
    events::Ui().Dispatch(VolumeChanged{});
  }
}

void AudioState::react(const system_fsm::KeyDownChanged& ev) {
  if (ev.falling && sOutput->AdjustVolumeDown()) {
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

void AudioState::react(const ChangeMaxVolume& ev) {
  ESP_LOGI(kTag, "new max volume %u db",
           (ev.new_max - drivers::wm8523::kLineLevelReferenceVolume) / 4);
  sI2SOutput->SetMaxVolume(ev.new_max);
  sServices->nvs().AmpMaxVolume(ev.new_max);
}

namespace states {

void Uninitialised::react(const system_fsm::BootComplete& ev) {
  sServices = ev.services;

  auto dac = drivers::I2SDac::create(sServices->gpios());
  if (!dac) {
    events::System().Dispatch(system_fsm::FatalError{});
    events::Ui().Dispatch(system_fsm::FatalError{});
    return;
  }

  sFileSource.reset(new FatfsAudioInput(sServices->tag_parser()));
  sI2SOutput.reset(new I2SAudioOutput(sServices->gpios(),
                                      std::unique_ptr<drivers::I2SDac>{*dac}));

  auto& nvs = sServices->nvs();
  sI2SOutput->SetMaxVolume(nvs.AmpMaxVolume().get());
  sI2SOutput->SetVolumeDb(nvs.AmpCurrentVolume().get());

  sOutput = sI2SOutput;
  // sOutput.reset(new BluetoothAudioOutput(bluetooth));

  sSampleConverter.reset(new SampleConverter());
  sSampleConverter->SetOutput(sOutput);

  Decoder::Start(sFileSource, sSampleConverter);

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
  auto current_track = sServices->track_queue().GetCurrent();
  if (!current_track || (sCurrentTrack && *sCurrentTrack == *current_track)) {
    return;
  }

  sCurrentTrack = current_track;

  auto db = sServices->database().lock();
  if (!db) {
    ESP_LOGW(kTag, "database not open; ignoring play request");
    return;
  }
  sFileSource->SetPath(db->GetTrackPath(*current_track));
}

void Standby::react(const TogglePlayPause& ev) {
  if (sCurrentTrack) {
    transit<Playback>();
  }
}

void Playback::entry() {
  ESP_LOGI(kTag, "beginning playback");
  sOutput->SetInUse(true);
}

void Playback::exit() {
  ESP_LOGI(kTag, "finishing playback");
  // TODO(jacqueline): Second case where it's useful to wait for the i2s buffer
  // to drain.
  vTaskDelay(pdMS_TO_TICKS(10));
  sOutput->SetInUse(false);

  events::System().Dispatch(PlaybackFinished{});
}

void Playback::react(const QueueUpdate& ev) {
  if (!ev.current_changed) {
    return;
  }
  auto current_track = sServices->track_queue().GetCurrent();
  if (!current_track) {
    sFileSource->SetPath();
    sCurrentTrack.reset();
    transit<Standby>();
    return;
  }

  sCurrentTrack = current_track;

  auto db = sServices->database().lock();
  if (!db) {
    return;
  }

  sFileSource->SetPath(db->GetTrackPath(*current_track));
}

void Playback::react(const TogglePlayPause& ev) {
  transit<Standby>();
}

void Playback::react(const PlaybackUpdate& ev) {
  ESP_LOGI(kTag, "elapsed: %lu, total: %lu", ev.seconds_elapsed,
           ev.seconds_total);
}

void Playback::react(const internal::InputFileOpened& ev) {}

void Playback::react(const internal::InputFileClosed& ev) {}

void Playback::react(const internal::InputFileFinished& ev) {
  ESP_LOGI(kTag, "finished playing file");
  sServices->track_queue().Next();
  if (!sServices->track_queue().GetCurrent()) {
    transit<Standby>();
  }
}

void Playback::react(const internal::AudioPipelineIdle& ev) {
  transit<Standby>();
}

}  // namespace states

}  // namespace audio

FSM_INITIAL_STATE(audio::AudioState, audio::states::Uninitialised)
