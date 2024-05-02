/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "events/event_queue.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"

#include "audio/audio_fsm.hpp"
#include "system_fsm/system_fsm.hpp"
#include "ui/ui_fsm.hpp"

namespace events {

namespace queues {
static Queue sSystemAndAudio;
static Queue sUi;

auto SystemAndAudio() -> Queue* {
  return &sSystemAndAudio;
}

auto Ui() -> Queue* {
  return &sUi;
}
}  // namespace queues

static Dispatcher<system_fsm::SystemState> sSystem{queues::SystemAndAudio()};
static Dispatcher<audio::AudioState> sAudio{queues::SystemAndAudio()};
static Dispatcher<ui::UiState> sUi{queues::Ui()};

auto System() -> Dispatcher<system_fsm::SystemState>& {
  return sSystem;
}

auto Audio() -> Dispatcher<audio::AudioState>& {
  return sAudio;
}

auto Ui() -> Dispatcher<ui::UiState>& {
  return sUi;
}

}  // namespace events
