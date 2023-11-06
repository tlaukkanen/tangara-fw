/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "haptics.hpp"
#include <stdint.h>

#include <cstdint>
#include <initializer_list>
#include <mutex>

#include "assert.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "hal/i2c_types.h"

#include "i2c.hpp"

namespace drivers {

static constexpr char kTag[] = "haptics";
static constexpr uint8_t kHapticsAddress = 0x5A;

Haptics::Haptics() {
  // TODO(robin): is this needed?
  vTaskDelay(pdMS_TO_TICKS(300));

  PowerUp();

  // Put into ERM Open Loop:
  // (§8.5.4.1 Programming for ERM Open-Loop Operation)
  // - Turn off N_ERM_LRA first
  WriteRegister(Register::kControl1,
                static_cast<uint8_t>(RegisterDefaults::kControl1) &
                    (~ControlMask::kNErmLra));
  // - Turn on ERM_OPEN_LOOP
  WriteRegister(Register::kControl3,
                static_cast<uint8_t>(RegisterDefaults::kControl3) |
                    ControlMask::kErmOpenLoop);

  // Set library
  // TODO(robin): try the other libraries and test response. C is marginal, D
  // too much?
  WriteRegister(Register::kWaveformLibrary, static_cast<uint8_t>(kDefaultLibrary));

  // Set mode (internal trigger, on writing 1 to Go register)
  WriteRegister(Register::kMode, static_cast<uint8_t>(Mode::kInternalTrigger));

  // Set up a default effect (sequence of one effect)
  SetWaveformEffect(kStartupEffect);
}

Haptics::~Haptics() {}

void Haptics::WriteRegister(Register reg, uint8_t val) {
  uint8_t regRaw = static_cast<uint8_t>(reg);
  I2CTransaction transaction;
  transaction.start()
      .write_addr(kHapticsAddress, I2C_MASTER_WRITE)
      .write_ack(regRaw, val)
      .stop();
  esp_err_t res = transaction.Execute(1);
  if (res != ESP_OK) {
    ESP_LOGW(kTag, "write failed: %s", esp_err_to_name(res));
  }
}

auto Haptics::PlayWaveformEffect(Effect effect) -> void {
  const std::lock_guard<std::mutex> lock{playing_effect_};  // locks until freed

  SetWaveformEffect(effect);
  Go();
}

// Starts the pre-programmed sequence
auto Haptics::Go() -> void {
  WriteRegister(Register::kGo,
                static_cast<uint8_t>(RegisterDefaults::kGo) | 0b00000001);
}

auto Haptics::SetWaveformEffect(Effect effect) -> void {
  if (!current_effect_ || current_effect_.value() != effect) {
    WriteRegister(Register::kWaveformSequenceSlot1,
                  static_cast<uint8_t>(effect));
    WriteRegister(Register::kWaveformSequenceSlot2,
                  static_cast<uint8_t>(Effect::kStop));
  }
  current_effect_ = effect;
}


auto Haptics::TourEffects() -> void {
  TourEffects(Effect::kFirst, Effect::kLast, kDefaultLibrary);
}
auto Haptics::TourEffects(Library lib) -> void {
  TourEffects(Effect::kFirst, Effect::kLast, lib);
}
auto Haptics::TourEffects(Effect from, Effect to) -> void {
  TourEffects(from, to, kDefaultLibrary);
}
auto Haptics::TourEffects(Effect from, Effect to, Library lib) -> void {
  ESP_LOGI(kTag, "With library #%u...", static_cast<uint8_t>(lib));

  for (uint8_t e = static_cast<uint8_t>(from);
       e <= static_cast<uint8_t>(to) &&
       e <= static_cast<uint8_t>(Effect::kLast);
       e++) {
    auto effect = static_cast<Effect>(e);
    auto label = EffectToLabel(effect);

    if (effect == Effect::kDontUseThis_Longbuzzforprogrammaticstopping_100Pct) {
      ESP_LOGI(kTag, "Ignoring effect '%s'...", label.c_str());
      continue;
    }

    ESP_LOGI(kTag, "Playing effect #%u: %s", e, label.c_str());
    PlayWaveformEffect(effect);
    Go();

    vTaskDelay(pdMS_TO_TICKS(800 /*ms*/));
  }
}

auto Haptics::TourLibraries(Effect from, Effect to) -> void {
  for (uint8_t lib = 1; lib <= 5; lib++) {
    WriteRegister(Register::kWaveformLibrary, lib);

    for (auto e = static_cast<uint8_t>(Effect::kFirst);
         e <= static_cast<uint8_t>(Effect::kLast); e++) {
      auto effect = static_cast<Effect>(e);
      ESP_LOGI(kTag, "Library %u, Effect: %s", lib,
               EffectToLabel(effect).c_str());

      PlayWaveformEffect(effect);
      Go();

      vTaskDelay(pdMS_TO_TICKS(800 /*ms*/));
    }
  }
}


auto Haptics::PowerDown() -> void {
  WriteRegister(Register::kMode, static_cast<uint8_t>(Mode::kInternalTrigger) |
                                     ModeMask::kStandby);
}

auto Haptics::Reset() -> void {
  WriteRegister(Register::kMode, static_cast<uint8_t>(Mode::kInternalTrigger) |
                                     ModeMask::kDevReset);
}

auto Haptics::PowerUp() -> void {
  // FIXME: technically overwriting the RESERVED bits of Mode, but eh
  uint8_t newMask = static_cast<uint8_t>(Mode::kInternalTrigger) &
                    (~ModeMask::kStandby) & (~ModeMask::kDevReset);
  WriteRegister(Register::kMode,
                static_cast<uint8_t>(RegisterDefaults::kMode) | newMask);
}

auto Haptics::EffectToLabel(Effect effect) -> std::string {
  switch (static_cast<Effect>(effect)) {
    case Effect::kStrongClick_100Pct:
      return "Strong Click - 100%";
    case Effect::kStrongClick_60Pct:
      return "Strong Click (60%)";
    case Effect::kStrongClick_30Pct:
      return "Strong Click (30%)";
    case Effect::kSharpClick_100Pct:
      return "Sharp Click (100%)";
    case Effect::kSharpClick_60Pct:
      return "Sharp Click (60%)";
    case Effect::kSharpClick_30Pct:
      return "Sharp Click (30%)";
    case Effect::kSoftBump_100Pct:
      return "Soft Bump (100%)";
    case Effect::kSoftBump_60Pct:
      return "Soft Bump (60%)";
    case Effect::kSoftBump_30Pct:
      return "Soft Bump (30%)";
    case Effect::kDoubleClick_100Pct:
      return "Double Click (100%)";
    case Effect::kDoubleClick_60Pct:
      return "Double Click (60%)";
    case Effect::kTripleClick_100Pct:
      return "Triple Click (100%)";
    case Effect::kSoftFuzz_60Pct:
      return "Soft Fuzz (60%)";
    case Effect::kStrongBuzz_100Pct:
      return "Strong Buzz (100%)";
    case Effect::k750msAlert_100Pct:
      return "750ms Alert (100%)";
    case Effect::k1000msAlert_100Pct:
      return "1000ms Alert (100%)";
    case Effect::kStrongClick1_100Pct:
      return "Strong Click1 (100%)";
    case Effect::kStrongClick2_80Pct:
      return "Strong Click2 (80%)";
    case Effect::kStrongClick3_60Pct:
      return "Strong Click3 (60%)";
    case Effect::kStrongClick4_30Pct:
      return "Strong Click4 (30%)";
    case Effect::kMediumClick1_100Pct:
      return "Medium Click1 (100%)";
    case Effect::kMediumClick2_80Pct:
      return "Medium Click2 (80%)";
    case Effect::kMediumClick3_60Pct:
      return "Medium Click3 (60%)";
    case Effect::kSharpTick1_100Pct:
      return "Sharp Tick1 (100%)";
    case Effect::kSharpTick2_80Pct:
      return "Sharp Tick2 (80%)";
    case Effect::kSharpTick3_60Pct:
      return "Sharp Tick3 (60%)";
    case Effect::kShortDoubleClickStrong1_100Pct:
      return "Short Double Click Strong1 (100%)";
    case Effect::kShortDoubleClickStrong2_80Pct:
      return "Short Double Click Strong2 (80%)";
    case Effect::kShortDoubleClickStrong3_60Pct:
      return "Short Double Click Strong3 (60%)";
    case Effect::kShortDoubleClickStrong4_30Pct:
      return "Short Double Click Strong4 (30%)";
    case Effect::kShortDoubleClickMedium1_100Pct:
      return "Short Double Click Medium1 (100%)";
    case Effect::kShortDoubleClickMedium2_80Pct:
      return "Short Double Click Medium2 (80%)";
    case Effect::kShortDoubleClickMedium3_60Pct:
      return "Short Double Click Medium3 (60%)";
    case Effect::kShortDoubleSharpTick1_100Pct:
      return "Short Double Sharp Tick1 (100%)";
    case Effect::kShortDoubleSharpTick2_80Pct:
      return "Short Double Sharp Tick2 (80%)";
    case Effect::kShortDoubleSharpTick3_60Pct:
      return "Short Double Sharp Tick3 (60%)";
    case Effect::kLongDoubleSharpClickStrong1_100Pct:
      return "Long Double Sharp Click Strong1 (100%)";
    case Effect::kLongDoubleSharpClickStrong2_80Pct:
      return "Long Double Sharp Click Strong2 (80%)";
    case Effect::kLongDoubleSharpClickStrong3_60Pct:
      return "Long Double Sharp Click Strong3 (60%)";
    case Effect::kLongDoubleSharpClickStrong4_30Pct:
      return "Long Double Sharp Click Strong4 (30%)";
    case Effect::kLongDoubleSharpClickMedium1_100Pct:
      return "Long Double Sharp Click Medium1 (100%)";
    case Effect::kLongDoubleSharpClickMedium2_80Pct:
      return "Long Double Sharp Click Medium2 (80%)";
    case Effect::kLongDoubleSharpClickMedium3_60Pct:
      return "Long Double Sharp Click Medium3 (60%)";
    case Effect::kLongDoubleSharpTick1_100Pct:
      return "Long Double Sharp Tick1 (100%)";
    case Effect::kLongDoubleSharpTick2_80Pct:
      return "Long Double Sharp Tick2 (80%)";
    case Effect::kLongDoubleSharpTick3_60Pct:
      return "Long Double Sharp Tick3 (60%)";
    case Effect::kBuzz1_100Pct:
      return "Buzz1 (100%)";
    case Effect::kBuzz2_80Pct:
      return "Buzz2 (80%)";
    case Effect::kBuzz3_60Pct:
      return "Buzz3 (60%)";
    case Effect::kBuzz4_40Pct:
      return "Buzz4 (40%)";
    case Effect::kBuzz5_20Pct:
      return "Buzz5 (20%)";
    case Effect::kPulsingStrong1_100Pct:
      return "Pulsing Strong1 (100%)";
    case Effect::kPulsingStrong2_60Pct:
      return "Pulsing Strong2 (60%)";
    case Effect::kPulsingMedium1_100Pct:
      return "Pulsing Medium1 (100%)";
    case Effect::kPulsingMedium2_60Pct:
      return "Pulsing Medium2 (60%)";
    case Effect::kPulsingSharp1_100Pct:
      return "Pulsing Sharp1 (100%)";
    case Effect::kPulsingSharp2_60Pct:
      return "Pulsing Sharp2 (60%)";
    case Effect::kTransitionClick1_100Pct:
      return "Transition Click1 (100%)";
    case Effect::kTransitionClick2_80Pct:
      return "Transition Click2 (80%)";
    case Effect::kTransitionClick3_60Pct:
      return "Transition Click3 (60%)";
    case Effect::kTransitionClick4_40Pct:
      return "Transition Click4 (40%)";
    case Effect::kTransitionClick5_20Pct:
      return "Transition Click5 (20%)";
    case Effect::kTransitionClick6_10Pct:
      return "Transition Click6 (10%)";
    case Effect::kTransitionHum1_100Pct:
      return "Transition Hum1 (100%)";
    case Effect::kTransitionHum2_80Pct:
      return "Transition Hum2 (80%)";
    case Effect::kTransitionHum3_60Pct:
      return "Transition Hum3 (60%)";
    case Effect::kTransitionHum4_40Pct:
      return "Transition Hum4 (40%)";
    case Effect::kTransitionHum5_20Pct:
      return "Transition Hum5 (20%)";
    case Effect::kTransitionHum6_10Pct:
      return "Transition Hum6 (10%)";

    // TODO: fix labels for XtoY-style
    case Effect::kTransitionRampDownLongSmooth1_100to0Pct:
      return "Transition Ramp Down Long Smooth1 (100→0%)";
    case Effect::kTransitionRampDownLongSmooth2_100to0Pct:
      return "Transition Ramp Down Long Smooth2 (100→0%)";
    case Effect::kTransitionRampDownMediumSmooth1_100to0Pct:
      return "Transition Ramp Down Medium Smooth1 (100→0%)";
    case Effect::kTransitionRampDownMediumSmooth2_100to0Pct:
      return "Transition Ramp Down Medium Smooth2 (100→0%)";
    case Effect::kTransitionRampDownShortSmooth1_100to0Pct:
      return "Transition Ramp Down Short Smooth1 (100→0%)";
    case Effect::kTransitionRampDownShortSmooth2_100to0Pct:
      return "Transition Ramp Down Short Smooth2 (100→0%)";
    case Effect::kTransitionRampDownLongSharp1_100to0Pct:
      return "Transition Ramp Down Long Sharp1 (100→0%)";
    case Effect::kTransitionRampDownLongSharp2_100to0Pct:
      return "Transition Ramp Down Long Sharp2 (100→0%)";
    case Effect::kTransitionRampDownMediumSharp1_100to0Pct:
      return "Transition Ramp Down Medium Sharp1 (100→0%)";
    case Effect::kTransitionRampDownMediumSharp2_100to0Pct:
      return "Transition Ramp Down Medium Sharp2 (100→0%)";
    case Effect::kTransitionRampDownShortSharp1_100to0Pct:
      return "Transition Ramp Down Short Sharp1 (100→0%)";
    case Effect::kTransitionRampDownShortSharp2_100to0Pct:
      return "Transition Ramp Down Short Sharp2 (100→0%)";
    case Effect::kTransitionRampUpLongSmooth1_0to100Pct:
      return "Transition Ramp Up Long Smooth1 (0→100%)";
    case Effect::kTransitionRampUpLongSmooth2_0to100Pct:
      return "Transition Ramp Up Long Smooth2 (0→100%)";
    case Effect::kTransitionRampUpMediumSmooth1_0to100Pct:
      return "Transition Ramp Up Medium Smooth1 (0→100%)";
    case Effect::kTransitionRampUpMediumSmooth2_0to100Pct:
      return "Transition Ramp Up Medium Smooth2 (0→100%)";
    case Effect::kTransitionRampUpShortSmooth1_0to100Pct:
      return "Transition Ramp Up Short Smooth1 (0→100%)";
    case Effect::kTransitionRampUpShortSmooth2_0to100Pct:
      return "Transition Ramp Up Short Smooth2 (0→100%)";
    case Effect::kTransitionRampUpLongSharp1_0to100Pct:
      return "Transition Ramp Up Long Sharp1 (0→100%)";
    case Effect::kTransitionRampUpLongSharp2_0to100Pct:
      return "Transition Ramp Up Long Sharp2 (0→100%)";
    case Effect::kTransitionRampUpMediumSharp1_0to100Pct:
      return "Transition Ramp Up Medium Sharp1 (0→100%)";
    case Effect::kTransitionRampUpMediumSharp2_0to100Pct:
      return "Transition Ramp Up Medium Sharp2 (0→100%)";
    case Effect::kTransitionRampUpShortSharp1_0to100Pct:
      return "Transition Ramp Up Short Sharp1 (0→100%)";
    case Effect::kTransitionRampUpShortSharp2_0to100Pct:
      return "Transition Ramp Up Short Sharp2 (0→100%)";
    case Effect::kTransitionRampDownLongSmooth1_50to0Pct:
      return "Transition Ramp Down Long Smooth1 (50→0%)";
    case Effect::kTransitionRampDownLongSmooth2_50to0Pct:
      return "Transition Ramp Down Long Smooth2 (50→0%)";
    case Effect::kTransitionRampDownMediumSmooth1_50to0Pct:
      return "Transition Ramp Down Medium Smooth1 (50→0%)";
    case Effect::kTransitionRampDownMediumSmooth2_50to0Pct:
      return "Transition Ramp Down Medium Smooth2 (50→0%)";
    case Effect::kTransitionRampDownShortSmooth1_50to0Pct:
      return "Transition Ramp Down Short Smooth1 (50→0%)";
    case Effect::kTransitionRampDownShortSmooth2_50to0Pct:
      return "Transition Ramp Down Short Smooth2 (50→0%)";
    case Effect::kTransitionRampDownLongSharp1_50to0Pct:
      return "Transition Ramp Down Long Sharp1 (50→0%)";
    case Effect::kTransitionRampDownLongSharp2_50to0Pct:
      return "Transition Ramp Down Long Sharp2 (50→0%)";
    case Effect::kTransitionRampDownMediumSharp1_50to0Pct:
      return "Transition Ramp Down Medium Sharp1 (50→0%)";
    case Effect::kTransitionRampDownMediumSharp2_50to0Pct:
      return "Transition Ramp Down Medium Sharp2 (50→0%)";
    case Effect::kTransitionRampDownShortSharp1_50to0Pct:
      return "Transition Ramp Down Short Sharp1 (50→0%)";
    case Effect::kTransitionRampDownShortSharp2_50to0Pct:
      return "Transition Ramp Down Short Sharp2 (50→0%)";
    case Effect::kTransitionRampUpLongSmooth_10to50Pct:
      return "Transition Ramp Up Long Smooth10to (10→50%)";
    case Effect::kTransitionRampUpLongSmooth_20to50Pct:
      return "Transition Ramp Up Long Smooth20to (10→50%)";
    case Effect::kTransitionRampUpMediumSmooth_10to50Pct:
      return "Transition Ramp Up Medium Smooth (10→50%)";
    case Effect::kTransitionRampUpMediumSmooth_20to50Pct:
      return "Transition Ramp Up Medium Smooth (20→50%)";
    case Effect::kTransitionRampUpShortSmooth_10to50Pct:
      return "Transition Ramp Up Short Smooth (10→50%)";
    case Effect::kTransitionRampUpShortSmooth_20to50Pct:
      return "Transition Ramp Up Short Smooth (20→50%)";
    case Effect::kTransitionRampUpLongSharp_10to50Pct:
      return "Transition Ramp Up Long Sharp (10→50%)";
    case Effect::kTransitionRampUpLongSharp_20to50Pct:
      return "Transition Ramp Up Long Sharp (20→50%)";
    case Effect::kTransitionRampUpMediumSharp_10to50Pct:
      return "Transition Ramp Up Medium Sharp (10→50%)";
    case Effect::kTransitionRampUpMediumSharp_20to50Pct:
      return "Transition Ramp Up Medium Sharp (20→50%)";
    case Effect::kTransitionRampUpShortSharp_10to50Pct:
      return "Transition Ramp Up Short Sharp (10→50%)";
    case Effect::kTransitionRampUpShortSharp_20to50Pct:
      return "Transition Ramp Up Short Sharp (20→50%)";
    case Effect::kDontUseThis_Longbuzzforprogrammaticstopping_100Pct:
      return "DON'T USE: Long Buzz for Programmatic Stopping (100%)";
    case Effect::kSmoothHum1NoKickOrBrakePulse_50Pct:
      return "Smooth Hum1 No Kick Or Brake Pulse (50%)";
    case Effect::kSmoothHum2NoKickOrBrakePulse_40Pct:
      return "Smooth Hum2 No Kick Or Brake Pulse (40%)";
    case Effect::kSmoothHum3NoKickOrBrakePulse_30Pct:
      return "Smooth Hum3 No Kick Or Brake Pulse (30%)";
    case Effect::kSmoothHum4NoKickOrBrakePulse_20Pct:
      return "Smooth Hum4 No Kick Or Brake Pulse (20%)";
    case Effect::kSmoothHum5NoKickOrBrakePulse_10Pct:
      return "Smooth Hum5 No Kick Or Brake Pulse (10%)";
    default:
      return "UNKNOWN EFFECT";
  }
}

}  // namespace drivers
