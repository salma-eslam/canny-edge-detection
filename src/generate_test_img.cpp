#include "image_io.h"

int main() {
    const int W = 512;
    const int H = 512;

    // Rectangle image
    Image rect;
    rect.width = W;
    rect.height = H;
    rect.data.resize(W * H, 0);

    for (int y = H / 4; y < 3 * H / 4; y++) {
        for (int x = W / 4; x < 3 * W / 4; x++) {
            rect.data[y * W + x] = 255;
        }
    }

    saveRawImage("rect.raw", rect);

    // Circle image
    Image circle;
    circle.width = W;
    circle.height = H;
    circle.data.resize(W * H, 0);

    int cx = W / 2;
    int cy = H / 2;
    int radius = W / 4;

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int dx = x - cx;
            int dy = y - cy;

            if (dx * dx + dy * dy <= radius * radius) {
                circle.data[y * W + x] = 255;
            }
        }
    }

    saveRawImage("circle.raw", circle);

    // Diagonal edge image
    Image diag;
    diag.width = W;
    diag.height = H;
    diag.data.resize(W * H, 0);

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (x > y) {
                diag.data[y * W + x] = 255;
            }
        }
    }

    saveRawImage("diag.raw", diag);

    return 0;
}

