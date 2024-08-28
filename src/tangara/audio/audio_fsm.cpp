/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio/audio_fsm.hpp"

#include <cstdint>
#include <future>
#include <memory>
#include <sstream>
#include <variant>

#include "audio/audio_source.hpp"
#include "audio/sine_source.hpp"
#include "cppbor.h"
#include "cppbor_parse.h"
#include "drivers/pcm_buffer.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "tinyfsm.hpp"

#include "audio/audio_decoder.hpp"
#include "audio/audio_events.hpp"
#include "audio/audio_sink.hpp"
#include "audio/bt_audio_output.hpp"
#include "audio/fatfs_stream_factory.hpp"
#include "audio/i2s_audio_output.hpp"
#include "audio/stream_cues.hpp"
#include "audio/track_queue.hpp"
#include "database/future_fetcher.hpp"
#include "database/track.hpp"
#include "drivers/bluetooth.hpp"
#include "drivers/bluetooth_types.hpp"
#include "drivers/i2s_dac.hpp"
#include "drivers/nvs.hpp"
#include "drivers/storage.hpp"
#include "drivers/wm8523.hpp"
#include "events/event_queue.hpp"
#include "sample.hpp"
#include "system_fsm/service_locator.hpp"
#include "system_fsm/system_events.hpp"

namespace audio {

[[maybe_unused]] static const char kTag[] = "audio_fsm";

std::shared_ptr<system_fsm::ServiceLocator> AudioState::sServices;

std::shared_ptr<FatfsStreamFactory> AudioState::sStreamFactory;

std::unique_ptr<Decoder> AudioState::sDecoder;
std::shared_ptr<SampleProcessor> AudioState::sSampleProcessor;

std::shared_ptr<IAudioOutput> AudioState::sOutput;
std::shared_ptr<I2SAudioOutput> AudioState::sI2SOutput;
std::shared_ptr<BluetoothAudioOutput> AudioState::sBtOutput;

// Two seconds of samples for two channels, at a representative sample rate.
constexpr size_t kDrainLatencySamples = 48000 * 2 * 2;

std::unique_ptr<drivers::PcmBuffer> AudioState::sDrainBuffer;
std::optional<IAudioOutput::Format> AudioState::sDrainFormat;

StreamCues AudioState::sStreamCues;

bool AudioState::sIsPaused = true;

auto AudioState::emitPlaybackUpdate(bool paused) -> void {
  std::optional<uint32_t> position;
  auto current = sStreamCues.current();
  if (current.first && sDrainFormat) {
    position = ((current.second +
                 (sDrainFormat->num_channels * sDrainFormat->sample_rate / 2)) /
                (sDrainFormat->num_channels * sDrainFormat->sample_rate)) +
               current.first->start_offset.value_or(0);
  }

  PlaybackUpdate event{
      .current_track = current.first,
      .track_position = position,
      .paused = paused,
  };

  events::System().Dispatch(event);
  events::Ui().Dispatch(event);
}

void AudioState::react(const QueueUpdate& ev) {
  SetTrack cmd{
      .new_track = std::monostate{},
      .seek_to_second = {},
  };

  auto current = sServices->track_queue().current();
  cmd.new_track = current;

  switch (ev.reason) {
    case QueueUpdate::kExplicitUpdate:
      if (!ev.current_changed) {
        return;
      }
      break;
    case QueueUpdate::kRepeatingLastTrack:
      break;
    case QueueUpdate::kTrackFinished:
      if (!ev.current_changed) {
        cmd.new_track = std::monostate{};
      }
      break;
    case QueueUpdate::kBulkLoadingUpdate:
      // Bulk loading updates are informational only; a separate QueueUpdate
      // event will be sent when loading is done.
    case QueueUpdate::kDeserialised:
      // The current track is deserialised separately in order to retain seek
      // position.
    default:
      return;
  }

  tinyfsm::FsmList<AudioState>::dispatch(cmd);
}

void AudioState::react(const SetTrack& ev) {
  if (std::holds_alternative<std::monostate>(ev.new_track)) {
    ESP_LOGI(kTag, "playback finished, awaiting drain");
    sDecoder->open({});
    return;
  }

  // Move the rest of the work to a background worker, since it may require db
  // lookups to resolve a track id into a path.
  auto new_track = ev.new_track;
  uint32_t seek_to = ev.seek_to_second.value_or(0);
  sServices->bg_worker().Dispatch<void>([=]() {
    std::shared_ptr<TaggedStream> stream;
    if (std::holds_alternative<database::TrackId>(new_track)) {
      stream = sStreamFactory->create(std::get<database::TrackId>(new_track),
                                      seek_to);
    } else if (std::holds_alternative<std::string>(new_track)) {
      stream =
          sStreamFactory->create(std::get<std::string>(new_track), seek_to);
    }

    // Always give the stream to the decoder, even if it turns out to be empty.
    // This has the effect of stopping the current playback, which is generally
    // what the user expects to happen when they say "Play this track!", even
    // if the new track has an issue.
    sDecoder->open(stream);

    // ...but if the stream that failed is the front of the queue, then we
    // should advance to the next track in order to keep the tunes flowing.
    if (!stream) {
      auto& queue = sServices->track_queue();
      if (new_track == queue.current()) {
        queue.finish();
      }
    }
  });
}

void AudioState::react(const PlaySineWave& ev) {
  auto tags = std::make_shared<database::TrackTags>();

  std::stringstream title;
  title << ev.frequency << "Hz Sine Wave";
  tags->title(title.str());

  sDecoder->open(std::make_shared<TaggedStream>(
      tags, std::make_unique<SineSource>(ev.frequency), title.str()));
}

void AudioState::react(const TogglePlayPause& ev) {
  sIsPaused = !ev.set_to.value_or(sIsPaused);
  if (!sIsPaused && is_in_state<states::Standby>() &&
      sStreamCues.current().first) {
    transit<states::Playback>();
  } else if (sIsPaused && is_in_state<states::Playback>()) {
    transit<states::Standby>();
  }
}

void AudioState::react(const internal::DecodingFinished& ev) {
  // If we just finished playing whatever's at the front of the queue, then we
  // need to advanve and start playing the next one ASAP in order to continue
  // gaplessly.
  sServices->bg_worker().Dispatch<void>([=]() {
    auto& queue = sServices->track_queue();
    auto current = queue.current();
    if (std::holds_alternative<std::monostate>(current)) {
      return;
    }
    auto db = sServices->database().lock();
    if (!db) {
      return;
    }
    std::string path;
    if (std::holds_alternative<std::string>(current)) {
      path = std::get<std::string>(current);
    } else if (std::holds_alternative<database::TrackId>(current)) {
      auto tid = std::get<database::TrackId>(current);
      path = db->getTrackPath(tid).value_or("");
    }
    if (path == ev.track->uri) {
      queue.finish();
    }
  });
}

void AudioState::react(const internal::StreamStarted& ev) {
  if (sDrainFormat != ev.sink_format) {
    sDrainFormat = ev.sink_format;
    ESP_LOGI(kTag, "sink_format=%u ch @ %lu hz", sDrainFormat->num_channels,
             sDrainFormat->sample_rate);
  }

  sStreamCues.addCue(ev.track, ev.cue_at_sample);
  sStreamCues.update(sDrainBuffer->totalReceived());

  if (!sIsPaused && !is_in_state<states::Playback>()) {
    transit<states::Playback>();
  } else {
    // Make sure everyone knows we've got a track ready to go, even if we're
    // not playing it yet. This mostly matters when restoring the queue from
    // disk after booting.
    emitPlaybackUpdate(true);
  }
}

void AudioState::react(const internal::StreamEnded& ev) {
  sStreamCues.addCue({}, ev.cue_at_sample);
}

void AudioState::react(const system_fsm::HasPhonesChanged& ev) {
  if (ev.has_headphones) {
    events::Audio().Dispatch(audio::OutputModeChanged{
        .set_to = drivers::NvsStorage::Output::kHeadphones});
  } else {
    if (sServices->bluetooth().enabled()) {
      events::Audio().Dispatch(audio::OutputModeChanged{
          .set_to = drivers::NvsStorage::Output::kBluetooth});
    }
  }
}

void AudioState::react(const system_fsm::BluetoothEvent& ev) {
  using drivers::bluetooth::SimpleEvent;
  if (std::holds_alternative<SimpleEvent>(ev.event)) {
    auto simpleEvent = std::get<SimpleEvent>(ev.event);
    switch (simpleEvent) {
      case SimpleEvent::kConnectionStateChanged: {
        auto bt = sServices->bluetooth();
        if (bt.connectionState() !=
            drivers::Bluetooth::ConnectionState::kConnected) {
          return;
        }
        auto dev = sServices->bluetooth().pairedDevice();
        if (!dev) {
          return;
        }
        sBtOutput->SetVolume(sServices->nvs().BluetoothVolume(dev->mac));
        events::Ui().Dispatch(VolumeChanged{
            .percent = sOutput->GetVolumePct(),
            .db = sOutput->GetVolumeDb(),
        });
        break;
      }
      default:
        break;
    }
  }
  if (std::holds_alternative<drivers::bluetooth::RemoteVolumeChanged>(
          ev.event)) {
    auto volume_chg =
        std::get<drivers::bluetooth::RemoteVolumeChanged>(ev.event).new_vol;
    events::Ui().Dispatch(RemoteVolumeChanged{.value = volume_chg});
  }
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
  if (ev.set_to) {
    new_mode = *ev.set_to;
  }
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
  sSampleProcessor->SetOutput(sOutput);

  // Bluetooth volume isn't 'changed' until we've connected to a device.
  if (new_mode == drivers::NvsStorage::Output::kHeadphones) {
    events::Ui().Dispatch(VolumeChanged{
        .percent = sOutput->GetVolumePct(),
        .db = sOutput->GetVolumeDb(),
    });
  }
}

auto AudioState::commitVolume() -> void {
  auto mode = sServices->nvs().OutputMode();
  auto vol = sOutput->GetVolume();
  if (mode == drivers::NvsStorage::Output::kHeadphones) {
    sServices->nvs().AmpCurrentVolume(vol);
  } else if (mode == drivers::NvsStorage::Output::kBluetooth) {
    auto dev = sServices->bluetooth().pairedDevice();
    if (!dev) {
      return;
    }
    sServices->nvs().BluetoothVolume(dev->mac, vol);
  }
}

namespace states {

void Uninitialised::react(const system_fsm::BootComplete& ev) {
  sServices = ev.services;

  sDrainBuffer = std::make_unique<drivers::PcmBuffer>(kDrainLatencySamples);

  sStreamFactory.reset(
      new FatfsStreamFactory(sServices->database(), sServices->tag_parser()));
  sI2SOutput.reset(new I2SAudioOutput(sServices->gpios(), *sDrainBuffer));
  sBtOutput.reset(new BluetoothAudioOutput(
      sServices->bluetooth(), *sDrainBuffer, sServices->bg_worker()));

  auto& nvs = sServices->nvs();
  sI2SOutput->SetMaxVolume(nvs.AmpMaxVolume());
  sI2SOutput->SetVolume(nvs.AmpCurrentVolume());
  sI2SOutput->SetVolumeImbalance(nvs.AmpLeftBias());

  if (sServices->nvs().OutputMode() ==
      drivers::NvsStorage::Output::kHeadphones) {
    sOutput = sI2SOutput;
  } else {
    // Ensure Bluetooth gets enabled if it's the default sink.
    sServices->bluetooth().enable(true);
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

  sSampleProcessor.reset(new SampleProcessor(*sDrainBuffer));
  sSampleProcessor->SetOutput(sOutput);

  sDecoder.reset(Decoder::Start(sSampleProcessor));

  transit<Standby>();
}

static const char kQueueKey[] = "audio:queue";
static const char kCurrentFileKey[] = "audio:current";

void Standby::react(const system_fsm::KeyLockChanged& ev) {
  if (!ev.locking) {
    return;
  }
  auto current = sStreamCues.current();
  sServices->bg_worker().Dispatch<void>([=]() {
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

    if (current.first && sDrainFormat) {
      uint32_t seconds = (current.second / (sDrainFormat->num_channels *
                                            sDrainFormat->sample_rate)) +
                         current.first->start_offset.value_or(0);
      cppbor::Array current_track{
          cppbor::Tstr{current.first->uri},
          cppbor::Uint{seconds},
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

    // Open the queue file
    sServices->track_queue().open();

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

static TimerHandle_t sHeartbeatTimer;

static void heartbeat(TimerHandle_t) {
  events::Audio().Dispatch(internal::StreamHeartbeat{});
}

void Playback::entry() {
  ESP_LOGI(kTag, "audio output resumed");
  sOutput->mode(IAudioOutput::Modes::kOnPlaying);
  emitPlaybackUpdate(false);

  if (!sHeartbeatTimer) {
    sHeartbeatTimer =
        xTimerCreate("stream", pdMS_TO_TICKS(1000), true, NULL, heartbeat);
  }
  xTimerStart(sHeartbeatTimer, portMAX_DELAY);
}

void Playback::exit() {
  ESP_LOGI(kTag, "audio output paused");
  xTimerStop(sHeartbeatTimer, portMAX_DELAY);
  sOutput->mode(IAudioOutput::Modes::kOnPaused);
  emitPlaybackUpdate(true);
}

void Playback::react(const system_fsm::SdStateChanged& ev) {
  if (sServices->sd() != drivers::SdState::kMounted) {
    transit<Standby>();
  }
}

void Playback::react(const internal::StreamHeartbeat& ev) {
  sStreamCues.update(sDrainBuffer->totalReceived());

  if (sStreamCues.hasStream()) {
    emitPlaybackUpdate(false);
  } else {
    // Finished the current stream, and there's nothing upcoming. We must be
    // finished.
    transit<Standby>();
  }
}

}  // namespace states

}  // namespace audio

FSM_INITIAL_STATE(audio::AudioState, audio::states::Uninitialised)
