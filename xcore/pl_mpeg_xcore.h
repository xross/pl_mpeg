#ifndef PL_MPEG_XCORE_H
#define PL_MPEG_XCORE_H

/* XMOS xcore specialisations of pl_mpeg.h, which includes this header on
 * xcore targets. Everything xcore-specific is in this directory; pl_mpeg.h
 * only names the hooks defined here. Decoded output is the same as that of
 * the portable code each hook replaces.
 *
 * Requirements on the application:
 * - PLM_MALLOC must return memory aligned to at least four bytes, so that
 *   frames, and therefore macroblock positions in them, are word aligned.
 * - With PLM_VIDEO_IDCT_VECTOR (XS3 only), build pl_mpeg_idct_xs3.S alongside
 *   the implementation. */

#include <stdint.h>

/* ---- Word access for motion compensation and block output ---------------
 * xcore traps on unaligned word access and its compiler turns a memcpy of
 * unknown alignment into a library call per word, so use aligned words
 * directly. xcore is little-endian. */
#define PLM_WORD_ACCESS
typedef uint32_t __attribute__((may_alias)) plm_word_t;

#define PLM_ALIGNED_WORD_ACCESS
#define PLM_WORD_LITTLE_ENDIAN 1
/* Retain the existing padded-word lookahead contract on xcore. */
#define PLM_WORD_LOOKAHEAD 1

static inline const uint8_t *plm_word_begin(const uint8_t *p, unsigned *position)
{
    *position = ((uintptr_t)p & 3) << 3;
    return (const uint8_t *)((uintptr_t)p & ~(uintptr_t)3);
}

static inline uint32_t plm_load_motion_word(const uint8_t *p)
{
    return *(const plm_word_t *)p;
}

static inline uint32_t plm_extract_word(uint32_t high, uint32_t low, unsigned position)
{
    uint32_t word;
    __asm__("lextract %0, %1, %2, %3, 32" : "=r"(word) : "r"(high), "r"(low), "r"(position));
    return word;
}

/* Build the word from the two aligned words that contain it, with one
 * LEXTRACT (operands: high word, low word, bit position). The second aligned
 * load retains the existing padded-word lookahead contract. */
static inline uint32_t plm_load_word(const uint8_t *p)
{
    unsigned position;
    const uint8_t *base = plm_word_begin(p, &position);
    return plm_extract_word(plm_load_motion_word(base + 4), plm_load_motion_word(base), position);
}

/* Words are only stored to macroblock positions in a frame, which are word
 * aligned whenever the frame allocation is. No check is made per word; a
 * misaligned frame traps on the first store. */
static inline void plm_store_word(uint8_t *p, uint32_t word)
{
    *(plm_word_t *)p = word;
}

/* A position that is stored to is aligned, so it needs no LEXTRACT. */
static inline uint32_t plm_load_stored_word(const uint8_t *p)
{
    return *(const plm_word_t *)p;
}

/* Write the four pixels of a decoded block row as one word instead of four
 * byte stores. */
#define PLM_STORE_PIXELS(DEST, V0, V1, V2, V3) \
    plm_store_word((DEST), (uint32_t)(V0) | ((uint32_t)(V1) << 8) | \
        ((uint32_t)(V2) << 16) | ((uint32_t)(V3) << 24))

#ifdef __XS3A__

/* ---- Full-pixel motion prediction ---------------------------------------
 * A full-pixel prediction from a word aligned source is a plain copy. Load 32
 * bytes into a vector register and store only the first block_size of them,
 * one row at a time. The load reads up to 24 bytes beyond the row; they must
 * be readable memory, and are never stored. VLDR only takes its address in
 * r11. Declines an unaligned source. */
static inline int plm_xs3_copy_block(const uint8_t *source, uint8_t *destination,
                                     int stride, int block_size)
{
    if ((uintptr_t)source & 3) return 0;
    unsigned mask = block_size == 16 ? 0xffff : 0xff; /* One bit per byte */
    for (int y = 0; y < block_size; y++, source += stride, destination += stride) {
        __asm__ volatile("mov r11, %0\n\tvldr r11[0]\n\tvstrpv %1[0], %2"
                         : : "r"(source), "r"(destination), "r"(mask) : "r11", "memory");
    }
    return 1;
}
#define PLM_COPY_BLOCK(SOURCE, DEST, STRIDE, SIZE) plm_xs3_copy_block(SOURCE, DEST, STRIDE, SIZE)

/* ---- 16-bit inverse DCT on the vector unit ------------------------------
 * One pass is 64 eight-term dot products (VLMACCR) and a rounding, saturating
 * shift (VLSAT), in pl_mpeg_idct_xs3.S. Vector loads need word alignment. */
#define PLM_VIDEO_BLOCK_ALIGNED __attribute__((aligned(4)))
#define PLM_IDCT_ALIGNED __attribute__((aligned(8)))
#ifdef __cplusplus
extern "C" {
#endif
/* Output, padded input, Q14 matrix, 16 shifts. */
void plm_idct_pass_xs3(int16_t *, const int16_t *, const int16_t *, const uint16_t *);
#ifdef __cplusplus
}
#endif
#ifdef PL_MPEG_IMPLEMENTATION
static const uint16_t plm_idct_shift13[16] PLM_IDCT_ALIGNED = {
    13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13};
static const uint16_t plm_idct_shift17[16] PLM_IDCT_ALIGNED = {
    17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17};
#endif
/* SHIFT is 13 for the first pass and 17 for the second. */
#define PLM_IDCT_VECTOR_PASS(OUTPUT, INPUT, SHIFT) \
    plm_idct_pass_xs3((OUTPUT), (INPUT), &plm_idct_matrix[0][0], plm_idct_shift##SHIFT)

/* ---- MP2 synthesis window ------------------------------------------------
 * Fused multiply-accumulate is one instruction. The kernel is kept out of
 * line, which measured faster than inlining it into the frame decoder. */
static inline float plm_xs3_fmacc(float sum, float a, float b)
{
    __asm__("fmacc %0, %0, %1, %2" : "+r"(sum) : "r"(a), "r"(b));
    return sum;
}
#define PLM_AUDIO_ACCUMULATE(SUM, A, B) plm_xs3_fmacc(SUM, A, B)
#define PLM_AUDIO_WINDOW_ATTRIBUTES __attribute__((noinline))

#endif /* __XS3A__ */
#endif
