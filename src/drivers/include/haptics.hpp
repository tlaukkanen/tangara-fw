/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <initializer_list>

namespace drivers {

class Haptics {
 public:
  static auto Create() -> Haptics* { return new Haptics(); }
  Haptics();
  ~Haptics();

  // Not copyable or movable.
  Haptics(const Haptics&) = delete;
  Haptics& operator=(const Haptics&) = delete;

  // See the datasheet for section references in the below comments:
  // https://www.ti.com/lit/ds/symlink/drv2605l.pdf

  // §12.1.2 Waveform Library Effects List
  enum class Effect {
    kStop = 0,  // Sentinel/terminator Effect for the Waveform Sequence Slots
    kStrongClick_100Pct = 1,
    kStrongClick_60Pct = 2,
    kStrongClick_30Pct = 3,
    kSharpClick_100Pct = 4,
    kSharpClick_60Pct = 5,
    kSharpClick_30Pct = 6,
    kSoftBump_100Pct = 7,
    kSoftBump_60Pct = 8,
    kSoftBump_30Pct = 9,
    kDoubleClick_100Pct = 10,
    kDoubleClick_60Pct = 11,
    kTripleClick_100Pct = 12,
    kSoftFuzz_60Pct = 13,
    kStrongBuzz_100Pct = 14,
    k750msAlert_100Pct = 15,
    k1000msAlert_100Pct = 16,
    kStrongClick1_100Pct = 17,
    kStrongClick2_80Pct = 18,
    kStrongClick3_60Pct = 19,
    kStrongClick4_30Pct = 20,
    kMediumClick1_100Pct = 21,
    kMediumClick2_80Pct = 22,
    kMediumClick3_60Pct = 23,
    kSharpTick1_100Pct = 24,
    kSharpTick2_80Pct = 25,
    kSharpTick3_60Pct = 26,
    kShortDoubleClickStrong1_100Pct = 27,
    kShortDoubleClickStrong2_80Pct = 28,
    kShortDoubleClickStrong3_60Pct = 29,
    kShortDoubleClickStrong4_30Pct = 30,
    kShortDoubleClickMedium1_100Pct = 31,
    kShortDoubleClickMedium2_80Pct = 32,
    kShortDoubleClickMedium3_60Pct = 33,
    kShortDoubleSharpTick1_100Pct = 34,
    kShortDoubleSharpTick2_80Pct = 35,
    kShortDoubleSharpTick3_60Pct = 36,
    kLongDoubleSharpClickStrong1_100Pct = 37,
    kLongDoubleSharpClickStrong2_80Pct = 38,
    kLongDoubleSharpClickStrong3_60Pct = 39,
    kLongDoubleSharpClickStrong4_30Pct = 40,
    kLongDoubleSharpClickMedium1_100Pct = 41,
    kLongDoubleSharpClickMedium2_80Pct = 42,
    kLongDoubleSharpClickMedium3_60Pct = 43,
    kLongDoubleSharpTick1_100Pct = 44,
    kLongDoubleSharpTick2_80Pct = 45,
    kLongDoubleSharpTick3_60Pct = 46,
    kBuzz1_100Pct = 47,
    kBuzz2_80Pct = 48,
    kBuzz3_60Pct = 49,
    kBuzz4_40Pct = 50,
    kBuzz5_20Pct = 51,
    kPulsingStrong1_100Pct = 52,
    kPulsingStrong2_60Pct = 53,
    kPulsingMedium1_100Pct = 54,
    kPulsingMedium2_60Pct = 55,
    kPulsingSharp1_100Pct = 56,
    kPulsingSharp2_60Pct = 57,
    kTransitionClick1_100Pct = 58,
    kTransitionClick2_80Pct = 59,
    kTransitionClick3_60Pct = 60,
    kTransitionClick4_40Pct = 61,
    kTransitionClick5_20Pct = 62,
    kTransitionClick6_10Pct = 63,
    kTransitionHum1_100Pct = 64,
    kTransitionHum2_80Pct = 65,
    kTransitionHum3_60Pct = 66,
    kTransitionHum4_40Pct = 67,
    kTransitionHum5_20Pct = 68,
    kTransitionHum6_10Pct = 69,
    kTransitionRampDownLongSmooth1_100to0Pct = 70,
    kTransitionRampDownLongSmooth2_100to0Pct = 71,
    kTransitionRampDownMediumSmooth1_100to0Pct = 72,
    kTransitionRampDownMediumSmooth2_100to0Pct = 73,
    kTransitionRampDownShortSmooth1_100to0Pct = 74,
    kTransitionRampDownShortSmooth2_100to0Pct = 75,
    kTransitionRampDownLongSharp1_100to0Pct = 76,
    kTransitionRampDownLongSharp2_100to0Pct = 77,
    kTransitionRampDownMediumSharp1_100to0Pct = 78,
    kTransitionRampDownMediumSharp2_100to0Pct = 79,
    kTransitionRampDownShortSharp1_100to0Pct = 80,
    kTransitionRampDownShortSharp2_100to0Pct = 81,
    kTransitionRampUpLongSmooth1_0to100Pct = 82,
    kTransitionRampUpLongSmooth2_0to100Pct = 83,
    kTransitionRampUpMediumSmooth1_0to100Pct = 84,
    kTransitionRampUpMediumSmooth2_0to100Pct = 85,
    kTransitionRampUpShortSmooth1_0to100Pct = 86,
    kTransitionRampUpShortSmooth2_0to100Pct = 87,
    kTransitionRampUpLongSharp1_0to100Pct = 88,
    kTransitionRampUpLongSharp2_0to100Pct = 89,
    kTransitionRampUpMediumSharp1_0to100Pct = 90,
    kTransitionRampUpMediumSharp2_0to100Pct = 91,
    kTransitionRampUpShortSharp1_0to100Pct = 92,
    kTransitionRampUpShortSharp2_0to100Pct = 93,
    kTransitionRampDownLongSmooth1_50to0Pct = 94,
    kTransitionRampDownLongSmooth2_50to0Pct = 95,
    kTransitionRampDownMediumSmooth1_50to0Pct = 96,
    kTransitionRampDownMediumSmooth2_50to0Pct = 97,
    kTransitionRampDownShortSmooth1_50to0Pct = 98,
    kTransitionRampDownShortSmooth2_50to0Pct = 99,
    kTransitionRampDownLongSharp1_50to0Pct = 100,
    kTransitionRampDownLongSharp2_50to0Pct = 101,
    kTransitionRampDownMediumSharp1_50to0Pct = 102,
    kTransitionRampDownMediumSharp2_50to0Pct = 103,
    kTransitionRampDownShortSharp1_50to0Pct = 104,
    kTransitionRampDownShortSharp2_50to0Pct = 105,
    kTransitionRampUpLongSmooth_10to50Pct = 106,
    kTransitionRampUpLongSmooth_20to50Pct = 107,
    kTransitionRampUpMediumSmooth_10to50Pct = 108,
    kTransitionRampUpMediumSmooth_20to50Pct = 109,
    kTransitionRampUpShortSmooth_10to50Pct = 110,
    kTransitionRampUpShortSmooth_20to50Pct = 111,
    kTransitionRampUpLongSharp_10to50Pct = 112,
    kTransitionRampUpLongSharp_20to50Pct = 113,
    kTransitionRampUpMediumSharp_10to50Pct = 114,
    kTransitionRampUpMediumSharp_20to50Pct = 115,
    kTransitionRampUpShortSharp_10to50Pct = 116,
    kTransitionRampUpShortSharp_20to50Pct = 117,
    kSmoothHum1NoKickOrBrakePulse_50Pct = 119,
    kSmoothHum2NoKickOrBrakePulse_40Pct = 120,
    kSmoothHum3NoKickOrBrakePulse_30Pct = 121,
    kSmoothHum4NoKickOrBrakePulse_20Pct = 122,
    kSmoothHum5NoKickOrBrakePulse_10Pct = 123,

    // We can't use this one; need to have the EN pin hooked up.
    kDontUseThis_Longbuzzforprogrammaticstopping_100Pct = 118,
  };

  auto PowerDown() -> void;
  auto Reset() -> void;

  auto SetWaveformEffect(Effect effect) -> void;
  auto Go() -> void;

  auto Tour() -> void;  // TODO(robin): remove or parameterise

 private:
  // §8.4.2 Changing Modes of Operation
  enum class Mode : uint8_t {
    kInternalTrigger = 0,
    kExternalTriggerEdge = 1,
    kExternalTriggerLevel = 2,
    kPwmAnalog = 3,
    kAudioToVibe = 4,
    kRealtimePlayback = 5,
    kDiagnostics = 6,
    kAutoCalibrate = 7,
  };

  struct ModeMask {
    // §8.4.1.4 Operation With STANDBY Control
    static constexpr uint8_t kStandby = 0b01000000;
    // §8.4.1.5 Operation With DEV_RESET Control
    static constexpr uint8_t kDevReset = 0b10000000;
  };

  struct ControlMask {
    // Control1
    static constexpr uint8_t kNErmLra = 0b10000000;
    // Control3
    static constexpr uint8_t kErmOpenLoop = 0b00100000;
  };

  // §8.6 Register Map
  enum class Register {
    kStatus = 0x00,
    kMode = 0x01,

    kRealtimePlaybackInput = 0x02,
    kWaveformLibrary = 0x03,  // see Library enum

    kWaveformSequenceSlot1 = 0x04,
    kWaveformSequenceSlot2 = 0x05,
    kWaveformSequenceSlot3 = 0x06,
    kWaveformSequenceSlot4 = 0x07,
    kWaveformSequenceSlot5 = 0x08,
    kWaveformSequenceSlot6 = 0x09,
    kWaveformSequenceSlot7 = 0x0a,
    kWaveformSequenceSlot8 = 0x0b,

    kGo = 0x0C,

    // §8.3.5.2.2 Library Parameterization
    kOverdriveTimeOffset = 0x0D,
    kSustainTimeOffsetPositive = 0x0E,
    kSustainTimeOffsetNegative = 0x0F,
    kBrakeTimeOffset = 0x10,
    kAudioToVibeControl = 0x11,

    kAudioToVibeInputLevelMin = 0x12,
    kAudioToVibeInputLevelMax = 0x13,
    kAudioToVibeOutputLevelMin = 0x14,
    kAudioToVibeOutputLevelMax = 0x15,
    kRatedVoltage = 0x16,
    kOverdriveClampVoltage = 0x17,
    kAutoCalibrationCompensationResult = 0x18,
    kAutoCalibrationBackEmfResult = 0x19,

    // A bunch of different options, not grouped
    // in any particular sensible way
    kControl1 = 0x1A,
    kControl2 = 0x1B,
    kControl3 = 0x1C,
    kControl4 = 0x1D,
    kControl5 = 0x1E,
    kControl6 = 0x1F,

    kSupplyVoltageMonitor = 0x21,  // "VBAT"
    kLraResonancePeriod = 0x22,
  };

  enum class RegisterDefaults : uint8_t {
    kStatus = 0xE0,
    kMode = 0x40,
    kRealtimePlaybackInput = 0,
    kWaveformLibrary = 0x01,
    kWaveformSequenceSlot1 = 0x01,
    kWaveformSequenceSlot2 = 0,
    kWaveformSequenceSlot3 = 0,
    kWaveformSequenceSlot4 = 0,
    kWaveformSequenceSlot5 = 0,
    kWaveformSequenceSlot6 = 0,
    kWaveformSequenceSlot7 = 0,
    kWaveformSequenceSlot8 = 0,
    kGo = 0,
    kOverdriveTimeOffset = 0,
    kSustainTimeOffsetPositive = 0,
    kSustainTimeOffsetNegative = 0,
    kBrakeTimeOffset = 0,
    kAudioToVibeControl = 0x05,
    kAudioToVibeInputLevelMin = 0x19,
    kAudioToVibeInputLevelMax = 0xFF,
    kAudioToVibeOutputLevelMin = 0x19,
    kAudioToVibeOutputLevelMax = 0xFF,
    kRatedVoltage = 0x3E,
    kOverdriveClampVoltage = 0x8C,
    kAutoCalibrationCompensationResult = 0x0C,
    kAutoCalibrationBackEmfResult = 0x6C,
    kControl1 = 0x36,
    kControl2 = 0x93,
    kControl3 = 0xF5,
    kControl4 = 0xA0,
    kControl5 = 0x20,
    kControl6 = 0x80,
    kSupplyVoltageMonitor = 0,
    kLraResonancePeriod = 0,
  };

  // §8.3.5.2 Internal Memory Interface
  // Pick the ERM Library matching the motor.
  enum class Library : uint8_t {
    A = 1,  // 1.3V-3V, Rise: 40-60ms,   Brake: 20-40ms
    B = 2,  // 3V, Rise: 40-60ms,   Brake: 5-15ms
    C = 3,  // 3V, Rise: 60-80ms,   Brake: 10-20ms
    D = 4,  // 3V, Rise: 100-140ms, Brake: 15-25ms
    E = 5   // 3V, Rise: >140ms,    Brake: >30ms
    // 6 is LRA-only, 7 is 4.5V+
  };

  auto PowerUp() -> void;
  auto WriteRegister(Register reg, uint8_t val) -> void;
};

}  // namespace drivers
