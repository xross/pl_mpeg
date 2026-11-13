/* Checks the decoder's own plm_video_idct() entry point.
 *
 * 16-bit backends: bit-exact against the scalar model (on xcore this is the
 * vector unit against portable C), in place, with canaries either side of the
 * padded block, and on a host also against the exact transform.
 * Every backend: on xcore, time the transform as the decoder calls it, so a
 * PLM_VIDEO_IDCT_ORIGINAL build gives the baseline for the same blocks. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifndef PLM_VIDEO_IDCT
#ifdef __XS3A__
#define PLM_VIDEO_IDCT 2
#else
#define PLM_VIDEO_IDCT 1
#endif
#endif
#define PLM_NO_STDIO
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"
#ifdef __xcore__
#include <xcore/hwtimer.h>
#else
#include <math.h>
#endif

static uint32_t random_state = 1234567;
static unsigned next_random(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

#if PLM_VIDEO_IDCT != PLM_VIDEO_IDCT_ORIGINAL
static void check_accuracy(void)
{
    int16_t input[64] = {0}, temporary[64], reference[64];
    struct { int16_t pre[4]; plm_video_coeff_t block[PLM_VIDEO_BLOCK_SIZE]; int16_t post[4]; }
        guarded __attribute__((aligned(8)));
    unsigned peak = 0;
    double squared_error = 0;
#ifndef __xcore__
    double basis[8][8];
    for (unsigned x = 0; x < 8; ++x)
        for (unsigned u = 0; u < 8; ++u)
            basis[x][u] = (u ? 1 : sqrt(0.5)) * cos((2 * x + 1) * u * acos(-1) / 16);
#endif
    enum { TEST_BLOCKS = 388 };
    for (unsigned test = 0; test < TEST_BLOCKS; ++test) {
        memset(input, 0, sizeof(input));
        if (test < 128) input[test / 2] = test & 1 ? -2048 : 2047;
        else if (test >= 384) {
            for (unsigned i = 0; i < 64; ++i) {
                if (test == 384) input[i] = 2047;
                if (test == 385) input[i] = -2048;
                if (test == 386) input[i] = i & 1 ? -2048 : 2047;
                if (test == 387) input[i] = (plm_idct_matrix[3][i % 8] < 0) ^
                                           (plm_idct_matrix[2][i / 8] < 0) ? -2048 : 2047;
            }
        }
        else for (unsigned i = 0; i < 64; ++i)
            input[i] = (int16_t)((next_random() & 4095) - 2048);
        plm_idct_scalar_pass(temporary, input, 13);
        plm_idct_scalar_pass(reference, temporary, 17);
        /* Non-zero padding: the result must not depend on what it holds. */
        memset(&guarded, 0x5a, sizeof(guarded));
        memcpy(guarded.block, input, sizeof(input));
        plm_video_idct(guarded.block);
        assert(!memcmp(reference, guarded.block, sizeof(reference)));
        for (unsigned i = 0; i < 4; ++i)
            assert(guarded.pre[i] == 0x5a5a && guarded.post[i] == 0x5a5a);
        for (unsigned i = 64; i < PLM_VIDEO_BLOCK_SIZE; ++i)
            assert(guarded.block[i] == 0x5a5a);
#ifndef __xcore__
        for (unsigned y = 0; y < 8; ++y) {
            for (unsigned x = 0; x < 8; ++x) {
                double value = 0;
                for (unsigned v = 0; v < 8; ++v)
                    for (unsigned u = 0; u < 8; ++u)
                        value += input[v * 8 + u] * basis[x][u] * basis[y][v];
                int exact = (int)floor(value / 4 + 0.5);
                int delta = reference[y * 8 + x] - exact;
                unsigned error = (unsigned)(delta < 0 ? -delta : delta);
                if (error > peak) peak = error;
                squared_error += delta * delta;
            }
        }
#endif
    }
#ifdef __xcore__
    (void)peak; (void)squared_error;
    puts("PASS: IDCT matches the scalar model: basis/extreme/random blocks, padding and canaries");
#else
    printf("PASS: IDCT scalar model, basis/extreme/random blocks, padding and canaries; mathematical peak=%u MSE=%f\n",
           peak, squared_error / (TEST_BLOCKS * 64));
    assert(peak <= 1);
#endif
}
#endif

#ifdef __xcore__
/* Dequantized coefficients, as the decoder would hold them before the
 * transform: every position, or a DC plus three low-frequency terms. */
static void time_blocks(const char *name, unsigned sparse)
{
    static volatile int observed;
    plm_video_coeff_t source[PLM_VIDEO_BLOCK_SIZE] PLM_VIDEO_BLOCK_ALIGNED = {0};
    plm_video_coeff_t block[PLM_VIDEO_BLOCK_SIZE] PLM_VIDEO_BLOCK_ALIGNED;
    for (unsigned i = 0; i < 64; ++i) {
        if (sparse && i != 0 && i != 1 && i != 8 && i != 9) continue;
        int level = (int)(next_random() & 255) - 128;
#if PLM_VIDEO_IDCT == PLM_VIDEO_IDCT_ORIGINAL
        level *= PLM_VIDEO_PREMULTIPLIER_MATRIX[i];
#endif
        source[i] = (plm_video_coeff_t)level;
    }
    enum { BLOCKS = 1000 };
    unsigned start = get_reference_time();
    for (unsigned i = 0; i < BLOCKS; ++i) {
        memcpy(block, source, sizeof(block));
        observed = block[i & 63];
    }
    unsigned overhead = get_reference_time() - start;
    start = get_reference_time();
    for (unsigned i = 0; i < BLOCKS; ++i) {
        memcpy(block, source, sizeof(block));
        plm_video_idct(block);
        observed = block[i & 63];
    }
    unsigned total = get_reference_time() - start;
    printf("IDCT timing: backend=%d %s=%u ns/block\n", PLM_VIDEO_IDCT, name,
           (total - overhead) * 10 / BLOCKS);
}
#endif

int main(void)
{
#if PLM_VIDEO_IDCT != PLM_VIDEO_IDCT_ORIGINAL
    check_accuracy();
#endif
#ifdef __xcore__
    time_blocks("dense", 0);
    time_blocks("sparse", 1);
#endif
    puts("IDCT test complete");
    return 0;
}
