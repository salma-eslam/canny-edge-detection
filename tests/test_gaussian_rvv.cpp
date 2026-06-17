#include "../src/gaussian.h"
#include "../src/gaussian_rvv.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================
// Equivalence test (spec section 3.2)
// Compares scalar gaussianBlur() against RVV gaussianBlurRVV()
// on the same input. Allows ±1 tolerance for rounding because
// the RVV version uses fixed-point (sum*240)>>16 instead of
// exact division by 273.
//
// This is an assert-based test harness (not GoogleTest) because
// it must run on QEMU with the cross-compiled RVV binary.
//
// Uses a non-power-of-two size (100x75) per spec hint in section
// 3.2: "Use a non-power-of-two image size to force the strip-mining
// tail case." Width 100 is not evenly divisible by typical vl values
// at VLEN=128 (vl=16) or VLEN=256 (vl=32), so this forces partial
// strips at the end of each row — exactly where VLA bugs hide.
// ============================================================

int main() {
    int W = 100, H = 75; // non-power-of-two, per spec hint
    int failures = 0;

    // --------------------------------------------------------
    // Test 1: Random-ish pattern image
    // --------------------------------------------------------
    {
        Image input;
        input.width  = W;
        input.height = H;
        input.data.resize(W * H);

        // deterministic pseudo-random pattern (no actual randomness,
        // so results are reproducible across runs)
        for (int i = 0; i < W * H; i++) {
            input.data[i] = (uint8_t)((i * 37 + 11) % 256);
        }

        Image scalarOut = gaussianBlur(input);
        Image rvvOut    = gaussianBlurRVV(input);

        int mismatches = 0;
        for (int i = 0; i < W * H; i++) {
            int diff = (int)scalarOut.data[i] - (int)rvvOut.data[i];
            if (diff < -1 || diff > 1) {
                mismatches++;
                if (mismatches <= 5) {
                    printf("MISMATCH at pixel %d: scalar=%d rvv=%d diff=%d\n",
                           i, scalarOut.data[i], rvvOut.data[i], diff);
                }
            }
        }

        if (mismatches == 0) {
            printf("[PASS] Test 1 (random pattern): scalar and RVV match within +/-1\n");
        } else {
            printf("[FAIL] Test 1 (random pattern): %d mismatches out of %d pixels\n",
                   mismatches, W * H);
            failures++;
        }
    }

    // --------------------------------------------------------
    // Test 2: Uniform image (all 128) — sanity check
    // --------------------------------------------------------
    {
        Image input;
        input.width  = W;
        input.height = H;
        input.data.resize(W * H, 128);

        Image scalarOut = gaussianBlur(input);
        Image rvvOut    = gaussianBlurRVV(input);

        int mismatches = 0;
        for (int i = 0; i < W * H; i++) {
            int diff = (int)scalarOut.data[i] - (int)rvvOut.data[i];
            if (diff < -1 || diff > 1) mismatches++;
        }

        if (mismatches == 0) {
            printf("[PASS] Test 2 (uniform image): scalar and RVV match within +/-1\n");
        } else {
            printf("[FAIL] Test 2 (uniform image): %d mismatches\n", mismatches);
            failures++;
        }
    }

    // --------------------------------------------------------
    // Test 3: All-black image — edge case
    // --------------------------------------------------------
    {
        Image input;
        input.width  = W;
        input.height = H;
        input.data.resize(W * H, 0);

        Image scalarOut = gaussianBlur(input);
        Image rvvOut    = gaussianBlurRVV(input);

        int mismatches = 0;
        for (int i = 0; i < W * H; i++) {
            if (scalarOut.data[i] != rvvOut.data[i]) mismatches++;
        }

        if (mismatches == 0) {
            printf("[PASS] Test 3 (all-black image): scalar and RVV match exactly\n");
        } else {
            printf("[FAIL] Test 3 (all-black image): %d mismatches\n", mismatches);
            failures++;
        }
    }

    // --------------------------------------------------------
    // Summary
    // --------------------------------------------------------
    if (failures == 0) {
        printf("\nALL TESTS PASSED (W=%d H=%d, non-power-of-two)\n", W, H);
        return 0; // success exit code
    } else {
        printf("\n%d TEST(S) FAILED\n", failures);
        return 1; // failure exit code
    }
}
