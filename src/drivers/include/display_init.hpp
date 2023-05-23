/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>

namespace drivers {
namespace displays {

extern const uint8_t kDelayBit;

struct InitialisationData {
  uint8_t num_sequences;
  const uint8_t* sequences[4];
};

extern const InitialisationData kST7735R;

/*
 * Valid command bytes that can be sent to ST77XX displays, as well as commands
 * for more specific variants.
 */
enum StCommands {
  ST77XX_NOP = 0x00,
  ST77XX_SWRESET = 0x01,
  ST77XX_RDDID = 0x04,
  ST77XX_RDDST = 0x09,

  ST77XX_SLPIN = 0x10,
  ST77XX_SLPOUT = 0x11,
  ST77XX_PTLON = 0x12,
  ST77XX_NORON = 0x13,

  ST77XX_INVOFF = 0x20,
  ST77XX_INVON = 0x21,
  ST77XX_DISPOFF = 0x28,
  ST77XX_DISPON = 0x29,
  ST77XX_CASET = 0x2A,
  ST77XX_RASET = 0x2B,
  ST77XX_RAMWR = 0x2C,
  ST77XX_RAMRD = 0x2E,

  ST77XX_PTLAR = 0x30,
  ST77XX_TEOFF = 0x34,
  ST77XX_TEON = 0x35,
  ST77XX_MADCTL = 0x36,
  ST77XX_COLMOD = 0x3A,

  ST77XX_MADCTL_MY = 0x80,
  ST77XX_MADCTL_MX = 0x40,
  ST77XX_MADCTL_MV = 0x20,
  ST77XX_MADCTL_ML = 0x10,
  ST77XX_MADCTL_RGB = 0x00,

  ST77XX_RDID1 = 0xDA,
  ST77XX_RDID2 = 0xDB,
  ST77XX_RDID3 = 0xDC,
  ST77XX_RDID4 = 0xDD,

  ST7735_MADCTL_BGR = 0x08,
  ST7735_MADCTL_MH = 0x04,

  ST7735_FRMCTR1 = 0xB1,
  ST7735_FRMCTR2 = 0xB2,
  ST7735_FRMCTR3 = 0xB3,
  ST7735_INVCTR = 0xB4,
  ST7735_DISSET5 = 0xB6,

  ST7735_PWCTR1 = 0xC0,
  ST7735_PWCTR2 = 0xC1,
  ST7735_PWCTR3 = 0xC2,
  ST7735_PWCTR4 = 0xC3,
  ST7735_PWCTR5 = 0xC4,
  ST7735_VMCTR1 = 0xC5,

  ST7735_PWCTR6 = 0xFC,

  ST7735_GMCTRP1 = 0xE0,
  ST7735_GMCTRN1 = 0xE1,
};

}  // namespace displays
}  // namespace drivers
