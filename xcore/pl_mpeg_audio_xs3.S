#if defined(__XS3A__)
#include "../pl_mpeg_audio_s32_tables.h"
.text
.issue_mode dual
.align 4
.globl plm_audio_window_s32_xs3
.type plm_audio_window_s32_xs3,@function
.cc_top plm_audio_window_s32_xs3.func,plm_audio_window_s32_xs3
/* output, doubled scaled window, circular history, position (multiple of 64).
 * Four groups of eight outputs, 16 Q30 products per lane. All vector reads
 * are word aligned and in bounds; no persistent transposition or extra history.
 * VLSAT(0) then VLASHR(-7) matches the portable final symmetric S32 saturation. */
plm_audio_window_s32_xs3:
    dualentsp 32
    std r4, r5, sp[0]
    std r6, r7, sp[1]
    std r8, r9, sp[2]
    std r10, r11, sp[3]
    ldc r11, 0
    vsetc r11
    vclrdr
    ldaw r11, sp[20]
    vstr r11[0]
    ldc r4, 2048
    shl r5, r3, 1
    sub r4, r4, r5
    add r1, r1, r4
    zext r3, 7
    shl r3, r3, 1
    add r8, r2, r3
    ldc r4, 384
    sub r4, r4, r3
    add r9, r2, r4
    ldc r2, 512
    ldc r6, 256
    ldc r3, PLM_AUDIO_WINDOW_LEFT
    neg r3, r3
    ldc r10, 4
.L_audio_window_group:
    vclrdr
    mov r4, r1
    mov r5, r8
    ldc r7, 8
.L_audio_window_first:
    vldc r5[0]
    vlmacc r4[0]
    add r5, r5, r2
    add r4, r4, r6
    sub r7, r7, 1
    bt r7, .L_audio_window_first
    ldc r11, 128
    add r4, r1, r11
    mov r5, r9
    ldc r7, 8
.L_audio_window_second:
    vldc r5[0]
    vlmacc r4[0]
    add r5, r5, r2
    add r4, r4, r6
    sub r7, r7, 1
    bt r7, .L_audio_window_second
    ldaw r11, sp[20]
    vlsat r11[0]
    ldaw r11, sp[12]
    vstr r11[0]
    vlashr r11[0], r3
    vstr r0[0]
    ldc r11, 32
    add r0, r0, r11
    add r1, r1, r11
    add r8, r8, r11
    add r9, r9, r11
    sub r10, r10, 1
    bt r10, .L_audio_window_group
    ldd r4, r5, sp[0]
    ldd r6, r7, sp[1]
    ldd r8, r9, sp[2]
    ldd r10, r11, sp[3]
    retsp 32
.cc_bottom plm_audio_window_s32_xs3.func
.globl plm_audio_window_s32_xs3.nstackwords
.globl plm_audio_window_s32_xs3.maxcores
.globl plm_audio_window_s32_xs3.maxtimers
.globl plm_audio_window_s32_xs3.maxchanends
.set plm_audio_window_s32_xs3.nstackwords,32
.set plm_audio_window_s32_xs3.maxcores,1
.set plm_audio_window_s32_xs3.maxtimers,0
.set plm_audio_window_s32_xs3.maxchanends,0
#endif
