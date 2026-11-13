#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define PLM_NO_STDIO
#define PLM_AUDIO_FAST_WINDOW 1
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"
#ifdef __xcore__
#include <xcore/hwtimer.h>
static volatile float observed;
#endif

static void reference(float output[32], const float window[1024],
                      const float history[1024], unsigned position)
{
    memset(output, 0, 32 * sizeof(float));
    unsigned d = 512 - (position >> 1), v = (position % 128) >> 1;
    while (v < 1024) {
        for (unsigned i = 0; i < 32; ++i) output[i] += window[d++] * history[v++];
        v += 96; d += 32;
    }
    d -= 480;
    v = 1120 - v;
    while (v < 1024) {
        for (unsigned i = 0; i < 32; ++i) output[i] += window[d++] * history[v++];
        v += 96; d += 32;
    }
}

int main(void)
{
    float window[1024], history[1024], expected[32];
    struct { unsigned before[2]; float samples[32]; unsigned after[2]; } output;
    uint32_t state = 31337;
    for (unsigned i = 0; i < 1024; ++i) {
        state = state * 1664525u + 1013904223u;
        window[i] = (float)((int)(state >> 16) - 32768);
        state = state * 1664525u + 1013904223u;
        history[i] = (float)((int)(state >> 16) - 32768) / 32768;
    }
    double signal = 0, noise = 0;
    for (unsigned position = 0; position < 1024; position += 64) {
        memset(&output, 0x5a, sizeof(output));
        reference(expected, window, history, position);
        plm_audio_synthesis_window(output.samples, window, history, position);
        for (unsigned i = 0; i < 32; ++i) {
            double delta = (double)output.samples[i] - expected[i];
            signal += (double)expected[i] * expected[i];
            noise += delta * delta;
        }
        assert(output.before[0] == 0x5a5a5a5a && output.before[1] == 0x5a5a5a5a);
        assert(output.after[0] == 0x5a5a5a5a && output.after[1] == 0x5a5a5a5a);
    }
    assert(noise < signal * 1e-12);
#ifdef __xcore__
    unsigned start = get_reference_time();
    for (unsigned i = 0; i < 64; ++i) {
        reference(expected, window, history, (i & 15) * 64);
        observed = expected[i & 31];
    }
    unsigned baseline = get_reference_time() - start;
    start = get_reference_time();
    for (unsigned i = 0; i < 64; ++i) {
        plm_audio_synthesis_window(output.samples, window, history, (i & 15) * 64);
        observed = output.samples[i & 31];
    }
    unsigned fast = get_reference_time() - start;
    printf("Window: reference=%u fused=%u ticks/64 windows\n", baseline, fast);
#endif
    puts("PASS: fused synthesis window, all history phases, SNR > 120 dB, canaries");
    return 0;
}
