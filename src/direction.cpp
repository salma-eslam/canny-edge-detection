#include "direction.h"
#include <cmath>

Image gradientDirection(const Image16& gx, const Image16& gy) {  
// setup output image size and fill it with zeros
    Image dir;
    dir.width = gx.width;
    dir.height = gx.height;
    dir.data.resize(gx.width * gx.height, 0);
// processing every pixel one by one
    for (int i = 0; i < gx.width * gx.height; ++i) {
        int32_t x = gx.data[i];
        int32_t y = gy.data[i];
// getting the positive values to check the angle size
        int32_t abs_x = std::abs(x);
        int32_t abs_y = std::abs(y);
// if the pixel is completely flat we skip it
        if (abs_x == 0 && abs_y == 0) {
            dir.data[i] = 0;
            continue;
        }
// now we match the pixel to a sector using fast integer math instead of floats

        if (abs_y * 1000 < abs_x * 414) {  //G_y/G_x < tan 22.5
            dir.data[i] = 0; // 0°
        }
        else if (abs_y * 1000 > abs_x * 2414) {  //G_y/G_x > tan 67.5
            dir.data[i] = 2; // 90°
        }
         // Diagonal check (45 or 135 degrees)
        else {
            bool same_sign = ((x > 0) == (y > 0)) || (x == 0) || (y == 0);  // Safety net: forces 45 degrees if a zero-noise glitch slips in
            if (same_sign) {
                dir.data[i] = 1; // 45°
            } else {
                dir.data[i] = 3; // 135°
            }
        }
    }
    return dir;
}
