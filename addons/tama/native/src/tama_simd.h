#pragma once
#include <cstddef>
#include <cstdint>

// ---------------------------------------------------------------------------
// Compile-time SIMD width selection.
// Build with -mavx2 -mfma (GCC/Clang) for the 8-wide path.
// SSE2 is the x86-64 baseline and requires no extra flag.
// ---------------------------------------------------------------------------

#if defined(__AVX2__)
#  include <immintrin.h>   // AVX2 + FMA
#  define TAMA_SIMD_WIDTH 8
#elif defined(__SSE2__)
#  include <emmintrin.h>   // SSE2
#  define TAMA_SIMD_WIDTH 4
#else
#  define TAMA_SIMD_WIDTH 1
#endif

// ---------------------------------------------------------------------------
// Aligned allocators — 32-byte for AVX2, 16-byte for SSE2, plain new[] otherwise.
// Use tama_simd_alloc/tama_simd_free as a matched pair.
// ---------------------------------------------------------------------------

inline float *tama_simd_alloc(int n) {
#if defined(__AVX2__)
    return static_cast<float *>(_mm_malloc(static_cast<size_t>(n) * sizeof(float), 32));
#elif defined(__SSE2__)
    return static_cast<float *>(_mm_malloc(static_cast<size_t>(n) * sizeof(float), 16));
#else
    return new float[n]();
#endif
}

inline void tama_simd_free(float *p) {
#if defined(__AVX2__) || defined(__SSE2__)
    _mm_free(p);
#else
    delete[] p;
#endif
}

// ---------------------------------------------------------------------------
// tama_pos_update — vectorised position integration.
//
//   pos_x[i] += vel_x[i] * delta
//   pos_y[i] += vel_y[i] * delta
//
// Only applied where simd_active[i] != 0. Inactive lanes are left untouched.
// All four float arrays must be TAMA_SIMD_WIDTH-float aligned (guaranteed by
// tama_simd_alloc). simd_active[] only needs byte alignment.
// ---------------------------------------------------------------------------

inline void tama_pos_update(
        float          * __restrict__ pos_x,
        float          * __restrict__ pos_y,
        const float    * __restrict__ vel_x,
        const float    * __restrict__ vel_y,
        const uint8_t  * __restrict__ simd_active,
        int   count,
        float delta)
{
#if defined(__AVX2__)
    const __m256 vd    = _mm256_set1_ps(delta);
    const __m256i zero = _mm256_setzero_si256();
    int i = 0;
    for (; i + 8 <= count; i += 8) {
        // Expand 8 active bytes → 8 × int32 → float bitmask
        __m128i a8   = _mm_loadl_epi64(reinterpret_cast<const __m128i *>(simd_active + i));
        __m256i a32  = _mm256_cvtepu8_epi32(a8);
        __m256  mask = _mm256_castsi256_ps(_mm256_cmpgt_epi32(a32, zero));

        __m256 px = _mm256_load_ps(pos_x + i);
        __m256 py = _mm256_load_ps(pos_y + i);
        __m256 vx = _mm256_load_ps(vel_x + i);
        __m256 vy = _mm256_load_ps(vel_y + i);

        // FMA: px = px + vx*delta  (blended — inactive lanes keep original px)
        __m256 new_px = _mm256_fmadd_ps(vx, vd, px);
        __m256 new_py = _mm256_fmadd_ps(vy, vd, py);
        _mm256_store_ps(pos_x + i, _mm256_blendv_ps(px, new_px, mask));
        _mm256_store_ps(pos_y + i, _mm256_blendv_ps(py, new_py, mask));
    }
    // Scalar tail (at most 7 elements)
    for (; i < count; ++i) {
        if (simd_active[i]) {
            pos_x[i] += vel_x[i] * delta;
            pos_y[i] += vel_y[i] * delta;
        }
    }

#elif defined(__SSE2__)
    const __m128 vd = _mm_set1_ps(delta);
    int i = 0;
    for (; i + 4 <= count; i += 4) {
        // Build per-lane mask from active bytes (0x00→0, else→all-ones)
        __m128i mask = _mm_set_epi32(
            simd_active[i+3] ? -1 : 0,
            simd_active[i+2] ? -1 : 0,
            simd_active[i+1] ? -1 : 0,
            simd_active[i  ] ? -1 : 0);
        __m128 fmask = _mm_castsi128_ps(mask);

        __m128 px = _mm_load_ps(pos_x + i);
        __m128 py = _mm_load_ps(pos_y + i);
        __m128 vx = _mm_load_ps(vel_x + i);
        __m128 vy = _mm_load_ps(vel_y + i);

        __m128 new_px = _mm_add_ps(px, _mm_mul_ps(vx, vd));
        __m128 new_py = _mm_add_ps(py, _mm_mul_ps(vy, vd));
        // SSE2 has no blendv — emulate with and/andnot/or
        _mm_store_ps(pos_x + i, _mm_or_ps(_mm_and_ps(fmask, new_px),
                                           _mm_andnot_ps(fmask, px)));
        _mm_store_ps(pos_y + i, _mm_or_ps(_mm_and_ps(fmask, new_py),
                                           _mm_andnot_ps(fmask, py)));
    }
    // Scalar tail
    for (; i < count; ++i) {
        if (simd_active[i]) {
            pos_x[i] += vel_x[i] * delta;
            pos_y[i] += vel_y[i] * delta;
        }
    }

#else
    // Portable scalar fallback — compiled when no SSE2/AVX2 is available.
    for (int i = 0; i < count; ++i) {
        if (simd_active[i]) {
            pos_x[i] += vel_x[i] * delta;
            pos_y[i] += vel_y[i] * delta;
        }
    }
#endif
}
