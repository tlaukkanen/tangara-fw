/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>

#include <algorithm>

#include <mad.h>

namespace sample {

// A signed, 16-bit PCM sample. All decoder output should be normalised to this
// format, in order to simplify resampling and/or re-encoding for bluetooth.
// Why 'only' 16 bits?
//  1. It's the lowest common bits per sample amongst our codecs. A higher bits
//     per sample would require us to uselessly scale up those outputs.
//  2. With appropriate dithering, you're not going to hear a difference
//     between 16 bit samples and higher bits anyway.
//  3. Monty from Xiph.org reckons it's all you need.
typedef int16_t Sample;

auto shiftWithDither(int64_t src, uint_fast8_t bits) -> Sample;

constexpr auto FromSigned(int32_t src, uint_fast8_t bits) -> Sample {
  if (bits > 16) {
    return shiftWithDither(src, bits - 16);
  } else if (bits < 16) {
    return src << (sizeof(Sample) * 8 - bits);
  }
  return src;
}

constexpr auto FromUnsigned(uint32_t src, uint_fast8_t bits) -> Sample {
  // Left-align, then substract the max value / 2 to make the sample centred
  // around zero.
  return (src << (sizeof(uint16_t) * 8 - bits)) - (INT16_MAX+1);
}

constexpr auto FromFloat(float src) -> Sample {
  int32_t quantised =
      std::clamp<float>(src, -1.0f, 1.0f) * static_cast<float>(INT32_MAX);
  return FromSigned(quantised, 32);
}

constexpr auto FromDouble(double src) -> Sample {
  int32_t quantised =
      std::clamp<double>(src, -1.0, 1.0) * static_cast<double>(INT32_MAX);
  return FromSigned(quantised, 32);
}

constexpr auto FromMad(mad_fixed_t src) -> Sample {
  // Round the bottom bits.
  src += (1L << (MAD_F_FRACBITS - 24));

  // Clip the leftover bits to within range.
  if (src >= MAD_F_ONE)
    src = MAD_F_ONE - 1;
  else if (src < -MAD_F_ONE)
    src = -MAD_F_ONE;

  // Quantize.
  return FromSigned(src >> (MAD_F_FRACBITS + 1 - 24), 24);
}

static constexpr float kFactor = 1.0f / static_cast<float>(INT16_MAX);

constexpr auto ToFloat(Sample src) -> float {
  return src * kFactor;
}

}  // namespace sample
