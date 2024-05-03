/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio/audio_fsm.hpp"
#include <stdint.h>

#include <future>
#include <memory>
#include <variant>

#include "audio/audio_sink.hpp"
#include "cppbor.h"
#include "cppbor_parse.h"
#include "drivers/bluetooth_types.hpp"
#include "drivers/storage.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"

#include "audio/audio_converter.hpp"
#include "audio/audio_decoder.hpp"
#include "audio/audio_events.hpp"
#include "audio/bt_audio_output.hpp"
#include "audio/fatfs_audio_input.hpp"
#include "audio/i2s_audio_output.hpp"
#include "audio/track_queue.hpp"
#include "database/future_fetcher.hpp"
#include "database/track.hpp"
#include "drivers/bluetooth.hpp"
#include "drivers/i2s_dac.hpp"
#include "drivers/nvs.hpp"
#include "drivers/wm8523.hpp"
#include "events/event_queue.hpp"
#include "sample.hpp"
#include "system_fsm/service_locator.hpp"
#include "system_fsm/system_events.hpp"
#include "tinyfsm.hpp"

namespace audio {

[[maybe_unused]] static const char kTag[] = "audio_fsm";

std::shared_ptr<system_fsm::ServiceLocator> AudioState::sServices;

std::shared_ptr<FatfsAudioInput> AudioState::sFileSource;
std::unique_ptr<Decoder> AudioState::sDecoder;
std::shared_ptr<SampleConverter> AudioState::sSampleConverter;
std::shared_ptr<I2SAudioOutput> AudioState::sI2SOutput;
std::shared_ptr<BluetoothAudioOutput> AudioState::sBtOutput;
std::shared_ptr<IAudioOutput> AudioState::sOutput;

// Two seconds of samples for two channels, at a representative sample rate.
constexpr size_t kDrainLatencySamples = 48000 * 2 * 2;
constexpr size_t kDrainBufferSize =
    sizeof(sample::Sample) * kDrainLatencySamples;

StreamBufferHandle_t AudioState::sDrainBuffer;
std::optional<IAudioOutput::Format> AudioState::sDrainFormat;

std::shared_ptr<TrackInfo> AudioState::sCurrentTrack;
uint64_t AudioState::sCurrentSamples;
bool AudioState::sCurrentTrackIsFromQueue;

std::shared_ptr<TrackInfo> AudioState::sNextTrack;
uint64_t AudioState::sNextTrackCueSamples;
bool AudioState::sNextTrackIsFromQueue;

bool AudioState::sIsResampling;
bool AudioState::sIsPaused = true;

auto AudioState::currentPositionSeconds() -> std::optional<uint32_t> {
  if (!sCurrentTrack || !sDrainFormat) {
    return {};
  }
  return sCurrentSamples /
         (sDrainFormat->num_channels * sDrainFormat->sample_rate);
}

void AudioState::react(const QueueUpdate& ev) {
  SetTrack cmd{
      .new_track = std::monostate{},
      .seek_to_second = {},
      .transition = SetTrack::Transition::kHardCut,
  };

  auto current = sServices->track_queue().current();
  if (current) {
    cmd.new_track = *current;
  }

  switch (ev.reason) {
    case QueueUpdate::kExplicitUpdate:
      if (!ev.current_changed) {
        return;
      }
      sNextTrackIsFromQueue = true;
      cmd.transition = SetTrack::Transition::kHardCut;
      break;
    case QueueUpdate::kRepeatingLastTrack:
      sNextTrackIsFromQueue = true;
      cmd.transition = SetTrack::Transition::kGapless;
      break;
    case QueueUpdate::kTrackFinished:
      if (!ev.current_changed) {
        cmd.new_track = std::monostate{};
      } else {
        sNextTrackIsFromQueue = true;
      }
      cmd.transition = SetTrack::Transition::kGapless;
      break;
    case QueueUpdate::kDeserialised:
    default:
      // The current track is deserialised separately in order to retain seek
      // position.
      return;
  }

  tinyfsm::FsmList<AudioState>::dispatch(cmd);
}

void AudioState::react(const SetTrack& ev) {
  // Remember the current track if there is one, since we need to preserve some
  // of the state if it turns out this SetTrack event corresponds to seeking
  // within the current track.
  std::string prev_uri;
  bool prev_from_queue = false;
  if (sCurrentTrack) {
    prev_uri = sCurrentTrack->uri;
    prev_from_queue = sCurrentTrackIsFromQueue;
  }

  if (ev.transition == SetTrack::Transition::kHardCut) {
    sCurrentTrack.reset();
    sCurrentSamples = 0;
    sCurrentTrackIsFromQueue = false;
    clearDrainBuffer();
  }

  if (std::holds_alternative<std::monostate>(ev.new_track)) {
    ESP_LOGI(kTag, "playback finished, awaiting drain");
    sFileSource->SetPath();
    awaitEmptyDrainBuffer();
    sCurrentTrack.reset();
    sDrainFormat.reset();
    sCurrentSamples = 0;
    sCurrentTrackIsFromQueue = false;
    transit<states::Standby>();
    return;
  }

  // Move the rest of the work to a background worker, since it may require db
  // lookups to resolve a track id into a path.
  auto new_track = ev.new_track;
  uint32_t seek_to = ev.seek_to_second.value_or(0);
  sServices->bg_worker().Dispatch<void>([=]() {
    std::optional<std::string> path;
    if (std::holds_alternative<database::TrackId>(new_track)) {
      auto db = sServices->database().lock();
      if (db) {
        path = db->getTrackPath(std::get<database::TrackId>(new_track));
      }
    } else if (std::holds_alternative<std::string>(new_track)) {
      path = std::get<std::string>(new_track);
    }

    if (path) {
      if (*path == prev_uri) {
        // This was a seek or replay within the same track; don't forget where
        // the track originally came from.
        sNextTrackIsFromQueue = prev_from_queue;
      }
      sFileSource->SetPath(*path, seek_to);
    } else {
      sFileSource->SetPath();
    }
  });
}

void AudioState::react(const TogglePlayPause& ev) {
  sIsPaused = !ev.set_to.value_or(sIsPaused);
  if (!sIsPaused && is_in_state<states::Standby>() && sCurrentTrack) {
    transit<states::Playback>();
  } else if (sIsPaused && is_in_state<states::Playback>()) {
    transit<states::Standby>();
  }
}

void AudioState::react(const internal::StreamStarted& ev) {
  sDrainFormat = ev.dst_format;
  sIsResampling = ev.src_format != ev.dst_format;

  sNextTrack = ev.track;
  sNextTrackCueSamples = sCurrentSamples + (kDrainLatencySamples / 2);

  ESP_LOGI(kTag, "new stream %s %u ch @ %lu hz (resample=%i)",
           ev.track->uri.c_str(), sDrainFormat->num_channels,
           sDrainFormat->sample_rate, sIsResampling);
}

void AudioState::react(const internal::StreamEnded&) {
  ESP_LOGI(kTag, "stream ended");

  if (sCurrentTrackIsFromQueue) {
    sServices->track_queue().finish();
  } else {
    tinyfsm::FsmList<AudioState>::dispatch(SetTrack{
        .new_track = std::monostate{},
        .seek_to_second = {},
        .transition = SetTrack::Transition::kGapless,
    });
  }
}

void AudioState::react(const internal::StreamUpdate& ev) {
  sCurrentSamples += ev.samples_sunk;

  if (sNextTrack && sCurrentSamples >= sNextTrackCueSamples) {
    ESP_LOGI(kTag, "next track is now sinking");
    sCurrentTrack = sNextTrack;
    sCurrentSamples -= sNextTrackCueSamples;
    sCurrentSamples += sNextTrack->start_offset.value_or(0) *
                       (sDrainFormat->num_channels * sDrainFormat->sample_rate);
    sCurrentTrackIsFromQueue = sNextTrackIsFromQueue;

    sNextTrack.reset();
    sNextTrackCueSamples = 0;
    sNextTrackIsFromQueue = false;
  }

  if (sCurrentTrack) {
    PlaybackUpdate event{
        .current_track = sCurrentTrack,
        .track_position = currentPositionSeconds(),
        .paused = !is_in_state<states::Playback>(),
    };
    events::System().Dispatch(event);
    events::Ui().Dispatch(event);
  }

  if (sCurrentTrack && !sIsPaused && !is_in_state<states::Playback>()) {
    ESP_LOGI(kTag, "ready to play!");
    transit<states::Playback>();
  }
}

void AudioState::react(const system_fsm::BluetoothEvent& ev) {
  if (ev.event != drivers::bluetooth::Event::kConnectionStateChanged) {
    return;
  }
  auto dev = sServices->bluetooth().ConnectedDevice();
  if (!dev) {
    return;
  }
  sBtOutput->SetVolume(sServices->nvs().BluetoothVolume(dev->mac));
  events::Ui().Dispatch(VolumeChanged{
      .percent = sOutput->GetVolumePct(),
      .db = sOutput->GetVolumeDb(),
  });
}

void AudioState::react(const StepUpVolume& ev) {
  if (sOutput->AdjustVolumeUp()) {
    commitVolume();
    events::Ui().Dispatch(VolumeChanged{
        .percent = sOutput->GetVolumePct(),
        .db = sOutput->GetVolumeDb(),
    });
  }
}

void AudioState::react(const StepDownVolume& ev) {
  if (sOutput->AdjustVolumeDown()) {
    commitVolume();
    events::Ui().Dispatch(VolumeChanged{
        .percent = sOutput->GetVolumePct(),
        .db = sOutput->GetVolumeDb(),
    });
  }
}

void AudioState::react(const system_fsm::HasPhonesChanged& ev) {
  if (ev.has_headphones) {
    ESP_LOGI(kTag, "headphones in!");
  } else {
    ESP_LOGI(kTag, "headphones out!");
  }
}

void AudioState::react(const SetVolume& ev) {
  if (ev.db.has_value()) {
    if (sOutput->SetVolumeDb(ev.db.value())) {
      commitVolume();
      events::Ui().Dispatch(VolumeChanged{
          .percent = sOutput->GetVolumePct(),
          .db = sOutput->GetVolumeDb(),
      });
    }

  } else if (ev.percent.has_value()) {
    if (sOutput->SetVolumePct(ev.percent.value())) {
      commitVolume();
      events::Ui().Dispatch(VolumeChanged{
          .percent = sOutput->GetVolumePct(),
          .db = sOutput->GetVolumeDb(),
      });
    }
  }
}

void AudioState::react(const SetVolumeLimit& ev) {
  uint16_t limit_in_dac_units =
      drivers::wm8523::kLineLevelReferenceVolume + (ev.limit_db * 4);

  sI2SOutput->SetMaxVolume(limit_in_dac_units);
  sServices->nvs().AmpMaxVolume(limit_in_dac_units);

  events::Ui().Dispatch(VolumeLimitChanged{
      .new_limit_db = ev.limit_db,
  });
  events::Ui().Dispatch(VolumeChanged{
      .percent = sOutput->GetVolumePct(),
      .db = sOutput->GetVolumeDb(),
  });
}

void AudioState::react(const SetVolumeBalance& ev) {
  sOutput->SetVolumeImbalance(ev.left_bias);
  sServices->nvs().AmpLeftBias(ev.left_bias);

  events::Ui().Dispatch(VolumeBalanceChanged{
      .left_bias = ev.left_bias,
  });
}

void AudioState::react(const OutputModeChanged& ev) {
  ESP_LOGI(kTag, "output mode changed");
  auto new_mode = sServices->nvs().OutputMode();
  sOutput->mode(IAudioOutput::Modes::kOff);
  switch (new_mode) {
    case drivers::NvsStorage::Output::kBluetooth:
      sOutput = sBtOutput;
      break;
    case drivers::NvsStorage::Output::kHeadphones:
      sOutput = sI2SOutput;
      break;
  }
  sOutput->mode(IAudioOutput::Modes::kOnPaused);
  sSampleConverter->SetOutput(sOutput);

  // Bluetooth volume isn't 'changed' until we've connected to a device.
  if (new_mode == drivers::NvsStorage::Output::kHeadphones) {
    events::Ui().Dispatch(VolumeChanged{
        .percent = sOutput->GetVolumePct(),
        .db = sOutput->GetVolumeDb(),
    });
  }
}

auto AudioState::clearDrainBuffer() -> void {
  // Tell the decoder to stop adding new samples. This might not take effect
  // immediately, since the decoder might currently be stuck waiting for space
  // to become available in the drain buffer.
  sFileSource->SetPath();

  auto mode = sOutput->mode();
  if (mode == IAudioOutput::Modes::kOnPlaying) {
    // If we're currently playing, then the drain buffer will be actively
    // draining on its own. Just keep trying to reset until it works.
    while (xStreamBufferReset(sDrainBuffer) != pdPASS) {
    }
  } else {
    // If we're not currently playing, then we need to actively pull samples
    // out of the drain buffer to unblock the decoder.
    while (!xStreamBufferIsEmpty(sDrainBuffer)) {
      // Read a little to unblock the decoder.
      uint8_t drain[2048];
      xStreamBufferReceive(sDrainBuffer, drain, sizeof(drain), 0);

      // Try to quickly discard the rest.
      xStreamBufferReset(sDrainBuffer);
    }
  }
}

auto AudioState::awaitEmptyDrainBuffer() -> void {
  if (is_in_state<states::Playback>()) {
    for (int i = 0; i < 10 && !xStreamBufferIsEmpty(sDrainBuffer); i++) {
      vTaskDelay(pdMS_TO_TICKS(250));
    }
  }
  if (!xStreamBufferIsEmpty(sDrainBuffer)) {
    clearDrainBuffer();
  }
}

auto AudioState::commitVolume() -> void {
  auto mode = sServices->nvs().OutputMode();
  auto vol = sOutput->GetVolume();
  if (mode == drivers::NvsStorage::Output::kHeadphones) {
    sServices->nvs().AmpCurrentVolume(vol);
  } else if (mode == drivers::NvsStorage::Output::kBluetooth) {
    auto dev = sServices->bluetooth().ConnectedDevice();
    if (!dev) {
      return;
    }
    sServices->nvs().BluetoothVolume(dev->mac, vol);
  }
}

namespace states {

void Uninitialised::react(const system_fsm::BootComplete& ev) {
  sServices = ev.services;

  ESP_LOGI(kTag, "allocating drain buffer, size %u KiB",
           kDrainBufferSize / 1024);

  auto meta = reinterpret_cast<StaticStreamBuffer_t*>(
      heap_caps_malloc(sizeof(StaticStreamBuffer_t), MALLOC_CAP_DMA));
  auto storage = reinterpret_cast<uint8_t*>(
      heap_caps_malloc(kDrainBufferSize, MALLOC_CAP_SPIRAM));

  sDrainBuffer = xStreamBufferCreateStatic(
      kDrainBufferSize, sizeof(sample::Sample), storage, meta);

  sFileSource.reset(
      new FatfsAudioInput(sServices->tag_parser(), sServices->bg_worker()));
  sI2SOutput.reset(new I2SAudioOutput(sDrainBuffer, sServices->gpios()));
  sBtOutput.reset(new BluetoothAudioOutput(sDrainBuffer, sServices->bluetooth(),
                                           sServices->bg_worker()));

  auto& nvs = sServices->nvs();
  sI2SOutput->SetMaxVolume(nvs.AmpMaxVolume());
  sI2SOutput->SetVolume(nvs.AmpCurrentVolume());
  sI2SOutput->SetVolumeImbalance(nvs.AmpLeftBias());

  if (sServices->nvs().OutputMode() ==
      drivers::NvsStorage::Output::kHeadphones) {
    sOutput = sI2SOutput;
  } else {
    // Ensure Bluetooth gets enabled if it's the default sink.
    sServices->bluetooth().Enable();
    sOutput = sBtOutput;
  }
  sOutput->mode(IAudioOutput::Modes::kOnPaused);

  events::Ui().Dispatch(VolumeLimitChanged{
      .new_limit_db =
          (static_cast<int>(nvs.AmpMaxVolume()) -
           static_cast<int>(drivers::wm8523::kLineLevelReferenceVolume)) /
          4,
  });
  events::Ui().Dispatch(VolumeChanged{
      .percent = sOutput->GetVolumePct(),
      .db = sOutput->GetVolumeDb(),
  });
  events::Ui().Dispatch(VolumeBalanceChanged{
      .left_bias = nvs.AmpLeftBias(),
  });

  sSampleConverter.reset(new SampleConverter());
  sSampleConverter->SetOutput(sOutput);

  Decoder::Start(sFileSource, sSampleConverter);

  transit<Standby>();
}

static const char kQueueKey[] = "audio:queue";
static const char kCurrentFileKey[] = "audio:current";

void Standby::react(const system_fsm::KeyLockChanged& ev) {
  if (!ev.locking) {
    return;
  }
  sServices->bg_worker().Dispatch<void>([this]() {
    auto db = sServices->database().lock();
    if (!db) {
      return;
    }
    auto& queue = sServices->track_queue();
    if (queue.totalSize() <= queue.currentPosition()) {
      // Nothing is playing, so don't bother saving the queue.
      db->put(kQueueKey, "");
      return;
    }
    db->put(kQueueKey, queue.serialise());

    if (sCurrentTrack) {
      cppbor::Array current_track{
          cppbor::Tstr{sCurrentTrack->uri},
          cppbor::Uint{currentPositionSeconds().value_or(0)},
      };
      db->put(kCurrentFileKey, current_track.toString());
    }
  });
}

void Standby::react(const system_fsm::SdStateChanged& ev) {
  auto state = sServices->sd();
  if (state != drivers::SdState::kMounted) {
    return;
  }
  sServices->bg_worker().Dispatch<void>([]() {
    auto db = sServices->database().lock();
    if (!db) {
      return;
    }

    // Restore the currently playing file before restoring the queue. This way,
    // we can fall back to restarting the queue's current track if there's any
    // issue restoring the current file.
    auto current = db->get(kCurrentFileKey);
    if (current) {
      // Again, ensure we don't boot-loop by trying to play a track that causes
      // a crash over and over again.
      db->put(kCurrentFileKey, "");
      auto [parsed, unused, err] = cppbor::parse(
          reinterpret_cast<uint8_t*>(current->data()), current->size());
      if (parsed->type() == cppbor::ARRAY) {
        std::string filename = parsed->asArray()->get(0)->asTstr()->value();
        uint32_t pos = parsed->asArray()->get(1)->asUint()->value();

        events::Audio().Dispatch(SetTrack{
            .new_track = filename,
            .seek_to_second = pos,
            .transition = SetTrack::Transition::kHardCut,
        });
      }
    }

    auto queue = db->get(kQueueKey);
    if (queue) {
      // Don't restore the same queue again. This ideally should do nothing,
      // but guards against bad edge cases where restoring the queue ends up
      // causing a crash.
      db->put(kQueueKey, "");
      sServices->track_queue().deserialise(*queue);
    }
  });
}

void Playback::entry() {
  ESP_LOGI(kTag, "audio output resumed");
  sOutput->mode(IAudioOutput::Modes::kOnPlaying);

  PlaybackUpdate event{
      .current_track = sCurrentTrack,
      .track_position = currentPositionSeconds(),
      .paused = false,
  };

  events::System().Dispatch(event);
  events::Ui().Dispatch(event);
}

void Playback::exit() {
  ESP_LOGI(kTag, "audio output paused");
  sOutput->mode(IAudioOutput::Modes::kOnPaused);

  PlaybackUpdate event{
      .current_track = sCurrentTrack,
      .track_position = currentPositionSeconds(),
      .paused = true,
  };

  events::System().Dispatch(event);
  events::Ui().Dispatch(event);
}

void Playback::react(const system_fsm::SdStateChanged& ev) {
  if (sServices->sd() != drivers::SdState::kMounted) {
    transit<Standby>();
  }
}

}  // namespace states

}  // namespace audio

FSM_INITIAL_STATE(audio::AudioState, audio::states::Uninitialised)
