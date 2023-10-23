/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "haptics.hpp"
#include <stdint.h>

#include <cstdint>
#include <initializer_list>

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
  // TODO: is this needed?
  vTaskDelay(pdMS_TO_TICKS(300));

  PowerUp();

  // disable real-time feedback
  // WriteRegister(Register::kRealtimePlaybackInput, 0);

  // WriteRegister(Register::kSustainTimeOffsetPositive, 0);
  // WriteRegister(Register::kSustainTimeOffsetNegative, 0);
  // WriteRegister(Register::kBrakeTimeOffset, 0);

  // TODO uhhhh remove
  // WriteRegister(Register::kAudioToVibeOutputLevelMax, 0x64);

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
  WriteRegister(Register::kWaveformLibrary, static_cast<uint8_t>(Library::A));

  // Set up a default effect (sequence of one effect)
  SetWaveformEffect(Effect::kStrongBuzz_100Pct);

  // Set mode (internal trigger, on writing 1 to Go register)
  WriteRegister(Register::kMode, static_cast<uint8_t>(Mode::kInternalTrigger));
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

// Starts the pre-programmed sequence
auto Haptics::Go() -> void {
  WriteRegister(Register::kGo,
                static_cast<uint8_t>(RegisterDefaults::kGo) | 0b00000001);
}

auto Haptics::SetWaveformEffect(Effect effect) -> void {
  WriteRegister(Register::kWaveformSequenceSlot1, static_cast<uint8_t>(effect));
  WriteRegister(Register::kWaveformSequenceSlot2, static_cast<uint8_t>(Effect::kStop));
}


// TODO: remove
auto Haptics::Tour() -> void {
  ESP_LOGI(kTag, "%s", "=== Waveform Library Effects List ===");

  for (uint8_t effect = 1; effect <= 123; effect++) {
    SetWaveformEffect(static_cast<Effect>(effect));

    if (effect == 1) {
      ESP_LOGI(kTag, "%s", "1 − Strong Click - 100%");
    }
    if (effect == 2) {
      ESP_LOGI(kTag, "%s", "2 − Strong Click - 60%");
    }
    if (effect == 3) {
      ESP_LOGI(kTag, "%s", "3 − Strong Click - 30%");
    }
    if (effect == 4) {
      ESP_LOGI(kTag, "%s", "4 − Sharp Click - 100%");
    }
    if (effect == 5) {
      ESP_LOGI(kTag, "%s", "5 − Sharp Click - 60%");
    }
    if (effect == 6) {
      ESP_LOGI(kTag, "%s", "6 − Sharp Click - 30%");
    }
    if (effect == 7) {
      ESP_LOGI(kTag, "%s", "7 − Soft Bump - 100%");
    }
    if (effect == 8) {
      ESP_LOGI(kTag, "%s", "8 − Soft Bump - 60%");
    }
    if (effect == 9) {
      ESP_LOGI(kTag, "%s", "9 − Soft Bump - 30%");
    }
    if (effect == 10) {
      ESP_LOGI(kTag, "%s", "10 − Double Click - 100%");
    }
    if (effect == 11) {
      ESP_LOGI(kTag, "%s", "11 − Double Click - 60%");
    }
    if (effect == 12) {
      ESP_LOGI(kTag, "%s", "12 − Triple Click - 100%");
    }
    if (effect == 13) {
      ESP_LOGI(kTag, "%s", "13 − Soft Fuzz - 60%");
    }
    if (effect == 14) {
      ESP_LOGI(kTag, "%s", "14 − Strong Buzz - 100%");
    }
    if (effect == 15) {
      ESP_LOGI(kTag, "%s", "15 − 750 ms Alert 100%");
    }
    if (effect == 16) {
      ESP_LOGI(kTag, "%s", "16 − 1000 ms Alert 100%");
    }
    if (effect == 17) {
      ESP_LOGI(kTag, "%s", "17 − Strong Click 1 - 100%");
    }
    if (effect == 18) {
      ESP_LOGI(kTag, "%s", "18 − Strong Click 2 - 80%");
    }
    if (effect == 19) {
      ESP_LOGI(kTag, "%s", "19 − Strong Click 3 - 60%");
    }
    if (effect == 20) {
      ESP_LOGI(kTag, "%s", "20 − Strong Click 4 - 30%");
    }
    if (effect == 21) {
      ESP_LOGI(kTag, "%s", "21 − Medium Click 1 - 100%");
    }
    if (effect == 22) {
      ESP_LOGI(kTag, "%s", "22 − Medium Click 2 - 80%");
    }
    if (effect == 23) {
      ESP_LOGI(kTag, "%s", "23 − Medium Click 3 - 60%");
    }
    if (effect == 24) {
      ESP_LOGI(kTag, "%s", "24 − Sharp Tick 1 - 100%");
    }
    if (effect == 25) {
      ESP_LOGI(kTag, "%s", "25 − Sharp Tick 2 - 80%");
    }
    if (effect == 26) {
      ESP_LOGI(kTag, "%s", "26 − Sharp Tick 3 – 60%");
    }
    if (effect == 27) {
      ESP_LOGI(kTag, "%s", "27 − Short Double Click Strong 1 – 100%");
    }
    if (effect == 28) {
      ESP_LOGI(kTag, "%s", "28 − Short Double Click Strong 2 – 80%");
    }
    if (effect == 29) {
      ESP_LOGI(kTag, "%s", "29 − Short Double Click Strong 3 – 60%");
    }
    if (effect == 30) {
      ESP_LOGI(kTag, "%s", "30 − Short Double Click Strong 4 – 30%");
    }
    if (effect == 31) {
      ESP_LOGI(kTag, "%s", "31 − Short Double Click Medium 1 – 100%");
    }
    if (effect == 32) {
      ESP_LOGI(kTag, "%s", "32 − Short Double Click Medium 2 – 80%");
    }
    if (effect == 33) {
      ESP_LOGI(kTag, "%s", "33 − Short Double Click Medium 3 – 60%");
    }
    if (effect == 34) {
      ESP_LOGI(kTag, "%s", "34 − Short Double Sharp Tick 1 – 100%");
    }
    if (effect == 35) {
      ESP_LOGI(kTag, "%s", "35 − Short Double Sharp Tick 2 – 80%");
    }
    if (effect == 36) {
      ESP_LOGI(kTag, "%s", "36 − Short Double Sharp Tick 3 – 60%");
    }
    if (effect == 37) {
      ESP_LOGI(kTag, "%s", "37 − Long Double Sharp Click Strong 1 – 100%");
    }
    if (effect == 38) {
      ESP_LOGI(kTag, "%s", "38 − Long Double Sharp Click Strong 2 – 80%");
    }
    if (effect == 39) {
      ESP_LOGI(kTag, "%s", "39 − Long Double Sharp Click Strong 3 – 60%");
    }
    if (effect == 40) {
      ESP_LOGI(kTag, "%s", "40 − Long Double Sharp Click Strong 4 – 30%");
    }
    if (effect == 41) {
      ESP_LOGI(kTag, "%s", "41 − Long Double Sharp Click Medium 1 – 100%");
    }
    if (effect == 42) {
      ESP_LOGI(kTag, "%s", "42 − Long Double Sharp Click Medium 2 – 80%");
    }
    if (effect == 43) {
      ESP_LOGI(kTag, "%s", "43 − Long Double Sharp Click Medium 3 – 60%");
    }
    if (effect == 44) {
      ESP_LOGI(kTag, "%s", "44 − Long Double Sharp Tick 1 – 100%");
    }
    if (effect == 45) {
      ESP_LOGI(kTag, "%s", "45 − Long Double Sharp Tick 2 – 80%");
    }
    if (effect == 46) {
      ESP_LOGI(kTag, "%s", "46 − Long Double Sharp Tick 3 – 60%");
    }
    if (effect == 47) {
      ESP_LOGI(kTag, "%s", "47 − Buzz 1 – 100%");
    }
    if (effect == 48) {
      ESP_LOGI(kTag, "%s", "48 − Buzz 2 – 80%");
    }
    if (effect == 49) {
      ESP_LOGI(kTag, "%s", "49 − Buzz 3 – 60%");
    }
    if (effect == 50) {
      ESP_LOGI(kTag, "%s", "50 − Buzz 4 – 40%");
    }
    if (effect == 51) {
      ESP_LOGI(kTag, "%s", "51 − Buzz 5 – 20%");
    }
    if (effect == 52) {
      ESP_LOGI(kTag, "%s", "52 − Pulsing Strong 1 – 100%");
    }
    if (effect == 53) {
      ESP_LOGI(kTag, "%s", "53 − Pulsing Strong 2 – 60%");
    }
    if (effect == 54) {
      ESP_LOGI(kTag, "%s", "54 − Pulsing Medium 1 – 100%");
    }
    if (effect == 55) {
      ESP_LOGI(kTag, "%s", "55 − Pulsing Medium 2 – 60%");
    }
    if (effect == 56) {
      ESP_LOGI(kTag, "%s", "56 − Pulsing Sharp 1 – 100%");
    }
    if (effect == 57) {
      ESP_LOGI(kTag, "%s", "57 − Pulsing Sharp 2 – 60%");
    }
    if (effect == 58) {
      ESP_LOGI(kTag, "%s", "58 − Transition Click 1 – 100%");
    }
    if (effect == 59) {
      ESP_LOGI(kTag, "%s", "59 − Transition Click 2 – 80%");
    }
    if (effect == 60) {
      ESP_LOGI(kTag, "%s", "60 − Transition Click 3 – 60%");
    }
    if (effect == 61) {
      ESP_LOGI(kTag, "%s", "61 − Transition Click 4 – 40%");
    }
    if (effect == 62) {
      ESP_LOGI(kTag, "%s", "62 − Transition Click 5 – 20%");
    }
    if (effect == 63) {
      ESP_LOGI(kTag, "%s", "63 − Transition Click 6 – 10%");
    }
    if (effect == 64) {
      ESP_LOGI(kTag, "%s", "64 − Transition Hum 1 – 100%");
    }
    if (effect == 65) {
      ESP_LOGI(kTag, "%s", "65 − Transition Hum 2 – 80%");
    }
    if (effect == 66) {
      ESP_LOGI(kTag, "%s", "66 − Transition Hum 3 – 60%");
    }
    if (effect == 67) {
      ESP_LOGI(kTag, "%s", "67 − Transition Hum 4 – 40%");
    }
    if (effect == 68) {
      ESP_LOGI(kTag, "%s", "68 − Transition Hum 5 – 20%");
    }
    if (effect == 69) {
      ESP_LOGI(kTag, "%s", "69 − Transition Hum 6 – 10%");
    }
    if (effect == 70) {
      ESP_LOGI(kTag, "%s",
               "70 − Transition Ramp Down Long Smooth 1 – 100 to 0%");
    }
    if (effect == 71) {
      ESP_LOGI(kTag, "%s",
               "71 − Transition Ramp Down Long Smooth 2 – 100 to 0%");
    }
    if (effect == 72) {
      ESP_LOGI(kTag, "%s",
               "72 − Transition Ramp Down Medium Smooth 1 – 100 to 0%");
    }
    if (effect == 73) {
      ESP_LOGI(kTag, "%s",
               "73 − Transition Ramp Down Medium Smooth 2 – 100 to 0%");
    }
    if (effect == 74) {
      ESP_LOGI(kTag, "%s",
               "74 − Transition Ramp Down Short Smooth 1 – 100 to 0%");
    }
    if (effect == 75) {
      ESP_LOGI(kTag, "%s",
               "75 − Transition Ramp Down Short Smooth 2 – 100 to 0%");
    }
    if (effect == 76) {
      ESP_LOGI(kTag, "%s",
               "76 − Transition Ramp Down Long Sharp 1 – 100 to 0%");
    }
    if (effect == 77) {
      ESP_LOGI(kTag, "%s",
               "77 − Transition Ramp Down Long Sharp 2 – 100 to 0%");
    }
    if (effect == 78) {
      ESP_LOGI(kTag, "%s",
               "78 − Transition Ramp Down Medium Sharp 1 – 100 to 0%");
    }
    if (effect == 79) {
      ESP_LOGI(kTag, "%s",
               "79 − Transition Ramp Down Medium Sharp 2 – 100 to 0%");
    }
    if (effect == 80) {
      ESP_LOGI(kTag, "%s",
               "80 − Transition Ramp Down Short Sharp 1 – 100 to 0%");
    }
    if (effect == 81) {
      ESP_LOGI(kTag, "%s",
               "81 − Transition Ramp Down Short Sharp 2 – 100 to 0%");
    }
    if (effect == 82) {
      ESP_LOGI(kTag, "%s", "82 − Transition Ramp Up Long Smooth 1 – 0 to 100%");
    }
    if (effect == 83) {
      ESP_LOGI(kTag, "%s", "83 − Transition Ramp Up Long Smooth 2 – 0 to 100%");
    }
    if (effect == 84) {
      ESP_LOGI(kTag, "%s",
               "84 − Transition Ramp Up Medium Smooth 1 – 0 to 100%");
    }
    if (effect == 85) {
      ESP_LOGI(kTag, "%s",
               "85 − Transition Ramp Up Medium Smooth 2 – 0 to 100%");
    }
    if (effect == 86) {
      ESP_LOGI(kTag, "%s",
               "86 − Transition Ramp Up Short Smooth 1 – 0 to 100%");
    }
    if (effect == 87) {
      ESP_LOGI(kTag, "%s",
               "87 − Transition Ramp Up Short Smooth 2 – 0 to 100%");
    }
    if (effect == 88) {
      ESP_LOGI(kTag, "%s", "88 − Transition Ramp Up Long Sharp 1 – 0 to 100%");
    }
    if (effect == 89) {
      ESP_LOGI(kTag, "%s", "89 − Transition Ramp Up Long Sharp 2 – 0 to 100%");
    }
    if (effect == 90) {
      ESP_LOGI(kTag, "%s",
               "90 − Transition Ramp Up Medium Sharp 1 – 0 to 100%");
    }
    if (effect == 91) {
      ESP_LOGI(kTag, "%s",
               "91 − Transition Ramp Up Medium Sharp 2 – 0 to 100%");
    }
    if (effect == 92) {
      ESP_LOGI(kTag, "%s", "92 − Transition Ramp Up Short Sharp 1 – 0 to 100%");
    }
    if (effect == 93) {
      ESP_LOGI(kTag, "%s", "93 − Transition Ramp Up Short Sharp 2 – 0 to 100%");
    }
    if (effect == 94) {
      ESP_LOGI(kTag, "%s",
               "94 − Transition Ramp Down Long Smooth 1 – 50 to 0%");
    }
    if (effect == 95) {
      ESP_LOGI(kTag, "%s",
               "95 − Transition Ramp Down Long Smooth 2 – 50 to 0%");
    }
    if (effect == 96) {
      ESP_LOGI(kTag, "%s",
               "96 − Transition Ramp Down Medium Smooth 1 – 50 to 0%");
    }
    if (effect == 97) {
      ESP_LOGI(kTag, "%s",
               "97 − Transition Ramp Down Medium Smooth 2 – 50 to 0%");
    }
    if (effect == 98) {
      ESP_LOGI(kTag, "%s",
               "98 − Transition Ramp Down Short Smooth 1 – 50 to 0%");
    }
    if (effect == 99) {
      ESP_LOGI(kTag, "%s",
               "99 − Transition Ramp Down Short Smooth 2 – 50 to 0%");
    }
    if (effect == 100) {
      ESP_LOGI(kTag, "%s",
               "100 − Transition Ramp Down Long Sharp 1 – 50 to 0%");
    }
    if (effect == 101) {
      ESP_LOGI(kTag, "%s",
               "101 − Transition Ramp Down Long Sharp 2 – 50 to 0%");
    }
    if (effect == 102) {
      ESP_LOGI(kTag, "%s",
               "102 − Transition Ramp Down Medium Sharp 1 – 50 to 0%");
    }
    if (effect == 103) {
      ESP_LOGI(kTag, "%s",
               "103 − Transition Ramp Down Medium Sharp 2 – 50 to 0%");
    }
    if (effect == 104) {
      ESP_LOGI(kTag, "%s",
               "104 − Transition Ramp Down Short Sharp 1 – 50 to 0%");
    }
    if (effect == 105) {
      ESP_LOGI(kTag, "%s",
               "105 − Transition Ramp Down Short Sharp 2 – 50 to 0%");
    }
    if (effect == 106) {
      ESP_LOGI(kTag, "%s", "106 − Transition Ramp Up Long Smooth 1 – 0 to 50%");
    }
    if (effect == 107) {
      ESP_LOGI(kTag, "%s", "107 − Transition Ramp Up Long Smooth 2 – 0 to 50%");
    }
    if (effect == 108) {
      ESP_LOGI(kTag, "%s",
               "108 − Transition Ramp Up Medium Smooth 1 – 0 to 50%");
    }
    if (effect == 109) {
      ESP_LOGI(kTag, "%s",
               "109 − Transition Ramp Up Medium Smooth 2 – 0 to 50%");
    }
    if (effect == 110) {
      ESP_LOGI(kTag, "%s",
               "110 − Transition Ramp Up Short Smooth 1 – 0 to 50%");
    }
    if (effect == 111) {
      ESP_LOGI(kTag, "%s",
               "111 − Transition Ramp Up Short Smooth 2 – 0 to 50%");
    }
    if (effect == 112) {
      ESP_LOGI(kTag, "%s", "112 − Transition Ramp Up Long Sharp 1 – 0 to 50%");
    }
    if (effect == 113) {
      ESP_LOGI(kTag, "%s", "113 − Transition Ramp Up Long Sharp 2 – 0 to 50%");
    }
    if (effect == 114) {
      ESP_LOGI(kTag, "%s",
               "114 − Transition Ramp Up Medium Sharp 1 – 0 to 50%");
    }
    if (effect == 115) {
      ESP_LOGI(kTag, "%s",
               "115 − Transition Ramp Up Medium Sharp 2 – 0 to 50%");
    }
    if (effect == 116) {
      ESP_LOGI(kTag, "%s", "116 − Transition Ramp Up Short Sharp 1 – 0 to 50%");
    }
    if (effect == 117) {
      ESP_LOGI(kTag, "%s", "117 − Transition Ramp Up Short Sharp 2 – 0 to 50%");
    }
    if (effect == 118) {
      // not you
      continue;
      //ESP_LOGI(kTag, "%s", "118 − Long buzz for programmatic stopping – 100%");
    }
    if (effect == 119) {
      ESP_LOGI(kTag, "%s", "119 − Smooth Hum 1 (No kick or brake pulse) – 50%");
    }
    if (effect == 120) {
      ESP_LOGI(kTag, "%s", "120 − Smooth Hum 2 (No kick or brake pulse) – 40%");
    }
    if (effect == 121) {
      ESP_LOGI(kTag, "%s", "121 − Smooth Hum 3 (No kick or brake pulse) – 30%");
    }
    if (effect == 122) {
      ESP_LOGI(kTag, "%s", "122 − Smooth Hum 4 (No kick or brake pulse) – 20%");
    }
    if (effect == 123) {
      ESP_LOGI(kTag, "%s", "123 − Smooth Hum 5 (No kick or brake pulse) – 10%");
    }
    Go();
    vTaskDelay(pdMS_TO_TICKS(500 /*ms*/));
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
