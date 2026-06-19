#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include "gradient.h"

// ── Timing helper ─────────────────────────────────────────────
static double getElapsedMs(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
}

// ── Correctness check ─────────────────────────────────────────
static bool imagesMatch(const Image& a, const Image& b, int tolerance) {
    if (a.width != b.width || a.height != b.height) {
        return false;
    }
    int total = a.width * a.height;
    for (int i = 0; i < total; i++) {
        int diff = abs((int)a.data[i] - (int)b.data[i]);
        if (diff > tolerance) {
            printf("  MISMATCH at pixel %d: scalar=%d rvv=%d diff=%d\n",
                   i, a.data[i], b.data[i], diff);
            return false;
        }
    }
    return true;
}

int main() {
    // ── Setup ─────────────────────────────────────────────────
    const int W = 49;
    const int H = 47;
    const int TOTAL = W * H;

    Image16 gx, gy;
    gx.width  = gy.width  = W;
    gx.height = gy.height = H;
    gx.data.resize(TOTAL);
    gy.data.resize(TOTAL);

    for (int i = 0; i < TOTAL; i++) {
        gx.data[i] = static_cast<int16_t>((i % 13) * 120 - 700);
        gy.data[i] = static_cast<int16_t>((i % 9)  * 90  - 400);
    }

    printf("=== Magnitude Benchmark: Scalar vs RVV ===\n");
    printf("Image size: %dx%d = %d pixels\n", W, H, TOTAL);
    printf("(Non-power-of-two forces strip-mining tail case)\n\n");

    // ── Step 1: Correctness check ──────────────────────────────
    printf("--- Correctness Check (tolerance = +-1) ---\n");

    Image scalarL1 = magnitudeL1(gx, gy);
    Image rvvL1    = magnitudeL1Rvv(gx, gy);
    Image scalarL2 = magnitudeL2(gx, gy);
    Image rvvL2    = magnitudeL2Rvv(gx, gy);

    if (imagesMatch(scalarL1, rvvL1, 1)) {
        printf("L1: PASS - scalar and RVV outputs match (+-1)\n");
    } else {
        printf("L1: FAIL - outputs do not match!\n");
    }

    if (imagesMatch(scalarL2, rvvL2, 1)) {
        printf("L2: PASS - scalar and RVV outputs match (+-1)\n");
    } else {
        printf("L2: FAIL - outputs do not match!\n");
    }

    // ── Step 2: Speed benchmark ────────────────────────────────
    const int ITERATIONS = 150;
    struct timespec start, end;

    printf("\n--- Speed Benchmark (%d iterations) ---\n", ITERATIONS);

    // Benchmark scalar L1
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int iter = 0; iter < ITERATIONS; iter++) {
        magnitudeL1(gx, gy);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double timeScalarL1 = getElapsedMs(start, end);

    // Benchmark RVV L1
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int iter = 0; iter < ITERATIONS; iter++) {
        magnitudeL1Rvv(gx, gy);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double timeRvvL1 = getElapsedMs(start, end);

    // Benchmark scalar L2
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int iter = 0; iter < ITERATIONS; iter++) {
        magnitudeL2(gx, gy);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double timeScalarL2 = getElapsedMs(start, end);

    // Benchmark RVV L2
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int iter = 0; iter < ITERATIONS; iter++) {
        magnitudeL2Rvv(gx, gy);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double timeRvvL2 = getElapsedMs(start, end);

    // ── Step 3: Print results ──────────────────────────────────
    printf("\n--- Results ---\n");
    printf("magnitudeL1  scalar: %.4f ms\n", timeScalarL1);
    printf("magnitudeL1  RVV:    %.4f ms\n", timeRvvL1);
    printf("L1 speedup:          %.2fx\n\n", timeScalarL1 / timeRvvL1);

    printf("magnitudeL2  scalar: %.4f ms\n", timeScalarL2);
    printf("magnitudeL2  RVV:    %.4f ms\n", timeRvvL2);
    printf("L2 speedup:          %.2fx\n", timeScalarL2 / timeRvvL2);

    printf("\nRun at VLEN=128, 256, 512 to see VLEN sweep results.\n");

    return 0;
}
