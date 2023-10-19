#include "bitmap.hpp"
#include <string.h>

namespace util {
namespace bitmap {

void gui_get_bitmap_header(
    cpp::span<uint8_t> buf,
    uint16_t width,
    uint16_t height,
    uint32_t depth
) {
    auto buffer = buf.data();
    auto bufsize = buf.size();

    const char* bm = "BM";
    memcpy(buffer, bm, strlen(bm));
    buffer += strlen(bm);

    // Bitmap file header
    bmp_header_t* bmp = (bmp_header_t*)buffer;
    bmp->bfSize       = (uint32_t)(width * height * depth / 8);
    bmp->bfReserved   = 0;
    bmp->bfOffBits    = bufsize;

    // Bitmap information header
    bmp->biSize          = 40;
    bmp->biWidth         = width;
    bmp->biHeight        = -height;
    bmp->biPlanes        = 1;
    bmp->biBitCount      = depth;
    bmp->biCompression   = 3; // BI_BITFIELDS
    bmp->biSizeImage     = bmp->bfSize;
    bmp->biXPelsPerMeter = 2836;
    bmp->biYPelsPerMeter = 2836;
    bmp->biClrUsed       = 0; // zero defaults to 2^n
    bmp->biClrImportant  = 0;

    // BI_BITFIELDS
    bmp->bdMask[0] = 0xF800; // Red bitmask  : 1111 1000 | 0000 0000
    bmp->bdMask[1] = 0x07E0; // Green bitmask: 0000 0111 | 1110 0000
    bmp->bdMask[2] = 0x001F; // Blue bitmask : 0000 0000 | 0001 1111
}

}
}
