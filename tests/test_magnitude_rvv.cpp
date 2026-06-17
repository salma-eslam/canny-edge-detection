#include <cassert>
#include <cstdio>
#include "gradient.h"

// ─────────────────────────────────────────────────────────────
// Equivalence test: RVV magnitude vs scalar magnitude
//
// This test verifies that magnitudeL1Rvv and magnitudeL2Rvv
// produce EXACTLY the same output as their scalar counterparts
// magnitudeL1 and magnitudeL2 on the same input.
//
// We test at VLEN=128, 256, and 512 by running this same binary
// under different QEMU configurations.
// ─────────────────────────────────────────────────────────────

int main() {

    // ── Test 1: uniform zero input ─────────────────────────────
    // If Gx and Gy are all zero, both scalar and RVV must
    // return all zeros regardless of VLEN.
    {
        Image16 gx, gy;
        gx.width  = gy.width  = 8;
        gx.height = gy.height = 8;
        gx.data.resize(64, 0);
        gy.data.resize(64, 0);

        Image scalarL1 = magnitudeL1(gx, gy);
        Image rvvL1    = magnitudeL1Rvv(gx, gy);
        Image scalarL2 = magnitudeL2(gx, gy);
        Image rvvL2    = magnitudeL2Rvv(gx, gy);

        for (int i = 0; i < 64; i++) {
            assert(scalarL1.data[i] == rvvL1.data[i]);
            assert(scalarL2.data[i] == rvvL2.data[i]);
        }
        printf("Test 1 passed: uniform zero input matches at all VLEN.\n");
    }

    // ── Test 2: mixed positive and negative values ──────────────
    // Sobel output can be negative. We test that both scalar
    // and RVV handle negative values identically.
    {
        Image16 gx, gy;
        gx.width  = gy.width  = 8;
        gx.height = gy.height = 8;
        gx.data.resize(64, 0);
        gy.data.resize(64, 0);

        for (int i = 0; i < 64; i++) {
            gx.data[i] = static_cast<int16_t>((i % 10) * 100 - 300);
            gy.data[i] = static_cast<int16_t>((i % 7)  * 80  - 200);
        }

        Image scalarL1 = magnitudeL1(gx, gy);
        Image rvvL1    = magnitudeL1Rvv(gx, gy);
        Image scalarL2 = magnitudeL2(gx, gy);
        Image rvvL2    = magnitudeL2Rvv(gx, gy);

        for (int i = 0; i < 64; i++) {
            assert(scalarL1.data[i] == rvvL1.data[i]);
            assert(scalarL2.data[i] == rvvL2.data[i]);
        }
        printf("Test 2 passed: mixed positive/negative values match.\n");
    }

    // ── Test 3: large image to stress test strip-mining ────────
    // A 256x256 image forces many strip-mining iterations.
    // This verifies the loop boundary handling is correct.
    {
        Image16 gx, gy;
        gx.width  = gy.width  = 256;
        gx.height = gy.height = 256;
        gx.data.resize(256 * 256, 0);
        gy.data.resize(256 * 256, 0);

        for (int i = 0; i < 256 * 256; i++) {
            gx.data[i] = static_cast<int16_t>((i % 15) * 50 - 350);
            gy.data[i] = static_cast<int16_t>((i % 11) * 60 - 250);
        }

        Image scalarL1 = magnitudeL1(gx, gy);
        Image rvvL1    = magnitudeL1Rvv(gx, gy);
        Image scalarL2 = magnitudeL2(gx, gy);
        Image rvvL2    = magnitudeL2Rvv(gx, gy);

        for (int i = 0; i < 256 * 256; i++) {
            assert(scalarL1.data[i] == rvvL1.data[i]);
            assert(scalarL2.data[i] == rvvL2.data[i]);
        }
        printf("Test 3 passed: 256x256 image strip-mining is correct.\n");
    }

    printf("\nAll equivalence tests passed! RVV matches scalar at this VLEN.\n");
    return 0;
}
