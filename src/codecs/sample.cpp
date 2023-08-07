#include "sample.hpp"

namespace audio {

namespace sample {

void siconv(int* dst, uint8_t* src, int bits, int skip, int count) {
  int i, v, s, b;

  b = (bits + 7) / 8;
  s = sizeof(int) * 8 - bits;
  while (count--) {
    v = 0;
    i = b;
    switch (b) {
      case 4:
        v = src[--i];
      case 3:
        v = (v << 8) | src[--i];
      case 2:
        v = (v << 8) | src[--i];
      case 1:
        v = (v << 8) | src[--i];
    }
    *dst++ = v << s;
    src += skip;
  }
}

void Siconv(int* dst, uint8_t* src, int bits, int skip, int count) {
  int i, v, s, b;

  b = (bits + 7) / 8;
  s = sizeof(int) * 8 - bits;
  while (count--) {
    v = 0;
    i = 0;
    switch (b) {
      case 4:
        v = src[i++];
      case 3:
        v = (v << 8) | src[i++];
      case 2:
        v = (v << 8) | src[i++];
      case 1:
        v = (v << 8) | src[i];
    }
    *dst++ = v << s;
    src += skip;
  }
}

void uiconv(int* dst, uint8_t* src, int bits, int skip, int count) {
  int i, s, b;
  uint32_t v;

  b = (bits + 7) / 8;
  s = sizeof(uint32_t) * 8 - bits;
  while (count--) {
    v = 0;
    i = b;
    switch (b) {
      case 4:
        v = src[--i];
      case 3:
        v = (v << 8) | src[--i];
      case 2:
        v = (v << 8) | src[--i];
      case 1:
        v = (v << 8) | src[--i];
    }
    *dst++ = (v << s) - (~0UL >> 1);
    src += skip;
  }
}

void Uiconv(int* dst, uint8_t* src, int bits, int skip, int count) {
  int i, s, b;
  uint32_t v;

  b = (bits + 7) / 8;
  s = sizeof(uint32_t) * 8 - bits;
  while (count--) {
    v = 0;
    i = 0;
    switch (b) {
      case 4:
        v = src[i++];
      case 3:
        v = (v << 8) | src[i++];
      case 2:
        v = (v << 8) | src[i++];
      case 1:
        v = (v << 8) | src[i];
    }
    *dst++ = (v << s) - (~0UL >> 1);
    src += skip;
  }
}

void ficonv(int* dst, uint8_t* src, int bits, int skip, int count) {
  if (bits == 32) {
    while (count--) {
      float f;

      f = *((float*)src), src += skip;
      if (f > 1.0)
        *dst++ = INT32_MAX;
      else if (f < -1.0)
        *dst++ = INT32_MIN;
      else
        *dst++ = f * ((float)INT32_MAX);
    }
  } else {
    while (count--) {
      double d;

      d = *((double*)src), src += skip;
      if (d > 1.0)
        *dst++ = INT32_MAX;
      else if (d < -1.0)
        *dst++ = INT32_MIN;
      else
        *dst++ = d * ((double)INT32_MAX);
    }
  }
}

void aiconv(int* dst, uint8_t* src, int, int skip, int count) {
  int t, seg;
  uint8_t a;

  while (count--) {
    a = *src, src += skip;
    a ^= 0x55;
    t = (a & 0xf) << 4;
    seg = (a & 0x70) >> 4;
    switch (seg) {
      case 0:
        t += 8;
        break;
      case 1:
        t += 0x108;
        break;
      default:
        t += 0x108;
        t <<= seg - 1;
    }
    t = (a & 0x80) ? t : -t;
    *dst++ = t << (sizeof(int) * 8 - 16);
  }
}

void µiconv(int* dst, uint8_t* src, int, int skip, int count) {
  int t;
  uint8_t u;

  while (count--) {
    u = *src, src += skip;
    u = ~u;
    t = ((u & 0xf) << 3) + 0x84;
    t <<= (u & 0x70) >> 4;
    t = u & 0x80 ? 0x84 - t : t - 0x84;
    *dst++ = t << (sizeof(int) * 8 - 16);
  }
}

void soconv(int* src, uint8_t* dst, int bits, int skip, int count) {
  int i, v, s, b;

  b = (bits + 7) / 8;
  s = sizeof(int) * 8 - bits;
  while (count--) {
    v = *src++ >> s;
    i = 0;
    switch (b) {
      case 4:
        dst[i++] = v, v >>= 8;
      case 3:
        dst[i++] = v, v >>= 8;
      case 2:
        dst[i++] = v, v >>= 8;
      case 1:
        dst[i] = v;
    }
    dst += skip;
  }
}

void Soconv(int* src, uint8_t* dst, int bits, int skip, int count) {
  int i, v, s, b;

  b = (bits + 7) / 8;
  s = sizeof(int) * 8 - bits;
  while (count--) {
    v = *src++ >> s;
    i = b;
    switch (b) {
      case 4:
        dst[--i] = v, v >>= 8;
      case 3:
        dst[--i] = v, v >>= 8;
      case 2:
        dst[--i] = v, v >>= 8;
      case 1:
        dst[--i] = v;
    }
    dst += skip;
  }
}

void uoconv(int* src, uint8_t* dst, int bits, int skip, int count) {
  int i, s, b;
  uint32_t v;

  b = (bits + 7) / 8;
  s = sizeof(uint32_t) * 8 - bits;
  while (count--) {
    v = ((~0UL >> 1) + *src++) >> s;
    i = 0;
    switch (b) {
      case 4:
        dst[i++] = v, v >>= 8;
      case 3:
        dst[i++] = v, v >>= 8;
      case 2:
        dst[i++] = v, v >>= 8;
      case 1:
        dst[i] = v;
    }
    dst += skip;
  }
}

void Uoconv(int* src, uint8_t* dst, int bits, int skip, int count) {
  int i, s, b;
  uint32_t v;

  b = (bits + 7) / 8;
  s = sizeof(uint32_t) * 8 - bits;
  while (count--) {
    v = ((~0UL >> 1) + *src++) >> s;
    i = b;
    switch (b) {
      case 4:
        dst[--i] = v, v >>= 8;
      case 3:
        dst[--i] = v, v >>= 8;
      case 2:
        dst[--i] = v, v >>= 8;
      case 1:
        dst[--i] = v;
    }
    dst += skip;
  }
}

void foconv(int* src, uint8_t* dst, int bits, int skip, int count) {
  if (bits == 32) {
    while (count--) {
      *((float*)dst) = *src++ / ((float)INT32_MAX);
      dst += skip;
    }
  } else {
    while (count--) {
      *((double*)dst) = *src++ / ((double)INT32_MAX);
      dst += skip;
    }
  }
}

}  // namespace sample

}  // namespace audio
