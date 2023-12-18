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
#include "idf_additions.h"
#include "nvs.hpp"
#include "sample.hpp"
#include "service_locator.hpp"
#include "system_events.hpp"
#include "track.hpp"
#include "track_queue.hpp"
#include "wm8523.hpp"

namespace audio {

[[maybe_unused]] static const char kTag[] = "audio_fsm";

std::shared_ptr<system_fsm::ServiceLocator> AudioState::sServices;

std::shared_ptr<FatfsAudioInput> AudioState::sFileSource;
std::unique_ptr<Decoder> AudioState::sDecoder;
std::shared_ptr<SampleConverter> AudioState::sSampleConverter;
std::shared_ptr<I2SAudioOutput> AudioState::sI2SOutput;
std::shared_ptr<BluetoothAudioOutput> AudioState::sBtOutput;
std::shared_ptr<IAudioOutput> AudioState::sOutput;

std::optional<database::TrackId> AudioState::sCurrentTrack;

void AudioState::react(const system_fsm::KeyLockChanged& ev) {
  if (ev.locking && sServices) {
    sServices->nvs().AmpCurrentVolume(sOutput->GetVolume());
  }
}

void AudioState::react(const StepUpVolume& ev) {
  if (sOutput->AdjustVolumeUp()) {
    events::Ui().Dispatch(VolumeChanged{});
  }
}

void AudioState::react(const StepDownVolume& ev) {
  if (sOutput->AdjustVolumeDown()) {
    events::Ui().Dispatch(VolumeChanged{});
  }
}

void AudioState::react(const system_fsm::HasPhonesChanged& ev) {
  if (ev.has_headphones) {
    ESP_LOGI(kTag, "headphones in!");
  } else {
    ESP_LOGI(kTag, "headphones out!");
  }
}

void AudioState::react(const ChangeMaxVolume& ev) {
  ESP_LOGI(kTag, "new max volume %u db",
           (ev.new_max - drivers::wm8523::kLineLevelReferenceVolume) / 4);
  sI2SOutput->SetMaxVolume(ev.new_max);
  sServices->nvs().AmpMaxVolume(ev.new_max);
}

void AudioState::react(const OutputModeChanged& ev) {
  // TODO: handle SetInUse
  ESP_LOGI(kTag, "output mode changed");
  auto new_mode = sServices->nvs().OutputMode();
  sOutput->SetMode(IAudioOutput::Modes::kOff);
  switch (new_mode) {
    case drivers::NvsStorage::Output::kBluetooth:
      sOutput = sBtOutput;
      break;
    case drivers::NvsStorage::Output::kHeadphones:
      sOutput = sI2SOutput;
      break;
  }
  sOutput->SetMode(IAudioOutput::Modes::kOnPaused);
}

auto AudioState::playTrack(database::TrackId id) -> void {
  sCurrentTrack = id;
  sServices->bg_worker().Dispatch<void>([=]() {
    auto db = sServices->database().lock();
    if (!db) {
      return;
    }
    sFileSource->SetPath(db->getTrackPath(id));
  });
}

namespace states {

void Uninitialised::react(const system_fsm::BootComplete& ev) {
  sServices = ev.services;

  constexpr size_t kDrainBufferSize =
      drivers::kI2SBufferLengthFrames * sizeof(sample::Sample) * 2 * 8;
  ESP_LOGI(kTag, "allocating drain buffer, size %u KiB",
           kDrainBufferSize / 1024);
  StreamBufferHandle_t stream = xStreamBufferCreateWithCaps(
      kDrainBufferSize, sizeof(sample::Sample) * 2, MALLOC_CAP_DMA);

  sFileSource.reset(
      new FatfsAudioInput(sServices->tag_parser(), sServices->bg_worker()));
  sI2SOutput.reset(new I2SAudioOutput(stream, sServices->gpios()));
  sBtOutput.reset(new BluetoothAudioOutput(stream, sServices->bluetooth()));

  auto& nvs = sServices->nvs();
  sI2SOutput->SetMaxVolume(nvs.AmpMaxVolume());
  sI2SOutput->SetVolumeDb(nvs.AmpCurrentVolume());

  if (sServices->nvs().OutputMode() ==
      drivers::NvsStorage::Output::kHeadphones) {
    sOutput = sI2SOutput;
  } else {
    sOutput = sBtOutput;
  }
  sOutput->SetMode(IAudioOutput::Modes::kOnPaused);

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
  auto current_track = sServices->track_queue().current();
  if (!current_track || (sCurrentTrack && *sCurrentTrack == *current_track)) {
    return;
  }
  playTrack(*current_track);
}

void Standby::react(const TogglePlayPause& ev) {
  if (sCurrentTrack) {
    transit<Playback>();
  }
}

void Playback::entry() {
  ESP_LOGI(kTag, "beginning playback");
  sOutput->SetMode(IAudioOutput::Modes::kOnPlaying);

  events::System().Dispatch(PlaybackStarted{});
  events::Ui().Dispatch(PlaybackStarted{});
}

void Playback::exit() {
  ESP_LOGI(kTag, "finishing playback");
  // TODO(jacqueline): Second case where it's useful to wait for the i2s buffer
  // to drain.
  vTaskDelay(pdMS_TO_TICKS(10));
  sOutput->SetMode(IAudioOutput::Modes::kOnPaused);

  events::System().Dispatch(PlaybackFinished{});
  events::Ui().Dispatch(PlaybackFinished{});
}

void Playback::react(const system_fsm::HasPhonesChanged& ev) {
  if (!ev.has_headphones) {
    transit<Standby>();
  }
}

void Playback::react(const QueueUpdate& ev) {
  if (!ev.current_changed) {
    return;
  }
  auto current_track = sServices->track_queue().current();
  if (!current_track) {
    sFileSource->SetPath();
    sCurrentTrack.reset();
    transit<Standby>();
    return;
  }
  playTrack(*current_track);
}

void Playback::react(const TogglePlayPause& ev) {
  transit<Standby>();
}

void Playback::react(const PlaybackUpdate& ev) {
  ESP_LOGI(kTag, "elapsed: %lu, total: %lu", ev.seconds_elapsed,
           ev.track->duration);
}

void Playback::react(const internal::InputFileOpened& ev) {}

void Playback::react(const internal::InputFileClosed& ev) {}

void Playback::react(const internal::InputFileFinished& ev) {
  ESP_LOGI(kTag, "finished playing file");
  sServices->track_queue().next();
  if (!sServices->track_queue().current()) {
    transit<Standby>();
  }
}

void Playback::react(const internal::AudioPipelineIdle& ev) {
  transit<Standby>();
}

}  // namespace states

}  // namespace audio

FSM_INITIAL_STATE(audio::AudioState, audio::states::Uninitialised)
