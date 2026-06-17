#include <riscv_vector.h>
#include "sobel.h"

// Fused RISC-V Vector (RVV 1.0) Sobel Filter Implementation
// This function calculates both horizontal (Gx) and vertical (Gy) gradients in a single pass.
// Fusing the loops means we only read the input image from memory once, saving massive cache bandwidth.
void sobel_fused_rvv(const Image& input, Image16& out_gx, Image16& out_gy) {
    int W = input.width;
    int H = input.height;

    // Set the output image dimensions to match the input
    out_gx.width = W; out_gx.height = H;
    out_gy.width = W; out_gy.height = H;
    
    // Fill both output buffers with zeros right from the start.
    // This perfectly handles the zero-padding requirement for the image borders
    // without needing slow, complex boundary checks inside our inner loops.
    out_gx.data.assign(W * H, 0);
    out_gy.data.assign(W * H, 0);

    // Pull out raw hardware data pointers from the std::vectors.
    // This stops "pointer aliasing" issues, letting the compiler know that our 
    // input and output buffers don't overlap in memory, maximizing processing speed.
    const uint8_t* img_ptr = input.data.data();
    int16_t* __restrict gx_ptr = out_gx.data.data();
    int16_t* __restrict gy_ptr = out_gy.data.data();

    // Loop through the internal rows, skipping the very top and very bottom lines
    for (int y = 1; y < H - 1; ++y) {
        
        // Find the starting memory addresses for the row above, current row, and row below
        const uint8_t* row_prev = &img_ptr[(y - 1) * W];
        const uint8_t* row_curr = &img_ptr[y * W];
        const uint8_t* row_next = &img_ptr[(y + 1) * W];

        int x = 1;
        int remaining_pixels = W - 2; // Process inner columns, skipping the first and last columns

        // Strip-mining loop to process columns in chunks dynamically
        for (size_t vl; remaining_pixels > 0; remaining_pixels -= vl, x += vl) {
            
            // Configure the vector engine based on our 8-bit inputs with LMUL=1.
            // This ensures our vector lengths match the actual capacity of our 8-bit loads.
            vl = __riscv_vsetvl_e8m1(remaining_pixels);

            // Load all 9 neighbor pixels for the current sliding 3x3 window chunk.
            // We use 8-bit unsigned loads because pixels are packed as bytes in RAM.

            // Left-column neighbors (shifted left by 1 element)
            vuint8m1_t v_p00 = __riscv_vle8_v_u8m1(&row_prev[x - 1], vl); // Top-Left
            vuint8m1_t v_p10 = __riscv_vle8_v_u8m1(&row_curr[x - 1], vl); // Mid-Left
            vuint8m1_t v_p20 = __riscv_vle8_v_u8m1(&row_next[x - 1], vl); // Bottom-Left

            // Center-column neighbors (directly at the current column index x)
            // (Note: We skip loading row_curr[x] because its coefficient is 0 in both Sobel matrices)
            vuint8m1_t v_p01 = __riscv_vle8_v_u8m1(&row_prev[x], vl);     // Top-Center
            vuint8m1_t v_p21 = __riscv_vle8_v_u8m1(&row_next[x], vl);     // Bottom-Center

            // Right-column neighbors (shifted right by 1 element)
            vuint8m1_t v_p02 = __riscv_vle8_v_u8m1(&row_prev[x + 1], vl); // Top-Right
            vuint8m1_t v_p12 = __riscv_vle8_v_u8m1(&row_curr[x + 1], vl); // Mid-Right
            vuint8m1_t v_p22 = __riscv_vle8_v_u8m1(&row_next[x + 1], vl); // Bottom-Right

            // Widen our 8-bit unsigned pixels into signed 16-bit registers (LMUL becomes 2).
            // vwcvtu widens unsigned u8 → unsigned u16 (preserving values 0-255 as positive).
            // vreinterpret then reinterprets the bit pattern as signed i16 — safe because
            // all values 0-255 fit within i16's positive range (max 32767), so no bits change.
            // This gives us signed i16m2 registers ready for subtraction in the Sobel math.
            vint16m2_t p00_16 = __riscv_vreinterpret_v_u16m2_i16m2(__riscv_vwcvtu_x_x_v_u16m2(v_p00, vl));
            vint16m2_t p10_16 = __riscv_vreinterpret_v_u16m2_i16m2(__riscv_vwcvtu_x_x_v_u16m2(v_p10, vl));
            vint16m2_t p20_16 = __riscv_vreinterpret_v_u16m2_i16m2(__riscv_vwcvtu_x_x_v_u16m2(v_p20, vl));

            vint16m2_t p01_16 = __riscv_vreinterpret_v_u16m2_i16m2(__riscv_vwcvtu_x_x_v_u16m2(v_p01, vl));
            vint16m2_t p21_16 = __riscv_vreinterpret_v_u16m2_i16m2(__riscv_vwcvtu_x_x_v_u16m2(v_p21, vl));

            vint16m2_t p02_16 = __riscv_vreinterpret_v_u16m2_i16m2(__riscv_vwcvtu_x_x_v_u16m2(v_p02, vl));
            vint16m2_t p12_16 = __riscv_vreinterpret_v_u16m2_i16m2(__riscv_vwcvtu_x_x_v_u16m2(v_p12, vl));
            vint16m2_t p22_16 = __riscv_vreinterpret_v_u16m2_i16m2(__riscv_vwcvtu_x_x_v_u16m2(v_p22, vl));

            // ─────────────────────────────────────────────────────────────────
            // COMPUTE SOBEL X (Horizontal Edge Detection)
            // Kernel: [ -1  0  +1 ]
            //         [ -2  0  +2 ]
            //         [ -1  0  +1 ]
            // ─────────────────────────────────────────────────────────────────
            // Add right-hand columns together: Top-Right + Bottom-Right
            vint16m2_t v_gx = __riscv_vadd_vv_i16m2(p02_16, p22_16, vl);
            // Multiply Mid-Right by 2 using a fast bitwise left-shift, then accumulate
            v_gx = __riscv_vadd_vv_i16m2(v_gx, __riscv_vsll_vx_i16m2(p12_16, 1, vl), vl);

            // Subtract left-hand columns: Top-Left and Bottom-Left
            v_gx = __riscv_vsub_vv_i16m2(v_gx, p00_16, vl);
            v_gx = __riscv_vsub_vv_i16m2(v_gx, p20_16, vl);
            // Multiply Mid-Left by 2 using bitwise left-shift, then subtract
            v_gx = __riscv_vsub_vv_i16m2(v_gx, __riscv_vsll_vx_i16m2(p10_16, 1, vl), vl);

            // ─────────────────────────────────────────────────────────────────
            // COMPUTE SOBEL Y (Vertical Edge Detection)
            // Kernel: [ -1 -2 -1 ]
            //         [  0  0  0 ]
            //         [ +1 +2 +1 ]
            // ─────────────────────────────────────────────────────────────────
            // Add bottom row elements together: Bottom-Left + Bottom-Right
            vint16m2_t v_gy = __riscv_vadd_vv_i16m2(p20_16, p22_16, vl);
            // Multiply Bottom-Center by 2 via left-shift and accumulate
            v_gy = __riscv_vadd_vv_i16m2(v_gy, __riscv_vsll_vx_i16m2(p21_16, 1, vl), vl);

            // Subtract top row elements: Top-Left and Top-Right
            v_gy = __riscv_vsub_vv_i16m2(v_gy, p00_16, vl);
            v_gy = __riscv_vsub_vv_i16m2(v_gy, p02_16, vl);
            // Multiply Top-Center by 2 via left-shift and subtract
            v_gy = __riscv_vsub_vv_i16m2(v_gy, __riscv_vsll_vx_i16m2(p01_16, 1, vl), vl);

            // Stream the calculated 16-bit results directly back out to system memory.
            // The 2D-to-1D mapping index formula `y * W + x` places them perfectly in the arrays.
            __riscv_vse16_v_i16m2(&gx_ptr[y * W + x], v_gx, vl);
            __riscv_vse16_v_i16m2(&gy_ptr[y * W + x], v_gy, vl);
        }
    }
}
