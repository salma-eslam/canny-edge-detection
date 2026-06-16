#include "gradient.h"
#include <riscv_vector.h>  // gives us all RVV intrinsic functions
#include <cstdlib>         // for abs()
#include <cmath>           // for sqrt()
#include <vector>          // for std::vector

// ─────────────────────────────────────────────────────────────
// RVV version of L1 magnitude: result = |Gx| + |Gy|
//
// Instead of processing one pixel at a time like the scalar
// version, this function uses RISC-V Vector instructions to
// process multiple pixels simultaneously.
//
// The number of pixels processed per iteration (vl) is decided
// automatically by the hardware based on VLEN:
//   VLEN=128 → vl=8 pixels per iteration
//   VLEN=256 → vl=16 pixels per iteration
//   VLEN=512 → vl=32 pixels per iteration
// This is why RVV code works at ALL VLEN values without changes.
// ─────────────────────────────────────────────────────────────
Image magnitudeL1Rvv(const Image16& gx, const Image16& gy) {

    Image output;
    output.width  = gx.width;
    output.height = gx.height;
    int total = gx.width * gx.height;
    output.data.resize(total, 0);

    // Temporary storage for raw magnitude values before normalization.
    // We use int32_t here because |gx| + |gy| can be up to 65534
    // which does NOT fit in int16_t (max 32767) but fits in int32_t.
    std::vector<int32_t> raw(total, 0);

    // Track maximum value across all pixels for normalization later.
    int32_t maxVal = 0;

    // Raw pointers to pixel data — RVV intrinsics need raw pointers
    // not std::vector iterators.
    const int16_t* pGx = gx.data.data();
    const int16_t* pGy = gy.data.data();

    // i = current pixel index
    int i = 0;

    // ── Strip-mining loop ──────────────────────────────────────
    // Each iteration processes vl pixels at once.
    // vl is set by vsetvl based on remaining pixels and VLEN.
    // When remaining pixels < full vector width, vl is smaller —
    // this handles images whose size is not a multiple of vl.
    // ──────────────────────────────────────────────────────────
    for (size_t vl; i < total; i += vl) {

        // Ask hardware: how many int16 elements to process now?
        // (total - i) = remaining pixels
        // e16 = each element is 16 bits (int16_t)
        // m1  = LMUL=1, use 1 vector register group
        // Higher LMUL uses more registers but processes more elements
        // LMUL=1 is a safe balanced choice for most cases
        vl = __riscv_vsetvl_e16m1(total - i);

        // Load vl int16 elements from gx starting at position i
        // vint16m1_t = a vector of int16 values using 1 register
        vint16m1_t vgx = __riscv_vle16_v_i16m1(pGx + i, vl);

        // Load vl int16 elements from gy starting at position i
        vint16m1_t vgy = __riscv_vle16_v_i16m1(pGy + i, vl);

        // Compute absolute value manually using negate then max.
        // RVV has no integer vabs, so we:
        // 1. Negate the vector
        // 2. Take element-wise max of original and negated
        // max(x, -x) always gives the absolute value.
        vint16m1_t vnegGx = __riscv_vneg_v_i16m1(vgx, vl);
        vint16m1_t vabsGx = __riscv_vmax_vv_i16m1(vgx, vnegGx, vl);

        vint16m1_t vnegGy = __riscv_vneg_v_i16m1(vgy, vl);
        vint16m1_t vabsGy = __riscv_vmax_vv_i16m1(vgy, vnegGy, vl);

        // Widen from int16 to int32 before adding.
        // Reason: |gx| can be up to 32767 and |gy| up to 32767.
        // Their sum 65534 overflows int16 (max 32767).
        // int32 (max 2,147,483,647) safely holds 65534.
        // vwcvt = vector widen convert
        // m2 = result uses 2 register groups (wider elements need more space)
        vint32m2_t vwideGx = __riscv_vwcvt_x_x_v_i32m2(vabsGx, vl);
        vint32m2_t vwideGy = __riscv_vwcvt_x_x_v_i32m2(vabsGy, vl);

        // Add all gx and gy absolute values at once.
        // result[j] = |gx[i+j]| + |gy[i+j]| for all j in 0..vl
        // This replaces the scalar: raw[i] = abs(gx[i]) + abs(gy[i]);
        vint32m2_t vsum = __riscv_vadd_vv_i32m2(vwideGx, vwideGy, vl);

        // Store all vl results back to raw array at position i.
        // vse32 = vector store element 32-bit
        __riscv_vse32_v_i32m2(raw.data() + i, vsum, vl);

        // Find maximum in this batch.
        // No RVV reduction here to keep code simple and correct.
        for (size_t j = 0; j < vl; j++) {
            if (raw[i + j] > maxVal) {
                maxVal = raw[i + j];
            }
        }
    }

    // Normalize all values to 0-255 range.
    // We divide each value by the maximum and multiply by 255.
    // This keeps relative differences but fits everything in 0-255.
    if (maxVal > 0) {
        for (int j = 0; j < total; j++) {
            output.data[j] = static_cast<uint8_t>(
                (raw[j] * 255) / maxVal
            );
        }
    }

    return output;
}

// ─────────────────────────────────────────────────────────────
// RVV version of L2 magnitude: result = sqrt(Gx² + Gy²)
//
// We use RVV for the multiply and add steps.
// We fall back to scalar for sqrt because RVV integer units
// do not have a sqrt instruction.
// We still get significant speedup from the multiply and add.
// ─────────────────────────────────────────────────────────────
Image magnitudeL2Rvv(const Image16& gx, const Image16& gy) {

    Image output;
    output.width  = gx.width;
    output.height = gx.height;
    int total = gx.width * gx.height;
    output.data.resize(total, 0);

    // Use double for accuracy — sqrt needs floating point precision.
    std::vector<double> raw(total, 0.0);
    double maxVal = 0.0;

    const int16_t* pGx = gx.data.data();
    const int16_t* pGy = gy.data.data();

    int i = 0;

    for (size_t vl; i < total; i += vl) {

        // Set vector length for int16 elements
        vl = __riscv_vsetvl_e16m1(total - i);

        // Load gx and gy vectors
        vint16m1_t vgx = __riscv_vle16_v_i16m1(pGx + i, vl);
        vint16m1_t vgy = __riscv_vle16_v_i16m1(pGy + i, vl);

        // Widen to int32 before multiplying.
        // Reason: gx² can be up to 32767² = 1,073,741,289
        // which overflows int16 (max 32767) and needs int32.
        vint32m2_t vwideGx = __riscv_vwcvt_x_x_v_i32m2(vgx, vl);
        vint32m2_t vwideGy = __riscv_vwcvt_x_x_v_i32m2(vgy, vl);

        // Multiply gx*gx element-wise for all vl pixels at once.
        // vmul = vector multiply
        vint32m2_t vgx2 = __riscv_vmul_vv_i32m2(vwideGx, vwideGx, vl);

        // Multiply gy*gy element-wise for all vl pixels at once.
        vint32m2_t vgy2 = __riscv_vmul_vv_i32m2(vwideGy, vwideGy, vl);

        // Add gx² + gy² for all vl pixels at once.
        vint32m2_t vsum = __riscv_vadd_vv_i32m2(vgx2, vgy2, vl);

        // Store results to temporary buffer.
        // We need scalar sqrt so we store the int32 values first.
        std::vector<int32_t> tmp(vl);
        __riscv_vse32_v_i32m2(tmp.data(), vsum, vl);

        // Compute sqrt in scalar — no RVV sqrt for integers.
        // This is the only scalar part of the L2 function.
        for (size_t j = 0; j < vl; j++) {
            raw[i + j] = sqrt(static_cast<double>(tmp[j]));
            if (raw[i + j] > maxVal) {
                maxVal = raw[i + j];
            }
        }
    }

    // Normalize to 0-255 range.
    if (maxVal > 0.0) {
        for (int j = 0; j < total; j++) {
            output.data[j] = static_cast<uint8_t>(
                (raw[j] / maxVal) * 255.0
            );
        }
    }

    return output;
}
