
#include <stddef.h>
#include <stdint.h>
#include <span.hpp>

namespace util {
namespace bitmap {

struct bmp_header_t
{
    uint32_t bfSize;
    uint32_t bfReserved;
    uint32_t bfOffBits;

    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;

    uint32_t bdMask[3]; // RGB
};

void gui_get_bitmap_header(
    cpp::span<uint8_t> buffer,
    uint16_t width,
    uint16_t height,
    uint32_t depth
);

} // namespace bitmap

} // namespace util
