/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>

#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "driver/i2s_std.h"
#include "driver/i2s_types.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/stream_buffer.h"
#include "result.hpp"
#include "span.hpp"

#include "gpio_expander.hpp"
#include "sys/_stdint.h"

namespace drivers {

namespace pcm512x {
class Register {
 public:
  uint8_t page;
  uint8_t reg;

  constexpr Register(uint8_t p, uint8_t r) : page(p), reg(r) {}
};

constexpr Register RESET(0, 1);
constexpr Register POWER(0, 2);
constexpr Register MUTE(0, 3);
constexpr Register PLL_EN(0, 4);
constexpr Register SPI_MISO_FUNCTION(0, 6);
constexpr Register DSP(0, 7);
constexpr Register GPIO_EN(0, 8);
constexpr Register BCLK_LRCLK_CFG(0, 9);
constexpr Register DSP_GPIO_INPUT(0, 10);
constexpr Register MASTER_MODE(0, 12);
constexpr Register PLL_REF(0, 13);
constexpr Register DAC_REF(0, 14);
constexpr Register GPIO_DACIN(0, 16);
constexpr Register GPIO_PLLIN(0, 18);
constexpr Register SYNCHRONIZE(0, 19);
constexpr Register PLL_COEFF_0(0, 20);
constexpr Register PLL_COEFF_1(0, 21);
constexpr Register PLL_COEFF_2(0, 22);
constexpr Register PLL_COEFF_3(0, 23);
constexpr Register PLL_COEFF_4(0, 24);
constexpr Register DSP_CLKDIV(0, 27);
constexpr Register DAC_CLKDIV(0, 28);
constexpr Register NCP_CLKDIV(0, 29);
constexpr Register OSR_CLKDIV(0, 30);
constexpr Register MASTER_CLKDIV_1(0, 32);
constexpr Register MASTER_CLKDIV_2(0, 33);
constexpr Register FS_SPEED_MODE(0, 34);
constexpr Register IDAC_1(0, 35);
constexpr Register IDAC_2(0, 36);
constexpr Register ERROR_DETECT(0, 37);
constexpr Register I2S_1(0, 40);
constexpr Register I2S_2(0, 41);
constexpr Register DAC_ROUTING(0, 42);
constexpr Register DSP_PROGRAM(0, 43);
constexpr Register CLKDET(0, 44);
constexpr Register AUTO_MUTE(0, 59);
constexpr Register DIGITAL_VOLUME_1(0, 60);
constexpr Register DIGITAL_VOLUME_2(0, 61);
constexpr Register DIGITAL_VOLUME_3(0, 62);
constexpr Register DIGITAL_MUTE_1(0, 63);
constexpr Register DIGITAL_MUTE_2(0, 64);
constexpr Register DIGITAL_MUTE_3(0, 65);
constexpr Register GPIO_OUTPUT_1(0, 80);
constexpr Register GPIO_OUTPUT_2(0, 81);
constexpr Register GPIO_OUTPUT_3(0, 82);
constexpr Register GPIO_OUTPUT_4(0, 83);
constexpr Register GPIO_OUTPUT_5(0, 84);
constexpr Register GPIO_OUTPUT_6(0, 85);
constexpr Register GPIO_CONTROL_1(0, 86);
constexpr Register GPIO_CONTROL_2(0, 87);
constexpr Register OVERFLOW(0, 90);
constexpr Register RATE_DET_1(0, 91);
constexpr Register RATE_DET_2(0, 92);
constexpr Register RATE_DET_3(0, 93);
constexpr Register RATE_DET_4(0, 94);
constexpr Register CLOCK_STATUS(0, 95);
constexpr Register ANALOG_MUTE_DET(0, 108);
constexpr Register POWER_STATE(0, 118);
constexpr Register GPIN(0, 119);
constexpr Register DIGITAL_MUTE_DET(0, 120);

constexpr Register OUTPUT_AMPLITUDE(1, 1);
constexpr Register ANALOG_GAIN_CTRL(1, 2);
constexpr Register UNDERVOLTAGE_PROT(1, 5);
constexpr Register ANALOG_MUTE_CTRL(1, 6);
constexpr Register ANALOG_GAIN_BOOST(1, 7);
constexpr Register VCOM_CTRL_1(1, 8);
constexpr Register VCOM_CTRL_2(1, 9);

constexpr Register CRAM_CTRL(44, 1);

constexpr Register FLEX_A(253, 63);
constexpr Register FLEX_B(253, 64);

}  // namespace pcm512x

/**
 * Interface for a PCM5122PWR DAC, configured over I2C.
 */
class AudioDac {
 public:
  enum Error {
    FAILED_TO_BOOT,
    FAILED_TO_CONFIGURE,
    FAILED_TO_INSTALL_I2S,
  };

  static auto create(GpioExpander* expander) -> cpp::result<AudioDac*, Error>;

  AudioDac(GpioExpander* gpio, i2s_chan_handle_t i2s_handle);
  ~AudioDac();

  /**
   * Sets the volume on a scale from 0 (loudest) to 254 (quietest). A value of
   * 255 engages the soft mute function.
   */
  void WriteVolume(uint8_t volume);

  enum PowerState {
    POWERDOWN = 0b0,
    WAIT_FOR_CP = 0b1,
    CALIBRATION_1 = 0b10,
    CALIBRATION_2 = 0b11,
    RAMP_UP = 0b100,
    RUN = 0b101,
    SHORT = 0b110,
    RAMP_DOWN = 0b111,
    STANDBY = 0b1000,
  };

  /* Returns the current boot-up status and internal state of the DAC */
  std::pair<bool, PowerState> ReadPowerState();

  enum BitsPerSample {
    BPS_16 = I2S_DATA_BIT_WIDTH_16BIT,
    BPS_24 = I2S_DATA_BIT_WIDTH_24BIT,
    BPS_32 = I2S_DATA_BIT_WIDTH_32BIT,
  };
  enum SampleRate {
    SAMPLE_RATE_11_025 = 11025,
    SAMPLE_RATE_16 = 16000,
    SAMPLE_RATE_22_05 = 22050,
    SAMPLE_RATE_32 = 32000,
    SAMPLE_RATE_44_1 = 44100,
    SAMPLE_RATE_48 = 48000,
    SAMPLE_RATE_96 = 96000,
    SAMPLE_RATE_192 = 192000,
  };

  // TODO(jacqueline): worth supporting channels here as well?
  auto Reconfigure(BitsPerSample bps, SampleRate rate) -> void;

  auto WriteData(const cpp::span<const std::byte>& data) -> void;
  auto SetSource(StreamBufferHandle_t buffer) -> void;

  auto Stop() -> void;
  auto LogStatus() -> void;

  // Not copyable or movable.
  AudioDac(const AudioDac&) = delete;
  AudioDac& operator=(const AudioDac&) = delete;

 private:
  GpioExpander* gpio_;
  i2s_chan_handle_t i2s_handle_;
  bool i2s_active_;
  std::optional<uint8_t> active_page_;

  i2s_std_clk_config_t clock_config_;
  i2s_std_slot_config_t slot_config_;

  /*
   * Pools the power state for up to 10ms, waiting for the given predicate to
   * be true.
   */
  bool WaitForPowerState(std::function<bool(bool, PowerState)> predicate);

  void WriteRegister(pcm512x::Register r, uint8_t val);
  uint8_t ReadRegister(pcm512x::Register r);

  void SelectPage(uint8_t page);
  void WriteRegisterRaw(uint8_t reg, uint8_t val);
  uint8_t ReadRegisterRaw(uint8_t reg);
};

}  // namespace drivers
