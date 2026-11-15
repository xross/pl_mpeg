#ifndef PL_MPEG_AUDIO_S32_H
#define PL_MPEG_AUDIO_S32_H

/* Standalone fixed-point MP2 synthesis. No platform/library dependency.
 * Transform: Q27 constants, Q9 history; the generated affine proof includes
 * every rounded butterfly intermediate, not just the final transform gain.
 * Window: XS3-compatible rounded Q30 products. Sixteen of them are at most
 * 2^36 in total, so the XS3 40-bit saturating accumulator never saturates and
 * the model needs no limit on the sum.
 * The coefficients include output scaling; the final left shift gives S32 PCM.
 * Right shifts of negative integers require arithmetic shift (as on xcore and
 * the supported C99 host compilers). No signed left shift or integer division. */
#include <stdint.h>
#include <limits.h>
#include "pl_mpeg_audio_s32_tables.h"

/* The transform's bound holds for subband samples of at most 65536 in
 * magnitude. Every valid sample code is within it, and so is every ungrouped
 * code. A grouped code beyond the largest valid one (levels^3 - 1) is not: it
 * can give 104848. The decoder limits a grouped code to the valid range. */
static inline int plm_audio_group_code_s32(int code, int levels) {
    int limit = levels * levels * levels - 1;
    return code > limit ? limit : code;
}

static inline int32_t plm_audio_sat_s32(int64_t x) {
    return x > INT32_MAX ? INT32_MAX : x < -INT32_MAX ? -INT32_MAX : (int32_t)x;
}

static inline int32_t plm_audio_mul_s32(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a*b + (INT64_C(1) << 26)) >> 27);
}

static inline int64_t plm_audio_macc_s32(int64_t accumulator, int32_t a, int32_t b) {
    return accumulator + (((int64_t)a*b + (INT64_C(1) << 29)) >> 30);
}

#include "pl_mpeg_audio_s32_transform.h"

static inline void plm_audio_window_s32(int32_t output[32], const int32_t window[1024],
                                const int32_t history[1024], unsigned position) {
    const int32_t *d = window + 512 - (position >> 1);
    unsigned first = (position & 127) >> 1, second = 96-first;
    for (unsigned i = 0; i < 32; ++i) {
        int64_t accumulator = 0;
        for (unsigned half = 0; half < 2; ++half) {
            const int32_t *w = d+i+half*32;
            const int32_t *v = history+i+(half ? second : first);
            for (unsigned tap = 0; tap < 8; ++tap, w += 64, v += 128)
                accumulator = plm_audio_macc_s32(accumulator, *w, *v);
        }
        output[i] = plm_audio_sat_s32(accumulator * (INT64_C(1) << PLM_AUDIO_WINDOW_LEFT));
    }
}
#endif
