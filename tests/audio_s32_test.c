#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define PLM_NO_STDIO
#define PLM_AUDIO_FAST_WINDOW 1
#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"
#include "pl_mpeg_audio_s32.h"
#include "pl_mpeg_audio.h"
#ifdef __xcore__
#include <xcore/hwtimer.h>
#endif

static uint32_t random_state = 31337;
static uint32_t random_word(void) {
    random_state = random_state*1664525u+1013904223u;
    return random_state;
}
static int input[32][3];
#ifndef PLM_AUDIO_TEST_QUICK
static double matrix[64][32];
static struct { uint32_t before[2]; int32_t data[64]; uint32_t after[2]; } transform;
#endif
static int32_t history[1024], window[1024];
static float float_history[1024], float_window[1024], float_output[32];
static struct { uint32_t before[2]; int32_t data[32]; uint32_t after[2]; } output;
static volatile int32_t observed;
static volatile float float_observed;

#ifndef PLM_AUDIO_TEST_QUICK
static double clipped(double x) {
    return x > INT32_MAX ? INT32_MAX : x < -INT32_MAX ? -INT32_MAX : x;
}
#endif

int main(void) {
    /* Grouped codes: three base-`levels` digits. Larger codes are invalid. */
    assert(plm_audio_group_code_s32(1023, 9) == 728 && plm_audio_group_code_s32(728, 9) == 728);
    assert(plm_audio_group_code_s32(31, 3) == 26 && plm_audio_group_code_s32(127, 5) == 124);
    assert(plm_audio_group_code_s32(0, 3) == 0);
    assert(plm_audio_sat_s32(INT64_MAX) == INT32_MAX);
    assert(plm_audio_sat_s32(INT64_MIN) == -INT32_MAX);
    assert(plm_audio_mul_s32(1, 1 << 26) == 1);
    assert(plm_audio_mul_s32(-1, 1 << 26) == 0); /* ties toward +infinity */
    /* Sixteen extreme products stay far inside the XS3 40-bit accumulator. */
    {
        int64_t extreme = 0;
        for (unsigned tap = 0; tap < 16; ++tap) extreme = plm_audio_macc_s32(extreme, INT32_MIN, INT32_MIN);
        assert(extreme == INT64_C(16) << 32 && extreme < (INT64_C(1) << 39) - 1);
    }
#ifndef PLM_AUDIO_TEST_QUICK
    double transform_peak=0, window_peak=0;
    for (unsigned i=0;i<64;++i) for(unsigned k=0;k<32;++k)
        matrix[i][k] = cos((16+i)*(2*k+1)*3.14159265358979323846/64);
    for (unsigned pattern=0;pattern<100;++pattern) {
        for(unsigned k=0;k<32;++k) for(unsigned p=0;p<3;++p)
            input[k][p] = pattern<32 ? (pattern==k ? 65536 : 0) :
                pattern==32 ? 65536 : pattern==33 ? -65536 :
                pattern==34 ? (k&1 ? 65536 : -65536) : (int)(random_word()%131073)-65536;
        for (unsigned p=0;p<3;++p) {
            memset(&transform,0x5a,sizeof(transform));
            plm_audio_idct36_s32(input,p,transform.data,0);
            for(unsigned i=0;i<64;++i) {
                double reference=0;
                for(unsigned k=0;k<32;++k) reference+=matrix[i][k]*input[k][p];
                double error=fabs(transform.data[i]/512.0-reference);
                if(error>transform_peak) transform_peak=error;
                assert(error<0.25);
            }
            assert(transform.before[0]==0x5a5a5a5a && transform.before[1]==0x5a5a5a5a);
            assert(transform.after[0]==0x5a5a5a5a && transform.after[1]==0x5a5a5a5a);
        }
    }
#endif
    for(unsigned i=0;i<1024;++i) {
        window[i]=plm_audio_window_s32_table[i&511];
        float_window[i]=PLM_AUDIO_SYNTHESIS_WINDOW[i&511];
    }
    for(unsigned iteration=0;iteration<36;++iteration) {
        for(unsigned i=0;i<1024;++i) {
            history[i]=iteration==0 ? 0 : iteration==1 ? INT32_MAX :
                iteration==2 ? -INT32_MAX : iteration==3 ? (i&1 ? INT32_MAX : -INT32_MAX) :
                (int32_t)random_word()/2;
            float_history[i]=history[i]/512.0f;
        }
        for(unsigned position=0;position<1024;position+=64) {
            memset(&output,0x5a,sizeof(output));
            plm_audio_window_s32(output.data,window,history,position);
#ifndef PLM_AUDIO_TEST_QUICK
            for(unsigned i=0;i<32;++i) {
                double sum=0;
                for(unsigned half=0;half<2;++half) {
                    unsigned v=(position&127)/2;
                    if(half) v=96-v;
                    unsigned d=512-position/2+half*32;
                    for(unsigned tap=0;tap<8;++tap)
                        sum += (double)PLM_AUDIO_SYNTHESIS_WINDOW[(d+tap*64+i)&511]*
                               history[v+tap*128+i]/512.0;
                }
                double reference=clipped(sum*(-2147483648.0/1090519040.0));
                double error=fabs(output.data[i]-reference);
                if(error>window_peak) window_peak=error;
                assert(error<4096);
            }
#endif
            assert(output.before[0]==0x5a5a5a5a && output.before[1]==0x5a5a5a5a);
            assert(output.after[0]==0x5a5a5a5a && output.after[1]==0x5a5a5a5a);
#ifdef PLM_AUDIO_WINDOW_S32
            struct { uint32_t before[2]; int32_t data[32]; uint32_t after[2]; } actual;
            memset(&actual,0x5a,sizeof(actual));
            PLM_AUDIO_WINDOW_S32(actual.data,window,history,position);
            assert(!memcmp(actual.data,output.data,sizeof(actual.data)));
            assert(actual.before[0]==0x5a5a5a5a && actual.before[1]==0x5a5a5a5a);
            assert(actual.after[0]==0x5a5a5a5a && actual.after[1]==0x5a5a5a5a);
#endif
        }
    }
#ifdef PLM_AUDIO_TEST_QUICK
    puts("PASS: S32 scalar/vector window, extreme/random history, all phases and canaries (quick)");
#else
    printf("PASS: S32 transform peak=%g subband units; window peak=%g PCM LSB; all phases and canaries\n",
           transform_peak,window_peak);
#endif
#ifdef __xcore__
    const unsigned count=64;
    /* Same coefficient input, one idle core, stock 600-MHz target; 100-MHz
     * reference timer. Volatile sinks prevent dead-code removal. */
    unsigned start=get_reference_time();
    for(unsigned i=0;i<count;++i) {
        plm_audio_idct36(input,i%3,float_history,0);
        float_observed=float_history[i&63];
    }
    unsigned transform_float=get_reference_time()-start;
    start=get_reference_time();
    for(unsigned i=0;i<count;++i) {
        plm_audio_idct36_s32(input,i%3,history,0);
        observed=history[i&63];
    }
    unsigned transform_s32=get_reference_time()-start;
    printf("Transform ticks/%u: float=%u s32=%u\n",count,transform_float,transform_s32);
    start=get_reference_time();
    for(unsigned i=0;i<count;++i) {
        plm_audio_synthesis_window(float_output,float_window,float_history,(i&15)*64);
        float_observed=float_output[i&31];
    }
    unsigned floating=get_reference_time()-start;
    start=get_reference_time();
    for(unsigned i=0;i<count;++i) {
        plm_audio_window_s32(output.data,window,history,(i&15)*64);
        observed=output.data[i&31];
    }
    unsigned scalar=get_reference_time()-start;
#ifdef PLM_AUDIO_WINDOW_S32
    start=get_reference_time();
    for(unsigned i=0;i<count;++i) {
        PLM_AUDIO_WINDOW_S32(output.data,window,history,(i&15)*64);
        observed=output.data[i&31];
    }
    printf("Window ticks/%u: float-fused=%u s32-scalar=%u s32-vector=%u\n",
           count,floating,scalar,(unsigned)(get_reference_time()-start));
#endif
#endif
    return 0;
}
