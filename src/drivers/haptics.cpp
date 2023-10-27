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
  WriteRegister(Register::kWaveformLibrary, static_cast<uint8_t>(Library::D));

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

auto Haptics::TourLibraries() -> void {
  // TODO(robin): try the other libraries and test response
  for (uint8_t lib = 1; lib <= 5; lib++) {
    WriteRegister(Register::kWaveformLibrary, lib);
    for (uint8_t eff = 1; eff <= 3; eff++) {
      ESP_LOGI(kTag, "Lib %u, # %d", lib, eff);
      SetWaveformEffect(static_cast<Effect>(44));
      Go();
      vTaskDelay(pdMS_TO_TICKS(900 /*ms*/));
    }
  }
}

// TODO: parameterise? remove?
auto Haptics::Tour() -> void {
  ESP_LOGI(kTag, "%s", "=== Waveform Library Effects List ===");

  auto label = "";
  for (uint8_t effect = 1; effect <= 123; effect++) {
    switch (static_cast<Effect>(effect)) {
      case Effect::kStrongClick_100Pct:
        label = "Strong Click - 100%";
        break;
      case Effect::kStrongClick_60Pct:
        label = "Strong Click (60%)";
        break;
      case Effect::kStrongClick_30Pct:
        label = "Strong Click (30%)";
        break;
      case Effect::kSharpClick_100Pct:
        label = "Sharp Click (100%)";
        break;
      case Effect::kSharpClick_60Pct:
        label = "Sharp Click (60%)";
        break;
      case Effect::kSharpClick_30Pct:
        label = "Sharp Click (30%)";
        break;
      case Effect::kSoftBump_100Pct:
        label = "Soft Bump (100%)";
        break;
      case Effect::kSoftBump_60Pct:
        label = "Soft Bump (60%)";
        break;
      case Effect::kSoftBump_30Pct:
        label = "Soft Bump (30%)";
        break;
      case Effect::kDoubleClick_100Pct:
        label = "Double Click (100%)";
        break;
      case Effect::kDoubleClick_60Pct:
        label = "Double Click (60%)";
        break;
      case Effect::kTripleClick_100Pct:
        label = "Triple Click (100%)";
        break;
      case Effect::kSoftFuzz_60Pct:
        label = "Soft Fuzz (60%)";
        break;
      case Effect::kStrongBuzz_100Pct:
        label = "Strong Buzz (100%)";
        break;
      case Effect::k750msAlert_100Pct:
        label = "750ms Alert (100%)";
        break;
      case Effect::k1000msAlert_100Pct:
        label = "1000ms Alert (100%)";
        break;
      case Effect::kStrongClick1_100Pct:
        label = "Strong Click1 (100%)";
        break;
      case Effect::kStrongClick2_80Pct:
        label = "Strong Click2 (80%)";
        break;
      case Effect::kStrongClick3_60Pct:
        label = "Strong Click3 (60%)";
        break;
      case Effect::kStrongClick4_30Pct:
        label = "Strong Click4 (30%)";
        break;
      case Effect::kMediumClick1_100Pct:
        label = "Medium Click1 (100%)";
        break;
      case Effect::kMediumClick2_80Pct:
        label = "Medium Click2 (80%)";
        break;
      case Effect::kMediumClick3_60Pct:
        label = "Medium Click3 (60%)";
        break;
      case Effect::kSharpTick1_100Pct:
        label = "Sharp Tick1 (100%)";
        break;
      case Effect::kSharpTick2_80Pct:
        label = "Sharp Tick2 (80%)";
        break;
      case Effect::kSharpTick3_60Pct:
        label = "Sharp Tick3 (60%)";
        break;
      case Effect::kShortDoubleClickStrong1_100Pct:
        label = "Short Double Click Strong1 (100%)";
        break;
      case Effect::kShortDoubleClickStrong2_80Pct:
        label = "Short Double Click Strong2 (80%)";
        break;
      case Effect::kShortDoubleClickStrong3_60Pct:
        label = "Short Double Click Strong3 (60%)";
        break;
      case Effect::kShortDoubleClickStrong4_30Pct:
        label = "Short Double Click Strong4 (30%)";
        break;
      case Effect::kShortDoubleClickMedium1_100Pct:
        label = "Short Double Click Medium1 (100%)";
        break;
      case Effect::kShortDoubleClickMedium2_80Pct:
        label = "Short Double Click Medium2 (80%)";
        break;
      case Effect::kShortDoubleClickMedium3_60Pct:
        label = "Short Double Click Medium3 (60%)";
        break;
      case Effect::kShortDoubleSharpTick1_100Pct:
        label = "Short Double Sharp Tick1 (100%)";
        break;
      case Effect::kShortDoubleSharpTick2_80Pct:
        label = "Short Double Sharp Tick2 (80%)";
        break;
      case Effect::kShortDoubleSharpTick3_60Pct:
        label = "Short Double Sharp Tick3 (60%)";
        break;
      case Effect::kLongDoubleSharpClickStrong1_100Pct:
        label = "Long Double Sharp Click Strong1 (100%)";
        break;
      case Effect::kLongDoubleSharpClickStrong2_80Pct:
        label = "Long Double Sharp Click Strong2 (80%)";
        break;
      case Effect::kLongDoubleSharpClickStrong3_60Pct:
        label = "Long Double Sharp Click Strong3 (60%)";
        break;
      case Effect::kLongDoubleSharpClickStrong4_30Pct:
        label = "Long Double Sharp Click Strong4 (30%)";
        break;
      case Effect::kLongDoubleSharpClickMedium1_100Pct:
        label = "Long Double Sharp Click Medium1 (100%)";
        break;
      case Effect::kLongDoubleSharpClickMedium2_80Pct:
        label = "Long Double Sharp Click Medium2 (80%)";
        break;
      case Effect::kLongDoubleSharpClickMedium3_60Pct:
        label = "Long Double Sharp Click Medium3 (60%)";
        break;
      case Effect::kLongDoubleSharpTick1_100Pct:
        label = "Long Double Sharp Tick1 (100%)";
        break;
      case Effect::kLongDoubleSharpTick2_80Pct:
        label = "Long Double Sharp Tick2 (80%)";
        break;
      case Effect::kLongDoubleSharpTick3_60Pct:
        label = "Long Double Sharp Tick3 (60%)";
        break;
      case Effect::kBuzz1_100Pct:
        label = "Buzz1 (100%)";
        break;
      case Effect::kBuzz2_80Pct:
        label = "Buzz2 (80%)";
        break;
      case Effect::kBuzz3_60Pct:
        label = "Buzz3 (60%)";
        break;
      case Effect::kBuzz4_40Pct:
        label = "Buzz4 (40%)";
        break;
      case Effect::kBuzz5_20Pct:
        label = "Buzz5 (20%)";
        break;
      case Effect::kPulsingStrong1_100Pct:
        label = "Pulsing Strong1 (100%)";
        break;
      case Effect::kPulsingStrong2_60Pct:
        label = "Pulsing Strong2 (60%)";
        break;
      case Effect::kPulsingMedium1_100Pct:
        label = "Pulsing Medium1 (100%)";
        break;
      case Effect::kPulsingMedium2_60Pct:
        label = "Pulsing Medium2 (60%)";
        break;
      case Effect::kPulsingSharp1_100Pct:
        label = "Pulsing Sharp1 (100%)";
        break;
      case Effect::kPulsingSharp2_60Pct:
        label = "Pulsing Sharp2 (60%)";
        break;
      case Effect::kTransitionClick1_100Pct:
        label = "Transition Click1 (100%)";
        break;
      case Effect::kTransitionClick2_80Pct:
        label = "Transition Click2 (80%)";
        break;
      case Effect::kTransitionClick3_60Pct:
        label = "Transition Click3 (60%)";
        break;
      case Effect::kTransitionClick4_40Pct:
        label = "Transition Click4 (40%)";
        break;
      case Effect::kTransitionClick5_20Pct:
        label = "Transition Click5 (20%)";
        break;
      case Effect::kTransitionClick6_10Pct:
        label = "Transition Click6 (10%)";
        break;
      case Effect::kTransitionHum1_100Pct:
        label = "Transition Hum1 (100%)";
        break;
      case Effect::kTransitionHum2_80Pct:
        label = "Transition Hum2 (80%)";
        break;
      case Effect::kTransitionHum3_60Pct:
        label = "Transition Hum3 (60%)";
        break;
      case Effect::kTransitionHum4_40Pct:
        label = "Transition Hum4 (40%)";
        break;
      case Effect::kTransitionHum5_20Pct:
        label = "Transition Hum5 (20%)";
        break;
      case Effect::kTransitionHum6_10Pct:
        label = "Transition Hum6 (10%)";
        break;

      // TODO: fix labels for XtoY-style
      case Effect::kTransitionRampDownLongSmooth1_100to0Pct:
        label = "Transition Ramp Down Long Smooth1 (100→0%)";
        break;
      case Effect::kTransitionRampDownLongSmooth2_100to0Pct:
        label = "Transition Ramp Down Long Smooth2 (100→0%)";
        break;
      case Effect::kTransitionRampDownMediumSmooth1_100to0Pct:
        label = "Transition Ramp Down Medium Smooth1 (100→0%)";
        break;
      case Effect::kTransitionRampDownMediumSmooth2_100to0Pct:
        label = "Transition Ramp Down Medium Smooth2 (100→0%)";
        break;
      case Effect::kTransitionRampDownShortSmooth1_100to0Pct:
        label = "Transition Ramp Down Short Smooth1 (100→0%)";
        break;
      case Effect::kTransitionRampDownShortSmooth2_100to0Pct:
        label = "Transition Ramp Down Short Smooth2 (100→0%)";
        break;
      case Effect::kTransitionRampDownLongSharp1_100to0Pct:
        label = "Transition Ramp Down Long Sharp1 (100→0%)";
        break;
      case Effect::kTransitionRampDownLongSharp2_100to0Pct:
        label = "Transition Ramp Down Long Sharp2 (100→0%)";
        break;
      case Effect::kTransitionRampDownMediumSharp1_100to0Pct:
        label = "Transition Ramp Down Medium Sharp1 (100→0%)";
        break;
      case Effect::kTransitionRampDownMediumSharp2_100to0Pct:
        label = "Transition Ramp Down Medium Sharp2 (100→0%)";
        break;
      case Effect::kTransitionRampDownShortSharp1_100to0Pct:
        label = "Transition Ramp Down Short Sharp1 (100→0%)";
        break;
      case Effect::kTransitionRampDownShortSharp2_100to0Pct:
        label = "Transition Ramp Down Short Sharp2 (100→0%)";
        break;
      case Effect::kTransitionRampUpLongSmooth1_0to100Pct:
        label = "Transition Ramp Up Long Smooth1 (0→100%)";
        break;
      case Effect::kTransitionRampUpLongSmooth2_0to100Pct:
        label = "Transition Ramp Up Long Smooth2 (0→100%)";
        break;
      case Effect::kTransitionRampUpMediumSmooth1_0to100Pct:
        label = "Transition Ramp Up Medium Smooth1 (0→100%)";
        break;
      case Effect::kTransitionRampUpMediumSmooth2_0to100Pct:
        label = "Transition Ramp Up Medium Smooth2 (0→100%)";
        break;
      case Effect::kTransitionRampUpShortSmooth1_0to100Pct:
        label = "Transition Ramp Up Short Smooth1 (0→100%)";
        break;
      case Effect::kTransitionRampUpShortSmooth2_0to100Pct:
        label = "Transition Ramp Up Short Smooth2 (0→100%)";
        break;
      case Effect::kTransitionRampUpLongSharp1_0to100Pct:
        label = "Transition Ramp Up Long Sharp1 (0→100%)";
        break;
      case Effect::kTransitionRampUpLongSharp2_0to100Pct:
        label = "Transition Ramp Up Long Sharp2 (0→100%)";
        break;
      case Effect::kTransitionRampUpMediumSharp1_0to100Pct:
        label = "Transition Ramp Up Medium Sharp1 (0→100%)";
        break;
      case Effect::kTransitionRampUpMediumSharp2_0to100Pct:
        label = "Transition Ramp Up Medium Sharp2 (0→100%)";
        break;
      case Effect::kTransitionRampUpShortSharp1_0to100Pct:
        label = "Transition Ramp Up Short Sharp1 (0→100%)";
        break;
      case Effect::kTransitionRampUpShortSharp2_0to100Pct:
        label = "Transition Ramp Up Short Sharp2 (0→100%)";
        break;
      case Effect::kTransitionRampDownLongSmooth1_50to0Pct:
        label = "Transition Ramp Down Long Smooth1 (50→0%)";
        break;
      case Effect::kTransitionRampDownLongSmooth2_50to0Pct:
        label = "Transition Ramp Down Long Smooth2 (50→0%)";
        break;
      case Effect::kTransitionRampDownMediumSmooth1_50to0Pct:
        label = "Transition Ramp Down Medium Smooth1 (50→0%)";
        break;
      case Effect::kTransitionRampDownMediumSmooth2_50to0Pct:
        label = "Transition Ramp Down Medium Smooth2 (50→0%)";
        break;
      case Effect::kTransitionRampDownShortSmooth1_50to0Pct:
        label = "Transition Ramp Down Short Smooth1 (50→0%)";
        break;
      case Effect::kTransitionRampDownShortSmooth2_50to0Pct:
        label = "Transition Ramp Down Short Smooth2 (50→0%)";
        break;
      case Effect::kTransitionRampDownLongSharp1_50to0Pct:
        label = "Transition Ramp Down Long Sharp1 (50→0%)";
        break;
      case Effect::kTransitionRampDownLongSharp2_50to0Pct:
        label = "Transition Ramp Down Long Sharp2 (50→0%)";
        break;
      case Effect::kTransitionRampDownMediumSharp1_50to0Pct:
        label = "Transition Ramp Down Medium Sharp1 (50→0%)";
        break;
      case Effect::kTransitionRampDownMediumSharp2_50to0Pct:
        label = "Transition Ramp Down Medium Sharp2 (50→0%)";
        break;
      case Effect::kTransitionRampDownShortSharp1_50to0Pct:
        label = "Transition Ramp Down Short Sharp1 (50→0%)";
        break;
      case Effect::kTransitionRampDownShortSharp2_50to0Pct:
        label = "Transition Ramp Down Short Sharp2 (50→0%)";
        break;
      case Effect::kTransitionRampUpLongSmooth_10to50Pct:
        label = "Transition Ramp Up Long Smooth10to (10→50%)";
        break;
      case Effect::kTransitionRampUpLongSmooth_20to50Pct:
        label = "Transition Ramp Up Long Smooth20to (10→50%)";
        break;
      case Effect::kTransitionRampUpMediumSmooth_10to50Pct:
        label = "Transition Ramp Up Medium Smooth (10→50%)";
        break;
      case Effect::kTransitionRampUpMediumSmooth_20to50Pct:
        label = "Transition Ramp Up Medium Smooth (20→50%)";
        break;
      case Effect::kTransitionRampUpShortSmooth_10to50Pct:
        label = "Transition Ramp Up Short Smooth (10→50%)";
        break;
      case Effect::kTransitionRampUpShortSmooth_20to50Pct:
        label = "Transition Ramp Up Short Smooth (20→50%)";
        break;
      case Effect::kTransitionRampUpLongSharp_10to50Pct:
        label = "Transition Ramp Up Long Sharp (10→50%)";
        break;
      case Effect::kTransitionRampUpLongSharp_20to50Pct:
        label = "Transition Ramp Up Long Sharp (20→50%)";
        break;
      case Effect::kTransitionRampUpMediumSharp_10to50Pct:
        label = "Transition Ramp Up Medium Sharp (10→50%)";
        break;
      case Effect::kTransitionRampUpMediumSharp_20to50Pct:
        label = "Transition Ramp Up Medium Sharp (20→50%)";
        break;
      case Effect::kTransitionRampUpShortSharp_10to50Pct:
        label = "Transition Ramp Up Short Sharp (10→50%)";
        break;
      case Effect::kTransitionRampUpShortSharp_20to50Pct:
        label = "Transition Ramp Up Short Sharp (20→50%)";
        break;
      case Effect::kDontUseThis_Longbuzzforprogrammaticstopping_100Pct:
        // not you
        continue;
      case Effect::kSmoothHum1NoKickOrBrakePulse_50Pct:
        label = "Smooth Hum1 No Kick Or Brake Pulse (50%)";
        break;
      case Effect::kSmoothHum2NoKickOrBrakePulse_40Pct:
        label = "Smooth Hum2 No Kick Or Brake Pulse (40%)";
        break;
      case Effect::kSmoothHum3NoKickOrBrakePulse_30Pct:
        label = "Smooth Hum3 No Kick Or Brake Pulse (30%)";
        break;
      case Effect::kSmoothHum4NoKickOrBrakePulse_20Pct:
        label = "Smooth Hum4 No Kick Or Brake Pulse (20%)";
        break;
      case Effect::kSmoothHum5NoKickOrBrakePulse_10Pct:
        label = "Smooth Hum5 No Kick Or Brake Pulse (10%)";
        break;
      default:
        label = "UNKNOWN EFFECT";
    }

    ESP_LOGI(kTag, "%u: %s", effect, label);

    SetWaveformEffect(static_cast<Effect>(effect));
    Go();

    vTaskDelay(pdMS_TO_TICKS(900 /*ms*/));
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

}  // namespace drivers
