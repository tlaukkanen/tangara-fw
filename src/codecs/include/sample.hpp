#pragma once

#include <stdint.h>

#include <algorithm>

#include <mad.h>

namespace sample {

// A signed, 32-bit PCM sample.
typedef int16_t Sample;

constexpr auto Clip(int64_t v) -> Sample {
  if (v > INT16_MAX)
    return INT16_MAX;
  if (v < INT16_MIN)
    return INT16_MIN;
  return v;
}

constexpr auto FromSigned(int32_t src, uint_fast8_t bits) -> Sample {
  if (bits > 16) {
    // Left-align samples, effectively scaling them up to 32 bits.
    return src << (sizeof(Sample) * 8 - bits);
  } else if (bits < 16) {
    return src << (bits - (sizeof(Sample) * 8));
  }
  return src;
}

constexpr auto FromUnsigned(uint32_t src, uint_fast8_t bits) -> Sample {
  // Left-align, then substract the max value / 2 to make the sample centred
  // around zero.
  return (src << (sizeof(uint16_t) * 8 - bits)) - (~0UL >> 1);
}

constexpr auto FromFloat(float src) -> Sample {
  return std::clamp<float>(src, -1.0f, 1.0f) * static_cast<float>(INT16_MAX);
}

constexpr auto FromDouble(double src) -> Sample {
  return std::clamp<double>(src, -1.0, 1.0) * static_cast<double>(INT16_MAX);
}

constexpr auto FromMad(mad_fixed_t src) -> Sample {
  // Round the bottom bits.
  src += (1L << (MAD_F_FRACBITS - 16));

  // Clip the leftover bits to within range.
  if (src >= MAD_F_ONE)
    src = MAD_F_ONE - 1;
  else if (src < -MAD_F_ONE)
    src = -MAD_F_ONE;

  // Quantize.
  return FromSigned(src >> (MAD_F_FRACBITS + 1 - 16), 16);
}

static constexpr float kFactor = 1.0f / static_cast<float>(INT16_MAX);

constexpr auto ToFloat(Sample src) -> float {
  return src * kFactor;
}

}  // namespace sample