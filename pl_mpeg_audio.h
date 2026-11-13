#ifndef PL_MPEG_AUDIO_H
#define PL_MPEG_AUDIO_H

/* Optional synthesis kernel; uses the existing decoder history and output
 * arrays, not another persistent buffer. Reference arithmetic is retained
 * in pl_mpeg.h when PLM_AUDIO_FAST_WINDOW is zero. */

/* A platform header may define PLM_AUDIO_ACCUMULATE(sum, a, b), a fused
 * sum + a * b, and PLM_AUDIO_WINDOW_ATTRIBUTES for the kernel below. */
#if PLM_AUDIO_FAST_WINDOW
#ifndef PLM_AUDIO_ACCUMULATE
#include <math.h>
#define PLM_AUDIO_ACCUMULATE(sum, a, b) fmaf(a, b, sum)
#endif
#ifndef PLM_AUDIO_WINDOW_ATTRIBUTES
#define PLM_AUDIO_WINDOW_ATTRIBUTES
#endif

static PLM_AUDIO_WINDOW_ATTRIBUTES void
plm_audio_synthesis_window(float output[32], const float window[1024],
                    const float history[1024], unsigned position)
{
    const float *d = window + 512 - (position >> 1);
    unsigned first = (position & 127) >> 1;
    unsigned second = 96 - first;
    for (unsigned i = 0; i < 32; i += 4) {
        float a = 0, b = 0, c = 0, e = 0;
        for (unsigned half = 0; half < 2; ++half) {
            const float *w = d + i + half * 32;
            const float *v = history + i + (half ? second : first);
            for (unsigned tap = 0; tap < 8; ++tap, w += 64, v += 128) {
                a = PLM_AUDIO_ACCUMULATE(a, w[0], v[0]);
                b = PLM_AUDIO_ACCUMULATE(b, w[1], v[1]);
                c = PLM_AUDIO_ACCUMULATE(c, w[2], v[2]);
                e = PLM_AUDIO_ACCUMULATE(e, w[3], v[3]);
            }
        }
        output[i] = a;
        output[i + 1] = b;
        output[i + 2] = c;
        output[i + 3] = e;
    }
}
#endif
#endif
