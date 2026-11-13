#ifndef PL_MPEG_IDCT_H
#define PL_MPEG_IDCT_H

/* Implementation detail included by pl_mpeg.h for the 16-bit backends.
 * Natural-order MPEG coefficients must be in [-2048,2047], without the legacy
 * premultiplier. Q14 shifts 13/17 bound the intermediate magnitude to 21642
 * and the 32-bit accumulation to 1873483014. No frame-sized scratch storage.
 * The scalar passes are the model a vector implementation must match bit for
 * bit. A platform header may define PLM_IDCT_ALIGNED as the alignment
 * attribute its vector implementation needs. */
#ifndef PLM_IDCT_ALIGNED
#define PLM_IDCT_ALIGNED
#endif

/* round(2^14 * C(u) * cos((2*x+1)*u*pi/16)), C(0)=1/sqrt(2).
 * Zero padding lets a 16-lane vector dot product be an eight-term transform. */
static const int16_t plm_idct_matrix[8][16] PLM_IDCT_ALIGNED = {
    {11585,16069,15137,13623,11585,9102,6270,3196,0,0,0,0,0,0,0,0},
    {11585,13623,6270,-3196,-11585,-16069,-15137,-9102,0,0,0,0,0,0,0,0},
    {11585,9102,-6270,-16069,-11585,3196,15137,13623,0,0,0,0,0,0,0,0},
    {11585,3196,-15137,-9102,11585,13623,-6270,-16069,0,0,0,0,0,0,0,0},
    {11585,-3196,-15137,9102,11585,-13623,-6270,16069,0,0,0,0,0,0,0,0},
    {11585,-9102,-6270,16069,-11585,-3196,15137,-13623,0,0,0,0,0,0,0,0},
    {11585,-13623,6270,3196,-11585,16069,-15137,9102,0,0,0,0,0,0,0,0},
    {11585,-16069,15137,-13623,11585,-9102,6270,-3196,0,0,0,0,0,0,0,0}
};

static inline void plm_idct_scalar_pass(int16_t *output, const int16_t *input, unsigned shift)
{
    for (unsigned row = 0; row < 8; ++row) {
        for (unsigned x = 0; x < 8; ++x) {
            int32_t sum = 0;
            for (unsigned u = 0; u < 8; ++u)
                sum += input[row * 8 + u] * plm_idct_matrix[x][u];
            output[x * 8 + row] = (int16_t)((sum + (1 << (shift - 1))) >> shift);
        }
    }
}

/* In place on the first 64 of PLM_VIDEO_BLOCK_SIZE (72) coefficients. A vector
 * pass may load 16 lanes per row, so the last row reads eight elements past
 * the 64 coefficients. Those lanes meet zero matrix entries: they must be
 * readable but their values, here and in the scratch, do not affect the
 * result. PLM_IDCT_VECTOR_PASS(output, input, shift) is one such pass, with
 * the same result as plm_idct_scalar_pass(). */
void plm_video_idct(plm_video_coeff_t *block)
{
    int16_t temporary[PLM_VIDEO_BLOCK_SIZE] PLM_IDCT_ALIGNED;
#if PLM_VIDEO_IDCT == PLM_VIDEO_IDCT_VECTOR
    PLM_IDCT_VECTOR_PASS(temporary, block, 13);
    PLM_IDCT_VECTOR_PASS(block, temporary, 17);
#else
    plm_idct_scalar_pass(temporary, block, 13);
    plm_idct_scalar_pass(block, temporary, 17);
#endif
}
#undef PLM_IDCT_ALIGNED
#endif
