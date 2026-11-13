#if defined(__XS3A__)
.text
.issue_mode dual
.align 4
.globl plm_idct_pass_xs3
.type plm_idct_pass_xs3,@function
.cc_top plm_idct_pass_xs3.func,plm_idct_pass_xs3
/* Four pairs of output rows. Reverse input-row order compensates the VPU
 * rotating accumulator order, transposing each pass without a separate copy.
 * Parameters: output, padded input, Q14 matrix, 16 uint16 shifts. */
plm_idct_pass_xs3:
    dualentsp 4
    std r4, r5, sp[0]
    std r6, r7, sp[1]
    ldc r11, 0x100
    vsetc r11
    ldc r4, 28
    ldaw r1, r1[r4]
    ldc r5, 16
    ldc r6, 4
    ldc r7, 32
.L_idct_pair:
    vclrdr
    add r11, r2, r7
    vldc r11[0]
    mov r4, r1
    { vlmaccr r4[0]; sub r4, r4, r5 }
    { vlmaccr r4[0]; sub r4, r4, r5 }
    { vlmaccr r4[0]; sub r4, r4, r5 }
    { vlmaccr r4[0]; sub r4, r4, r5 }
    { vlmaccr r4[0]; sub r4, r4, r5 }
    { vlmaccr r4[0]; sub r4, r4, r5 }
    { vlmaccr r4[0]; sub r4, r4, r5 }
    vlmaccr r4[0]
    mov r11, r2
    vldc r11[0]
    mov r4, r1
    { vlmaccr r4[0]; sub r4, r4, r5 }
    { vlmaccr r4[0]; sub r4, r4, r5 }
    { vlmaccr r4[0]; sub r4, r4, r5 }
    { vlmaccr r4[0]; sub r4, r4, r5 }
    { vlmaccr r4[0]; sub r4, r4, r5 }
    { vlmaccr r4[0]; sub r4, r4, r5 }
    { vlmaccr r4[0]; sub r4, r4, r5 }
    vlmaccr r4[0]
    vlsat r3[0]
    vstr r0[0]
    add r0, r0, r7
    add r2, r2, r7
    add r2, r2, r7
    sub r6, r6, 1
    bt r6, .L_idct_pair
    ldd r4, r5, sp[0]
    ldd r6, r7, sp[1]
    retsp 4
.cc_bottom plm_idct_pass_xs3.func
.globl plm_idct_pass_xs3.nstackwords
.globl plm_idct_pass_xs3.maxcores
.globl plm_idct_pass_xs3.maxtimers
.globl plm_idct_pass_xs3.maxchanends
.set plm_idct_pass_xs3.nstackwords,4
.set plm_idct_pass_xs3.maxcores,1
.set plm_idct_pass_xs3.maxtimers,0
.set plm_idct_pass_xs3.maxchanends,0
#endif
