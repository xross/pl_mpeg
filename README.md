# PL_MPEG - MPEG1 Video decoder, MP2 Audio decoder, MPEG-PS demuxer

Single-file MIT licensed library for C/C++

See [pl_mpeg.h](pl_mpeg.h) for the documentation.


## This fork

This is a fork of [phoboslab/pl_mpeg](https://github.com/phoboslab/pl_mpeg) with
decoder optimisations, aimed at small embedded targets and at XMOS xcore in
particular. The API is unchanged.

- Faster portable code, always on: DCT coefficients are decoded from a 32 bit
window with an 8 bit code table, the inverse DCT skips columns and rows that
only have a first coefficient, motion compensation and block output work four
pixels at a time, and start codes are found by scanning the buffered data.
- Optional decoder backends, off by default; see
[Build options](#build-options).
- Platform hooks: a platform header can replace word access, block copies, pixel
stores, the inverse DCT pass and the audio accumulation. See the comments at
each hook in `pl_mpeg.h`.
- An xcore platform header in [xcore/](xcore/), included automatically on xcore
targets, with aligned word access, an XS3 vector unit inverse DCT and block
copy, and an FMACC synthesis window.
- Two fixes: the last reference frame is returned when a stream ends with a B
frame, and the end of a buffer is registered when it is signalled while empty.

With the default options the library is still a single file. The optional
backends add `pl_mpeg_idct.h` and `pl_mpeg_audio.h`, and xcore targets add the
`xcore/` directory; keep these next to `pl_mpeg.h`.


## Why?

This is meant as a simple way to get video playback into your app or game. Other
solutions, such as ffmpeg require huge libraries and a lot of glue code.

MPEG1 is an old and inefficient codec, but it's still good enough for many use
cases. All patents related to MPEG1 and MP2 have expired, so it's completely
free now.

The portable code does not make use of any SIMD instructions, but because of
the relative simplicity of the codec it still manages to decode 4k60fps video
on a single CPU core (on my i7-6700k at least). The only vector code is the
optional xcore XS3 specialisation.


## Building

A Makefile to build the example applications is included. 

- `pl_mpeg_extract_frames`: a command line tool that dumps all frames into BMPs
- `pl_mpeg_player_gl`: a video player using SDL2 and OpenGL
- `pl_mpeg_player_sdl`: a video player using SDL2 and it's built-in 2d renderer

The players require the SDL2 library to be installed.

```shell
make # build all
make extract # only build pl_mpeg_extract_frames
make player_gl # only build pl_mpeg_player_gl
make player_sdl # only build pl_mpeg_player_sdl
```

Tests for the optional inverse DCT and synthesis window are in `tests/`:

```shell
cmake -S tests -B build-tests
cmake --build build-tests
ctest --test-dir build-tests
```


## Build options

Define these before including `pl_mpeg.h` with `PL_MPEG_IMPLEMENTATION`. The
defaults keep the original arithmetic.

`PLM_VIDEO_IDCT` selects the inverse DCT:

- `PLM_VIDEO_IDCT_ORIGINAL` (default): the original 32 bit transform.
- `PLM_VIDEO_IDCT_SCALAR16`: an orthonormal Q14 transform on 16 bit
coefficients, with different rounding. This is the portable model for vector
implementations; it is slower than the original and not an optimisation.
- `PLM_VIDEO_IDCT_VECTOR`: a platform vector implementation, bit-identical to
`SCALAR16`. It is a compile error where the platform provides none.
- `PLM_VIDEO_IDCT_NATIVE`: `VECTOR` where there is one, `ORIGINAL` elsewhere.

`PLM_AUDIO_FAST_SCALE` and `PLM_AUDIO_FAST_WINDOW` (both default `0`) change
only floating-point rounding in the MP2 decoder. `FAST_SCALE` scales PCM with a
reciprocal. `FAST_WINDOW` uses fused accumulation in the synthesis window; it
requires C99 `fmaf` (link libm where the platform requires it), unless a
platform header provides `PLM_AUDIO_ACCUMULATE`.

`PLM_AUDIO_FORMAT` defaults to `PLM_AUDIO_FORMAT_FLOAT` (`0`). Select
`PLM_AUDIO_FORMAT_S32` (`1`) for full-scale signed 32-bit PCM. The public
`plm_audio_sample_t` and the sample/history arrays then become `int32_t`;
define the format consistently in every translation unit that uses this API.
Both interleaved and `PLM_AUDIO_SEPARATE_CHANNELS` layouts are supported.
PCM clips symmetrically to `[-INT32_MAX, INT32_MAX]`, matching XS3 VPU arithmetic,
not wrapping. Float remains unclipped and unchanged. The float fast switches
have no effect on S32.

Integer synthesis uses Q27 butterfly coefficients and Q9 history. The generator
`tools/audio_s32_generate.py --check` verifies the checked-in headers and proves
an affine/rounding bound for **every** intermediate with subband input magnitude
at most 65536. Nine is the largest safe number of history fractional bits.
A valid stream stays within that magnitude; an invalid grouped sample code could
reach 104848, so each grouped code is limited to the largest valid one.
Scaled window coefficients, rounded Q30 products, an accumulator that sixteen
products cannot saturate
and a saturating seven-bit left shift produce Q1.31 PCM. No extra persistent
history buffer or external math library is required. Arithmetic right shift of
negative signed integers is required, as on the tested C99 host/xcore compilers.

`PLM_AUDIO_SYNTHESIS_SCALAR` (`0`) selects the standalone portable model.
`PLM_AUDIO_SYNTHESIS_VECTOR` (`1`) selects a platform `PLM_AUDIO_WINDOW_S32` hook
and is a compile-time error when none exists. `PLM_AUDIO_SYNTHESIS_NATIVE`
(the default) selects the hook when present and scalar otherwise. The transform
remains scalar in both cases; only the 32-output synthesis window is vectorised.
The portable 64-bit arithmetic is a correctness model, not a speed claim.

The native tests compare MP2 output with pristine upstream float (peak error
at most `2^-20` full scale and SNR at least 100 dB), and check the kernels against
independent double/cosine references. They cover extreme/random inputs, history
phases, saturation, canaries, short refills, failures and replay. On the full
5042-frame reference soundtrack, measured S32 peak error was `7.82e-7` and SNR
117.41 dB. Deliberately over-range output is tested for clipping separately;
an unclipped float output above full scale cannot meet that error bound.

### xcore

`xcore/pl_mpeg_xcore.h` is included whenever `__xcore__` is defined. Its hooks
give the same decoded output as the portable code they replace.

- `PLM_MALLOC` must return memory aligned to at least four bytes.
- `PLM_VIDEO_IDCT_VECTOR` is XS3 only; build `xcore/pl_mpeg_idct_xs3.S`
alongside the implementation.
- On XS3, `PLM_AUDIO_FAST_WINDOW` uses the FMACC instruction and does not need
`fmaf`.
- S32 vector synthesis requires `xcore/pl_mpeg_audio_xs3.S`. Its eight-lane Q30
  window is bit-identical to the portable model. Inputs/output must be at least
  word aligned; the decoder's existing doubled window and history are reused.


## Encoding for PL_MPEG

Most [MPEG-PS](https://en.wikipedia.org/wiki/MPEG_program_stream) (`.mpg`) files
containing MPEG1 Video ("mpeg1") and MPEG1 Audio Layer II ("mp2") streams should
work with PL_MPEG. Note that `.mpg` files can also contain MPEG2 Video, which is
not supported by this library.

You can encode video in a suitable format using ffmpeg:

```
ffmpeg -i input.mp4 -c:v mpeg1video -q:v 0 -c:a libtwolame -b:a 224k -format mpeg output.mpg
```

`-q:v` sets a fixed video quality with a variable bitrate, where `0` is the 
highest. You may use `-b:v` to set a fixed bitrate instead; e.g. 
`-b:v 2000k` for 2000 kbit/s. Please refer to the 
[ffmpeg documentation](http://ffmpeg.org/ffmpeg.html#Options) for more details.

If you just want to quickly test the library, try this file:

https://phoboslab.org/files/bjork-all-is-full-of-love.mpg


## Limitations

- no error reporting. PL_MPEG will silently ignore any invalid data.
- the pts (presentation time stamp) for packets in the MPEG-PS container is
ignored. This may cause sync issues with some files.
- bugs, probably.
