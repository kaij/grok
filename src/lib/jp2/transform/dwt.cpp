/*
 *    Copyright (C) 2016-2020 Grok Image Compression Inc.
 *
 *    This source code is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This source code is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *    This source code incorporates work covered by the following copyright and
 *    permission notice:
 *
 * The copyright in this software is being made available under the 2-clauses
 * BSD License, included below. This software may be subject to other third
 * party and contributor rights, including patent rights, and no such rights
 * are granted under this license.
 *
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2001-2003, David Janssens
 * Copyright (c) 2002-2003, Yannick Verschueren
 * Copyright (c) 2003-2007, Francois-Olivier Devaux
 * Copyright (c) 2003-2014, Antonin Descampe
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
 * Copyright (c) 2007, Jonathan Ballard <dzonatas@dzonux.net>
 * Copyright (c) 2007, Callum Lerwick <seg@haxxed.com>
 * Copyright (c) 2017, IntoPIX SA <support@intopix.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include "simd.h"
#include "grok_includes.h"

#include "sparse_array.h"
#include "dwt.h"
#include <algorithm>

using namespace std;

namespace grk {


#define GRK_WS(i) v->mem[(i)*2]
#define GRK_WD(i) v->mem[(1+(i)*2)]

/** Number of columns that we can process in parallel in the vertical pass */
#define PLL_COLS_53     (2*VREG_INT_COUNT)

/** @name Local data structures */
/*@{*/

struct dwt_data_53 {
    int32_t* mem;
    int32_t dn;   /* number of elements in high pass band */
    int32_t sn;   /* number of elements in low pass band */
    int32_t cas;  /* 0 = start on even coord, 1 = start on odd coord */
} ;

template <typename T> struct decode_job{
	decode_job( T data,
					uint32_t w,
					int32_t * restrict tiledp,
					uint32_t min_j,
					uint32_t max_j) : data(data),
									w(w),
									tiledp(tiledp),
									min_j(min_j),
									max_j(max_j)
	{}

    T data;
    uint32_t w;
    int32_t * restrict tiledp;
    uint32_t min_j;
    uint32_t max_j;
} ;


typedef union {
    float f[4];
} v4_data;

struct dwt_data_97 {
    v4_data*   wavelet ;
    int32_t       dn ;  /* number of elements in high pass band */
    int32_t       sn ;  /* number of elements in low pass band */
    int32_t       cas ; /* 0 = start on even coord, 1 = start on odd coord */
    uint32_t      win_l_x0; /* start coord in low pass band */
    uint32_t      win_l_x1; /* end coord in low pass band */
    uint32_t      win_h_x0; /* start coord in high pass band */
    uint32_t      win_h_x1; /* end coord in high pass band */
}  ;

static const float dwt_alpha =  1.586134342f; /*  12994 */
static const float dwt_beta  =  0.052980118f; /*    434 */
static const float dwt_gamma = -0.882911075f; /*  -7233 */
static const float dwt_delta = -0.443506852f; /*  -3633 */

static const float K      = 1.230174105f; /*  10078 */
static const float c13318 = 1.625732422f;

/*@}*/

/** @name Local static functions */
/*@{*/

/**
Inverse wavelet transform in 2-D.
*/
static bool decode_tile_53(TileComponent* tilec, uint32_t i);

static bool decode_partial_tile_53(
    TileComponent* tilec,
    uint32_t numres);


static uint32_t max_resolution(grk_tcd_resolution* restrict r,
        uint32_t i);

/* <summary>                             */
/* Inverse 9-7 wavelet transform in 1-D. */
/* </summary>                            */
static void decode_step_97(dwt_data_97* restrict dwt);

static void interleave_h_97(dwt_data_97* restrict dwt,
                                   float* restrict a,
                                   uint32_t width,
                                   uint32_t remaining_height);

static void interleave_v_97(dwt_data_97* restrict dwt,
                                   float* restrict a,
                                   uint32_t width,
                                   uint32_t nb_elts_read);

#ifdef __SSE__
static void decode_step1_sse_97(v4_data* w,
                                       uint32_t start,
                                       uint32_t end,
                                       const __m128 c);

static void decode_step2_sse_97(v4_data* l, v4_data* w,
                                       uint32_t start,
                                       uint32_t end,
                                       uint32_t m, __m128 c);

#else
static void decode_step1_97(v4_data* w,
                                   uint32_t start,
                                   uint32_t end,
                                   const float c);

static void decode_step2_97(v4_data* l, v4_data* w,
                                   uint32_t start,
                                   uint32_t end,
                                   uint32_t m,
                                   float c);

#endif

/*@}*/

/*@}*/

#define GRK_S(i) a[(i)*2]
#define GRK_D(i) a[(1+(i)*2)]
#define GRK_S_(i) ((i)<0?GRK_S(0):((i)>=sn?GRK_S(sn-1):GRK_S(i)))
#define GRK_D_(i) ((i)<0?GRK_D(0):((i)>=dn?GRK_D(dn-1):GRK_D(i)))
/* new */
#define GRK_SS_(i) ((i)<0?GRK_S(0):((i)>=dn?GRK_S(dn-1):GRK_S(i)))
#define GRK_DD_(i) ((i)<0?GRK_D(0):((i)>=sn?GRK_D(sn-1):GRK_D(i)))


/*
==========================================================
   local functions
==========================================================
*/

static void  decode_h_cas0_53(int32_t* tmp,
                               const int32_t sn,
                               const int32_t len,
                               int32_t* tiledp){
    int32_t i, j;
    const int32_t* in_even = &tiledp[0];
    const int32_t* in_odd = &tiledp[sn];

    int32_t d1c, d1n, s1n, s0c, s0n;

    assert(len > 1);

    /* Improved version of the TWO_PASS_VERSION: */
    /* Performs lifting in one single iteration. Saves memory */
    /* accesses and explicit interleaving. */
    s1n = in_even[0];
    d1n = in_odd[0];
    s0n = s1n - ((d1n + 1) >> 1);

    for (i = 0, j = 1; i < (len - 3); i += 2, j++) {
        d1c = d1n;
        s0c = s0n;

        s1n = in_even[j];
        d1n = in_odd[j];

        s0n = s1n - ((d1c + d1n + 2) >> 2);

        tmp[i  ] = s0c;
        tmp[i + 1] = d1c + ((s0c + s0n) >> 1);
    }

    tmp[i] = s0n;

    if (len & 1) {
        tmp[len - 1] = in_even[(len - 1) / 2] - ((d1n + 1) >> 1);
        tmp[len - 2] = d1n + ((s0n + tmp[len - 1]) >> 1);
    } else {
        tmp[len - 1] = d1n + s0n;
    }
    memcpy(tiledp, tmp, (uint32_t)len * sizeof(int32_t));
}

static void  decode_h_cas1_53(int32_t* tmp,
                               const int32_t sn,
                               const int32_t len,
                               int32_t* tiledp){
    int32_t i, j;
    const int32_t* in_even = &tiledp[sn];
    const int32_t* in_odd = &tiledp[0];
    int32_t s1, s2, dc, dn;

    assert(len > 2);

    /* Improved version of the TWO_PASS_VERSION:
       Performs lifting in one single iteration. Saves memory
       accesses and explicit interleaving. */

    s1 = in_even[1];
    dc = in_odd[0] - ((in_even[0] + s1 + 2) >> 2);
    tmp[0] = in_even[0] + dc;

    for (i = 1, j = 1; i < (len - 2 - !(len & 1)); i += 2, j++) {

        s2 = in_even[j + 1];

        dn = in_odd[j] - ((s1 + s2 + 2) >> 2);
        tmp[i  ] = dc;
        tmp[i + 1] = s1 + ((dn + dc) >> 1);

        dc = dn;
        s1 = s2;
    }

    tmp[i] = dc;

    if (!(len & 1)) {
        dn = in_odd[len / 2 - 1] - ((s1 + 1) >> 1);
        tmp[len - 2] = s1 + ((dn + dc) >> 1);
        tmp[len - 1] = dn;
    } else {
        tmp[len - 1] = s1 + dc;
    }
    memcpy(tiledp, tmp, (uint32_t)len * sizeof(int32_t));
}

/* <summary>                            */
/* Inverse 5-3 wavelet transform in 1-D for one row. */
/* </summary>                           */
/* Performs interleave, inverse wavelet transform and copy back to buffer */
static void decode_h_53(const dwt_data_53 *dwt,
                         int32_t* tiledp)
{
    const int32_t sn = dwt->sn;
    const int32_t len = sn + dwt->dn;
    if (dwt->cas == 0) { /* Left-most sample is on even coordinate */
        if (len > 1) {
            decode_h_cas0_53(dwt->mem, sn, len, tiledp);
        } else {
            /* Unmodified value */
        }
    } else { /* Left-most sample is on odd coordinate */
        if (len == 1) {
            tiledp[0] /= 2;
        } else if (len == 2) {
            int32_t* out = dwt->mem;
            const int32_t* in_even = &tiledp[sn];
            const int32_t* in_odd = &tiledp[0];
            out[1] = in_odd[0] - ((in_even[0] + 1) >> 1);
            out[0] = in_even[0] + out[1];
            memcpy(tiledp, dwt->mem, (uint32_t)len * sizeof(int32_t));
        } else if (len > 2) {
            decode_h_cas1_53(dwt->mem, sn, len, tiledp);
        }
    }
}

#if (defined(__SSE2__) || defined(__AVX2__))

#define ADD3(x,y,z) ADD(ADD(x,y),z)

static
void decode_v_final_memcpy_53(int32_t* tiledp_col,
                               const int32_t* tmp,
                               int32_t len,
                               size_t stride){
    int32_t i;
    for (i = 0; i < len; ++i) {
        /* A memcpy(&tiledp_col[i * stride + 0],
                    &tmp[PARALLEL_COLS_53 * i + 0],
                    PARALLEL_COLS_53 * sizeof(int32_t))
           would do but would be a tiny bit slower.
           We can take here advantage of our knowledge of alignment */
        STOREU(&tiledp_col[(size_t)i * stride + 0],
               LOAD(&tmp[PLL_COLS_53 * i + 0]));
        STOREU(&tiledp_col[(size_t)i * stride + VREG_INT_COUNT],
               LOAD(&tmp[PLL_COLS_53 * i + VREG_INT_COUNT]));
    }
}

/** Vertical inverse 5x3 wavelet transform for 8 columns in SSE2, or
 * 16 in AVX2, when top-most pixel is on even coordinate */
static void decode_v_cas0_mcols_SSE2_OR_AVX2_53(int32_t* tmp,
												const int32_t sn,
												const int32_t len,
												int32_t* tiledp_col,
												const size_t stride){
    const int32_t* in_even = &tiledp_col[0];
    const int32_t* in_odd = &tiledp_col[(size_t)sn * stride];

    int32_t i;
    size_t j;
    VREG d1c_0, d1n_0, s1n_0, s0c_0, s0n_0;
    VREG d1c_1, d1n_1, s1n_1, s0c_1, s0n_1;
    const VREG two = LOAD_CST(2);

    assert(len > 1);
#if __AVX2__
    assert(PLL_COLS_53 == 16);
    assert(VREG_INT_COUNT == 8);
#else
    assert(PLL_COLS_53 == 8);
    assert(VREG_INT_COUNT == 4);
#endif

    /* Note: loads of input even/odd values must be done in a unaligned */
    /* fashion. But stores in tmp can be done with aligned store, since */
    /* the temporary buffer is properly aligned */
    assert((size_t)tmp % (sizeof(int32_t) * VREG_INT_COUNT) == 0);

    s1n_0 = LOADU(in_even + 0);
    s1n_1 = LOADU(in_even + VREG_INT_COUNT);
    d1n_0 = LOADU(in_odd);
    d1n_1 = LOADU(in_odd + VREG_INT_COUNT);

    /* s0n = s1n - ((d1n + 1) >> 1); <==> */
    /* s0n = s1n - ((d1n + d1n + 2) >> 2); */
    s0n_0 = SUB(s1n_0, SAR(ADD3(d1n_0, d1n_0, two), 2));
    s0n_1 = SUB(s1n_1, SAR(ADD3(d1n_1, d1n_1, two), 2));

    for (i = 0, j = 1; i < (len - 3); i += 2, j++) {
        d1c_0 = d1n_0;
        s0c_0 = s0n_0;
        d1c_1 = d1n_1;
        s0c_1 = s0n_1;

        s1n_0 = LOADU(in_even + j * stride);
        s1n_1 = LOADU(in_even + j * stride + VREG_INT_COUNT);
        d1n_0 = LOADU(in_odd + j * stride);
        d1n_1 = LOADU(in_odd + j * stride + VREG_INT_COUNT);

        /*s0n = s1n - ((d1c + d1n + 2) >> 2);*/
        s0n_0 = SUB(s1n_0, SAR(ADD3(d1c_0, d1n_0, two), 2));
        s0n_1 = SUB(s1n_1, SAR(ADD3(d1c_1, d1n_1, two), 2));

        STORE(tmp + PLL_COLS_53 * (i + 0), s0c_0);
        STORE(tmp + PLL_COLS_53 * (i + 0) + VREG_INT_COUNT, s0c_1);

        /* d1c + ((s0c + s0n) >> 1) */
        STORE(tmp + PLL_COLS_53 * (i + 1) + 0,
              ADD(d1c_0, SAR(ADD(s0c_0, s0n_0), 1)));
        STORE(tmp + PLL_COLS_53 * (i + 1) + VREG_INT_COUNT,
              ADD(d1c_1, SAR(ADD(s0c_1, s0n_1), 1)));
    }

    STORE(tmp + PLL_COLS_53 * (i + 0) + 0, s0n_0);
    STORE(tmp + PLL_COLS_53 * (i + 0) + VREG_INT_COUNT, s0n_1);

    if (len & 1) {
        VREG tmp_len_minus_1;
        s1n_0 = LOADU(in_even + (size_t)((len - 1) / 2) * stride);
        /* tmp_len_minus_1 = s1n - ((d1n + 1) >> 1); */
        tmp_len_minus_1 = SUB(s1n_0, SAR(ADD3(d1n_0, d1n_0, two), 2));
        STORE(tmp + PLL_COLS_53 * (len - 1), tmp_len_minus_1);
        /* d1n + ((s0n + tmp_len_minus_1) >> 1) */
        STORE(tmp + PLL_COLS_53 * (len - 2),
              ADD(d1n_0, SAR(ADD(s0n_0, tmp_len_minus_1), 1)));

        s1n_1 = LOADU(in_even + (size_t)((len - 1) / 2) * stride + VREG_INT_COUNT);
        /* tmp_len_minus_1 = s1n - ((d1n + 1) >> 1); */
        tmp_len_minus_1 = SUB(s1n_1, SAR(ADD3(d1n_1, d1n_1, two), 2));
        STORE(tmp + PLL_COLS_53 * (len - 1) + VREG_INT_COUNT,
              tmp_len_minus_1);
        /* d1n + ((s0n + tmp_len_minus_1) >> 1) */
        STORE(tmp + PLL_COLS_53 * (len - 2) + VREG_INT_COUNT,
              ADD(d1n_1, SAR(ADD(s0n_1, tmp_len_minus_1), 1)));
    } else {
        STORE(tmp + PLL_COLS_53 * (len - 1) + 0,
              ADD(d1n_0, s0n_0));
        STORE(tmp + PLL_COLS_53 * (len - 1) + VREG_INT_COUNT,
              ADD(d1n_1, s0n_1));
    }
    decode_v_final_memcpy_53(tiledp_col, tmp, len, stride);
}


/** Vertical inverse 5x3 wavelet transform for 8 columns in SSE2, or
 * 16 in AVX2, when top-most pixel is on odd coordinate */
static void decode_v_cas1_mcols_SSE2_OR_AVX2_53(
    int32_t* tmp,
    const int32_t sn,
    const int32_t len,
    int32_t* tiledp_col,
    const size_t stride){
    int32_t i;
    size_t j;

    VREG s1_0, s2_0, dc_0, dn_0;
    VREG s1_1, s2_1, dc_1, dn_1;
    const VREG two = LOAD_CST(2);

    const int32_t* in_even = &tiledp_col[(size_t)sn * stride];
    const int32_t* in_odd = &tiledp_col[0];

    assert(len > 2);
#if __AVX2__
    assert(PLL_COLS_53 == 16);
    assert(VREG_INT_COUNT == 8);
#else
    assert(PLL_COLS_53 == 8);
    assert(VREG_INT_COUNT == 4);
#endif

    /* Note: loads of input even/odd values must be done in a unaligned */
    /* fashion. But stores in tmp can be done with aligned store, since */
    /* the temporary buffer is properly aligned */
    assert((size_t)tmp % (sizeof(int32_t) * VREG_INT_COUNT) == 0);

    s1_0 = LOADU(in_even + stride);
    /* in_odd[0] - ((in_even[0] + s1 + 2) >> 2); */
    dc_0 = SUB(LOADU(in_odd + 0),
               SAR(ADD3(LOADU(in_even + 0), s1_0, two), 2));
    STORE(tmp + PLL_COLS_53 * 0, ADD(LOADU(in_even + 0), dc_0));

    s1_1 = LOADU(in_even + stride + VREG_INT_COUNT);
    /* in_odd[0] - ((in_even[0] + s1 + 2) >> 2); */
    dc_1 = SUB(LOADU(in_odd + VREG_INT_COUNT),
               SAR(ADD3(LOADU(in_even + VREG_INT_COUNT), s1_1, two), 2));
    STORE(tmp + PLL_COLS_53 * 0 + VREG_INT_COUNT,
          ADD(LOADU(in_even + VREG_INT_COUNT), dc_1));

    for (i = 1, j = 1; i < (len - 2 - !(len & 1)); i += 2, j++) {

        s2_0 = LOADU(in_even + (j + 1) * stride);
        s2_1 = LOADU(in_even + (j + 1) * stride + VREG_INT_COUNT);

        /* dn = in_odd[j * stride] - ((s1 + s2 + 2) >> 2); */
        dn_0 = SUB(LOADU(in_odd + j * stride),
                   SAR(ADD3(s1_0, s2_0, two), 2));
        dn_1 = SUB(LOADU(in_odd + j * stride + VREG_INT_COUNT),
                   SAR(ADD3(s1_1, s2_1, two), 2));

        STORE(tmp + PLL_COLS_53 * i, dc_0);
        STORE(tmp + PLL_COLS_53 * i + VREG_INT_COUNT, dc_1);

        /* tmp[i + 1] = s1 + ((dn + dc) >> 1); */
        STORE(tmp + PLL_COLS_53 * (i + 1) + 0,
              ADD(s1_0, SAR(ADD(dn_0, dc_0), 1)));
        STORE(tmp + PLL_COLS_53 * (i + 1) + VREG_INT_COUNT,
              ADD(s1_1, SAR(ADD(dn_1, dc_1), 1)));

        dc_0 = dn_0;
        s1_0 = s2_0;
        dc_1 = dn_1;
        s1_1 = s2_1;
    }
    STORE(tmp + PLL_COLS_53 * i, dc_0);
    STORE(tmp + PLL_COLS_53 * i + VREG_INT_COUNT, dc_1);

    if (!(len & 1)) {
        /*dn = in_odd[(len / 2 - 1) * stride] - ((s1 + 1) >> 1); */
        dn_0 = SUB(LOADU(in_odd + (size_t)(len / 2 - 1) * stride),
                   SAR(ADD3(s1_0, s1_0, two), 2));
        dn_1 = SUB(LOADU(in_odd + (size_t)(len / 2 - 1) * stride + VREG_INT_COUNT),
                   SAR(ADD3(s1_1, s1_1, two), 2));

        /* tmp[len - 2] = s1 + ((dn + dc) >> 1); */
        STORE(tmp + PLL_COLS_53 * (len - 2) + 0,
              ADD(s1_0, SAR(ADD(dn_0, dc_0), 1)));
        STORE(tmp + PLL_COLS_53 * (len - 2) + VREG_INT_COUNT,
              ADD(s1_1, SAR(ADD(dn_1, dc_1), 1)));

        STORE(tmp + PLL_COLS_53 * (len - 1) + 0, dn_0);
        STORE(tmp + PLL_COLS_53 * (len - 1) + VREG_INT_COUNT, dn_1);
    } else {
        STORE(tmp + PLL_COLS_53 * (len - 1) + 0, ADD(s1_0, dc_0));
        STORE(tmp + PLL_COLS_53 * (len - 1) + VREG_INT_COUNT,
              ADD(s1_1, dc_1));
    }
    decode_v_final_memcpy_53(tiledp_col, tmp, len, stride);
}

#undef VREG
#undef LOAD_CST
#undef LOADU
#undef LOAD
#undef STORE
#undef STOREU
#undef ADD
#undef ADD3
#undef SUB
#undef SAR

#endif /* (defined(__SSE2__) || defined(__AVX2__)) */

/** Vertical inverse 5x3 wavelet transform for one column, when top-most
 * pixel is on even coordinate */
static void decode_v_cas0_53(int32_t* tmp,
                             const int32_t sn,
                             const int32_t len,
                             int32_t* tiledp_col,
                             const size_t stride){
    int32_t i, j;
    int32_t d1c, d1n, s1n, s0c, s0n;

    assert(len > 1);

    /* Performs lifting in one single iteration. Saves memory */
    /* accesses and explicit interleaving. */

    s1n = tiledp_col[0];
    d1n = tiledp_col[(size_t)sn * stride];
    s0n = s1n - ((d1n + 1) >> 1);

    for (i = 0, j = 0; i < (len - 3); i += 2, j++) {
        d1c = d1n;
        s0c = s0n;

        s1n = tiledp_col[(size_t)(j + 1) * stride];
        d1n = tiledp_col[(size_t)(sn + j + 1) * stride];

        s0n = s1n - ((d1c + d1n + 2) >> 2);

        tmp[i  ] = s0c;
        tmp[i + 1] = d1c + ((s0c + s0n) >> 1);
    }

    tmp[i] = s0n;

    if (len & 1) {
        tmp[len - 1] =
            tiledp_col[(size_t)((len - 1) / 2) * stride] -
            ((d1n + 1) >> 1);
        tmp[len - 2] = d1n + ((s0n + tmp[len - 1]) >> 1);
    } else {
        tmp[len - 1] = d1n + s0n;
    }

    for (i = 0; i < len; ++i) {
        tiledp_col[(size_t)i * stride] = tmp[i];
    }
}


/** Vertical inverse 5x3 wavelet transform for one column, when top-most
 * pixel is on odd coordinate */
static void decode_v_cas1_53(int32_t* tmp,
                             const int32_t sn,
                             const int32_t len,
                             int32_t* tiledp_col,
                             const size_t stride){
    int32_t i, j;
    int32_t s1, s2, dc, dn;
    const int32_t* in_even = &tiledp_col[(size_t)sn * stride];
    const int32_t* in_odd = &tiledp_col[0];

    assert(len > 2);

    /* Performs lifting in one single iteration. Saves memory */
    /* accesses and explicit interleaving. */

    s1 = in_even[stride];
    dc = in_odd[0] - ((in_even[0] + s1 + 2) >> 2);
    tmp[0] = in_even[0] + dc;
    for (i = 1, j = 1; i < (len - 2 - !(len & 1)); i += 2, j++) {

        s2 = in_even[(size_t)(j + 1) * stride];

        dn = in_odd[(size_t)j * stride] - ((s1 + s2 + 2) >> 2);
        tmp[i  ] = dc;
        tmp[i + 1] = s1 + ((dn + dc) >> 1);

        dc = dn;
        s1 = s2;
    }
    tmp[i] = dc;
    if (!(len & 1)) {
        dn = in_odd[(size_t)(len / 2 - 1) * stride] - ((s1 + 1) >> 1);
        tmp[len - 2] = s1 + ((dn + dc) >> 1);
        tmp[len - 1] = dn;
    } else {
        tmp[len - 1] = s1 + dc;
    }

    for (i = 0; i < len; ++i) {
        tiledp_col[(size_t)i * stride] = tmp[i];
    }
}

/* <summary>                            */
/* Inverse vertical 5-3 wavelet transform in 1-D for several columns. */
/* </summary>                           */
/* Performs interleave, inverse wavelet transform and copy back to buffer */
static void decode_v_53(const dwt_data_53 *dwt,
                         int32_t* tiledp_col,
                         size_t stride,
                         int32_t nb_cols){
    const int32_t sn = dwt->sn;
    const int32_t len = sn + dwt->dn;
    if (dwt->cas == 0) {
        /* If len == 1, unmodified value */

#if (defined(__SSE2__) || defined(__AVX2__))
        if (len > 1 && nb_cols == PLL_COLS_53) {
            /* Same as below general case, except that thanks to SSE2/AVX2 */
            /* we can efficiently process 8/16 columns in parallel */
            decode_v_cas0_mcols_SSE2_OR_AVX2_53(dwt->mem, sn, len, tiledp_col, stride);
            return;
        }
#endif
        if (len > 1) {
            int32_t c;
            for (c = 0; c < nb_cols; c++, tiledp_col++) {
                decode_v_cas0_53(dwt->mem, sn, len, tiledp_col, stride);
            }
            return;
        }
    } else {
        if (len == 1) {
            int32_t c;
            for (c = 0; c < nb_cols; c++, tiledp_col++) {
                tiledp_col[0] /= 2;
            }
            return;
        }

        if (len == 2) {
            int32_t c;
            int32_t* out = dwt->mem;
            for (c = 0; c < nb_cols; c++, tiledp_col++) {
                int32_t i;
                const int32_t* in_even = &tiledp_col[(size_t)sn * stride];
                const int32_t* in_odd = &tiledp_col[0];

                out[1] = in_odd[0] - ((in_even[0] + 1) >> 1);
                out[0] = in_even[0] + out[1];

                for (i = 0; i < len; ++i) {
                    tiledp_col[(size_t)i * stride] = out[i];
                }
            }

            return;
        }

#if (defined(__SSE2__) || defined(__AVX2__))
        if (len > 2 && nb_cols == PLL_COLS_53) {
            /* Same as below general case, except that thanks to SSE2/AVX2 */
            /* we can efficiently process 8/16 columns in parallel */
            decode_v_cas1_mcols_SSE2_OR_AVX2_53(dwt->mem, sn, len, tiledp_col, stride);
            return;
        }
#endif
        if (len > 2) {
            int32_t c;
            for (c = 0; c < nb_cols; c++, tiledp_col++) {
                decode_v_cas1_53(dwt->mem, sn, len, tiledp_col, stride);
            }
            return;
        }
    }
}

/*
==========================================================
   DWT interface
==========================================================
*/



/* <summary>                            */
/* Inverse 5-3 wavelet transform in 2-D. */
/* </summary>                           */
bool decode_53(TileProcessor *p_tcd, TileComponent* tilec,
                        uint32_t numres)
{
    if (p_tcd->whole_tile_decoding) {
        return decode_tile_53(tilec,numres);
    } else {
        return decode_partial_tile_53(tilec, numres);
    }
}

/* <summary>                             */
/* Determine maximum computed resolution level for inverse wavelet transform */
/* </summary>                            */
static uint32_t max_resolution(grk_tcd_resolution* restrict r,
        uint32_t i){
    uint32_t mr   = 0;
    uint32_t w;
    while (--i) {
        ++r;
        if (mr < (w = (uint32_t)(r->x1 - r->x0))) {
            mr = w ;
        }
        if (mr < (w = (uint32_t)(r->y1 - r->y0))) {
            mr = w ;
        }
    }

    return mr ;
}

/* <summary>                            */
/* Inverse wavelet transform in 2-D.    */
/* </summary>                           */
static bool decode_tile_53( TileComponent* tilec, uint32_t numres){
    if (numres == 1U)
        return true;

    auto tr = tilec->resolutions;

    /* width of the resolution level computed */
    uint32_t rw = (uint32_t)(tr->x1 - tr->x0);
    /* height of the resolution level computed */
    uint32_t rh = (uint32_t)(tr->y1 - tr->y0);

    uint32_t w = (uint32_t)(tilec->resolutions[tilec->minimum_num_resolutions - 1].x1 -
                                tilec->resolutions[tilec->minimum_num_resolutions - 1].x0);

    size_t num_threads = Scheduler::g_tp->num_threads();
    size_t h_mem_size = max_resolution(tr, numres);
    /* overflow check */
    if (h_mem_size > (SIZE_MAX / PLL_COLS_53 / sizeof(int32_t))) {
        GROK_ERROR("Overflow");
        return false;
    }
    /* We need PLL_COLS_53 times the height of the array, */
    /* since for the vertical pass */
    /* we process PLL_COLS_53 columns at a time */
    dwt_data_53 h;
    h_mem_size *= PLL_COLS_53 * sizeof(int32_t);
    h.mem = (int32_t*)grok_aligned_malloc(h_mem_size);
    if (! h.mem) {
        GROK_ERROR("Out of memory");
        return false;
    }
    dwt_data_53 v;
    v.mem = h.mem;
    bool rc = true;
    int32_t * restrict tiledp = tilec->buf->get_ptr( 0, 0, 0, 0);
    while (--numres) {
        ++tr;
        h.sn = (int32_t)rw;
        v.sn = (int32_t)rh;

        rw = (uint32_t)(tr->x1 - tr->x0);
        rh = (uint32_t)(tr->y1 - tr->y0);

        h.dn = (int32_t)(rw - (uint32_t)h.sn);
        h.cas = tr->x0 % 2;

        if (num_threads <= 1 || rh <= 1) {
            for (uint32_t j = 0; j < rh; ++j)
                decode_h_53(&h, &tiledp[(size_t)j * w]);
        } else {
            uint32_t num_jobs = (uint32_t)num_threads;
            if (rh < num_jobs)
                num_jobs = rh;
            uint32_t step_j = (rh / num_jobs);
			std::vector< std::future<int> > results;
			for(uint32_t j = 0; j < num_jobs; ++j) {
               auto job = new decode_job<dwt_data_53>(h,
											w,
											tiledp,
											j * step_j,
											j < (num_jobs - 1U) ? (j + 1U) * step_j : rh);
                job->data.mem = (int32_t*)grok_aligned_malloc(h_mem_size);
                if (!job->data.mem) {
                    GROK_ERROR("Out of memory");
                    grok_aligned_free(h.mem);
                    return false;
                }
				results.emplace_back(
					Scheduler::g_tp->enqueue([job] {
					    for (uint32_t j = job->min_j; j < job->max_j; j++)
					        decode_h_53(&job->data, &job->tiledp[j * job->w]);
					    grok_aligned_free(job->data.mem);
					    delete job;
						return 0;
					})
				);
			}
			for(auto && result: results)
				result.get();
        }

        v.dn = (int32_t)(rh - (uint32_t)v.sn);
        v.cas = tr->y0 % 2;

        if (num_threads <= 1 || rw <= 1) {
            uint32_t j;
            for (j = 0; j + PLL_COLS_53 <= rw; j += PLL_COLS_53)
                decode_v_53(&v, &tiledp[j], (size_t)w, PLL_COLS_53);
            if (j < rw)
                decode_v_53(&v, &tiledp[j], (size_t)w, (int32_t)(rw - j));
        } else {
            uint32_t num_jobs = (uint32_t)num_threads;
            if (rw < num_jobs)
                num_jobs = rw;
            uint32_t step_j = (rw / num_jobs);
			std::vector< std::future<int> > results;
            for (uint32_t j = 0; j < num_jobs; j++) {
                auto job = new decode_job<dwt_data_53>(v,
											w,
											tiledp,
											j * step_j,
											j < (num_jobs - 1U) ? (j + 1U) * step_j : rw);
                job->data.mem = (int32_t*)grok_aligned_malloc(h_mem_size);
                if (!job->data.mem) {
                    GROK_ERROR("Out of memory");
                    grok_aligned_free(v.mem);
                    return false;
                }
				results.emplace_back(
					Scheduler::g_tp->enqueue([job] {
						uint32_t j;
						for (j = job->min_j; j + PLL_COLS_53 <= job->max_j;	j += PLL_COLS_53)
							decode_v_53(&job->data, &job->tiledp[j], (size_t)job->w, PLL_COLS_53);
						if (j < job->max_j)
							decode_v_53(&job->data, &job->tiledp[j], (size_t)job->w, (int32_t)(job->max_j - j));
						grok_aligned_free(job->data.mem);
						delete job;
					return 0;
					})
				);
            }
			for(auto && result: results)
				result.get();
        }
    }
    grok_aligned_free(h.mem);

    return rc;
}

static void interleave_partial_h_53(int32_t *dest,
        int32_t cas,
        sparse_array_int32_t* sa,
        uint32_t sa_line,
        uint32_t sn,
        uint32_t win_l_x0,
        uint32_t win_l_x1,
        uint32_t win_h_x0,
        uint32_t win_h_x1){
    bool ret = sparse_array_int32_read(sa,
                                      win_l_x0, sa_line,
                                      win_l_x1, sa_line + 1,
                                      dest + cas + 2 * win_l_x0,
                                      2, 0, true);
    assert(ret);
    ret = sparse_array_int32_read(sa,
                                      sn + win_h_x0, sa_line,
                                      sn + win_h_x1, sa_line + 1,
                                      dest + 1 - cas + 2 * win_h_x0,
                                      2, 0, true);
    assert(ret);
    GRK_UNUSED(ret);
}


static void interleave_partial_v_53(int32_t *dest,
        int32_t cas,
        sparse_array_int32_t* sa,
        uint32_t sa_col,
        uint32_t nb_cols,
        uint32_t sn,
        uint32_t win_l_y0,
        uint32_t win_l_y1,
        uint32_t win_h_y0,
        uint32_t win_h_y1){
    bool ret = sparse_array_int32_read(sa,
                                       sa_col, win_l_y0,
                                       sa_col + nb_cols, win_l_y1,
                                       dest + cas * 4 + 2 * 4 * win_l_y0,
                                       1, 2 * 4, true);
    assert(ret);
    ret = sparse_array_int32_read(sa,
                                      sa_col, sn + win_h_y0,
                                      sa_col + nb_cols, sn + win_h_y1,
                                      dest + (1 - cas) * 4 + 2 * 4 * win_h_y0,
                                      1, 2 * 4, true);
    assert(ret);
    GRK_UNUSED(ret);
}

static void decode_partial_1_53(int32_t *a, int32_t dn, int32_t sn,
                                     int32_t cas,
                                     int32_t win_l_x0,
                                     int32_t win_l_x1,
                                     int32_t win_h_x0,
                                     int32_t win_h_x1){
    int32_t i;

    if (!cas) {
        if ((dn > 0) || (sn > 1)) { /* NEW :  CASE ONE ELEMENT */

            /* Naive version is :
            for (i = win_l_x0; i < i_max; i++) {
                GRK_S(i) -= (GRK_D_(i - 1) + GRK_D_(i) + 2) >> 2;
            }
            for (i = win_h_x0; i < win_h_x1; i++) {
                GRK_D(i) += (GRK_S_(i) + GRK_S_(i + 1)) >> 1;
            }
            but the compiler doesn't manage to unroll it to avoid bound
            checking in GRK_S_ and GRK_D_ macros
            */

            i = win_l_x0;
            if (i < win_l_x1) {
                int32_t i_max;

                /* Left-most case */
                GRK_S(i) -= (GRK_D_(i - 1) + GRK_D_(i) + 2) >> 2;
                i ++;

                i_max = win_l_x1;
                if (i_max > dn) {
                    i_max = dn;
                }
                for (; i < i_max; i++) {
                    /* No bound checking */
                    GRK_S(i) -= (GRK_D(i - 1) + GRK_D(i) + 2) >> 2;
                }
                for (; i < win_l_x1; i++) {
                    /* Right-most case */
                    GRK_S(i) -= (GRK_D_(i - 1) + GRK_D_(i) + 2) >> 2;
                }
            }

            i = win_h_x0;
            if (i < win_h_x1) {
                int32_t i_max = win_h_x1;
                if (i_max >= sn) {
                    i_max = sn - 1;
                }
                for (; i < i_max; i++) {
                    /* No bound checking */
                    GRK_D(i) += (GRK_S(i) + GRK_S(i + 1)) >> 1;
                }
                for (; i < win_h_x1; i++) {
                    /* Right-most case */
                    GRK_D(i) += (GRK_S_(i) + GRK_S_(i + 1)) >> 1;
                }
            }
        }
    } else {
        if (!sn  && dn == 1) {        /* NEW :  CASE ONE ELEMENT */
            GRK_S(0) /= 2;
        } else {
            for (i = win_l_x0; i < win_l_x1; i++) {
                GRK_D(i) -= (GRK_SS_(i) + GRK_SS_(i + 1) + 2) >> 2;
            }
            for (i = win_h_x0; i < win_h_x1; i++) {
                GRK_S(i) += (GRK_DD_(i) + GRK_DD_(i - 1)) >> 1;
            }
        }
    }
}

#define GRK_S_off(i,off) a[(uint32_t)(i)*2*4+off]
#define GRK_D_off(i,off) a[(1+(uint32_t)(i)*2)*4+off]
#define GRK_S__off(i,off) ((i)<0?GRK_S_off(0,off):((i)>=sn?GRK_S_off(sn-1,off):GRK_S_off(i,off)))
#define GRK_D__off(i,off) ((i)<0?GRK_D_off(0,off):((i)>=dn?GRK_D_off(dn-1,off):GRK_D_off(i,off)))
#define GRK_SS__off(i,off) ((i)<0?GRK_S_off(0,off):((i)>=dn?GRK_S_off(dn-1,off):GRK_S_off(i,off)))
#define GRK_DD__off(i,off) ((i)<0?GRK_D_off(0,off):((i)>=sn?GRK_D_off(sn-1,off):GRK_D_off(i,off)))

static void decode_partial_1_parallel_53(int32_t *a,
        uint32_t nb_cols,
        int32_t dn, int32_t sn,
        int32_t cas,
        int32_t win_l_x0,
        int32_t win_l_x1,
        int32_t win_h_x0,
        int32_t win_h_x1){
    int32_t i;
    uint32_t off;

    (void)nb_cols;

    if (!cas) {
        if ((dn > 0) || (sn > 1)) { /* NEW :  CASE ONE ELEMENT */

            /* Naive version is :
            for (i = win_l_x0; i < i_max; i++) {
                GRK_S(i) -= (GRK_D_(i - 1) + GRK_D_(i) + 2) >> 2;
            }
            for (i = win_h_x0; i < win_h_x1; i++) {
                GRK_D(i) += (GRK_S_(i) + GRK_S_(i + 1)) >> 1;
            }
            but the compiler doesn't manage to unroll it to avoid bound
            checking in GRK_S_ and GRK_D_ macros
            */

            i = win_l_x0;
            if (i < win_l_x1) {
                int32_t i_max;

                /* Left-most case */
                for (off = 0; off < 4; off++) {
                    GRK_S_off(i, off) -= (GRK_D__off(i - 1, off) + GRK_D__off(i, off) + 2) >> 2;
                }
                i ++;

                i_max = win_l_x1;
                if (i_max > dn) {
                    i_max = dn;
                }

#ifdef __SSE2__
                if (i + 1 < i_max) {
                    const __m128i two = _mm_set1_epi32(2);
                    __m128i Dm1 = _mm_load_si128((__m128i *)(a + 4 + (i - 1) * 8));
                    for (; i + 1 < i_max; i += 2) {
                        /* No bound checking */
                        __m128i S = _mm_load_si128((__m128i *)(a + i * 8));
                        __m128i D = _mm_load_si128((__m128i *)(a + 4 + i * 8));
                        __m128i S1 = _mm_load_si128((__m128i *)(a + (i + 1) * 8));
                        __m128i D1 = _mm_load_si128((__m128i *)(a + 4 + (i + 1) * 8));
                        S = _mm_sub_epi32(S,
                                          _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(Dm1, D), two), 2));
                        S1 = _mm_sub_epi32(S1,
                                           _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(D, D1), two), 2));
                        _mm_store_si128((__m128i*)(a + i * 8), S);
                        _mm_store_si128((__m128i*)(a + (i + 1) * 8), S1);
                        Dm1 = D1;
                    }
                }
#endif

                for (; i < i_max; i++) {
                    /* No bound checking */
                    for (off = 0; off < 4; off++) {
                        GRK_S_off(i, off) -= (GRK_D_off(i - 1, off) + GRK_D_off(i, off) + 2) >> 2;
                    }
                }
                for (; i < win_l_x1; i++) {
                    /* Right-most case */
                    for (off = 0; off < 4; off++) {
                        GRK_S_off(i, off) -= (GRK_D__off(i - 1, off) + GRK_D__off(i, off) + 2) >> 2;
                    }
                }
            }

            i = win_h_x0;
            if (i < win_h_x1) {
                int32_t i_max = win_h_x1;
                if (i_max >= sn) {
                    i_max = sn - 1;
                }

#ifdef __SSE2__
                if (i + 1 < i_max) {
                    __m128i S =  _mm_load_si128((__m128i *)(a + i * 8));
                    for (; i + 1 < i_max; i += 2) {
                        /* No bound checking */
                        __m128i D = _mm_load_si128((__m128i *)(a + 4 + i * 8));
                        __m128i S1 = _mm_load_si128((__m128i *)(a + (i + 1) * 8));
                        __m128i D1 = _mm_load_si128((__m128i *)(a + 4 + (i + 1) * 8));
                        __m128i S2 = _mm_load_si128((__m128i *)(a + (i + 2) * 8));
                        D = _mm_add_epi32(D, _mm_srai_epi32(_mm_add_epi32(S, S1), 1));
                        D1 = _mm_add_epi32(D1, _mm_srai_epi32(_mm_add_epi32(S1, S2), 1));
                        _mm_store_si128((__m128i*)(a + 4 + i * 8), D);
                        _mm_store_si128((__m128i*)(a + 4 + (i + 1) * 8), D1);
                        S = S2;
                    }
                }
#endif

                for (; i < i_max; i++) {
                    /* No bound checking */
                    for (off = 0; off < 4; off++) {
                        GRK_D_off(i, off) += (GRK_S_off(i, off) + GRK_S_off(i + 1, off)) >> 1;
                    }
                }
                for (; i < win_h_x1; i++) {
                    /* Right-most case */
                    for (off = 0; off < 4; off++) {
                        GRK_D_off(i, off) += (GRK_S__off(i, off) + GRK_S__off(i + 1, off)) >> 1;
                    }
                }
            }
        }
    } else {
        if (!sn  && dn == 1) {        /* NEW :  CASE ONE ELEMENT */
            for (off = 0; off < 4; off++) {
                GRK_S_off(0, off) /= 2;
            }
        } else {
            for (i = win_l_x0; i < win_l_x1; i++) {
                for (off = 0; off < 4; off++) {
                    GRK_D_off(i, off) -= (GRK_SS__off(i, off) + GRK_SS__off(i + 1, off) + 2) >> 2;
                }
            }
            for (i = win_h_x0; i < win_h_x1; i++) {
                for (off = 0; off < 4; off++) {
                    GRK_S_off(i, off) += (GRK_DD__off(i, off) + GRK_DD__off(i - 1, off)) >> 1;
                }
            }
        }
    }
}

static void get_band_coordinates(TileComponent* tilec,
        uint32_t resno,
        uint32_t bandno,
        uint32_t tcx0,
        uint32_t tcy0,
        uint32_t tcx1,
        uint32_t tcy1,
        uint32_t* tbx0,
        uint32_t* tby0,
        uint32_t* tbx1,
        uint32_t* tby1){
    /* Compute number of decomposition for this band. See table F-1 */
    uint32_t nb = (resno == 0) ?
                    tilec->numresolutions - 1 :
                    tilec->numresolutions - resno;
    /* Map above tile-based coordinates to sub-band-based coordinates per */
    /* equation B-15 of the standard */
    uint32_t x0b = bandno & 1;
    uint32_t y0b = bandno >> 1;
    if (tbx0) {
        *tbx0 = (nb == 0) ? tcx0 :
                (tcx0 <= (1U << (nb - 1)) * x0b) ? 0 :
                uint_ceildivpow2(tcx0 - (1U << (nb - 1)) * x0b, nb);
    }
    if (tby0) {
        *tby0 = (nb == 0) ? tcy0 :
                (tcy0 <= (1U << (nb - 1)) * y0b) ? 0 :
                uint_ceildivpow2(tcy0 - (1U << (nb - 1)) * y0b, nb);
    }
    if (tbx1) {
        *tbx1 = (nb == 0) ? tcx1 :
                (tcx1 <= (1U << (nb - 1)) * x0b) ? 0 :
                uint_ceildivpow2(tcx1 - (1U << (nb - 1)) * x0b, nb);
    }
    if (tby1) {
        *tby1 = (nb == 0) ? tcy1 :
                (tcy1 <= (1U << (nb - 1)) * y0b) ? 0 :
                uint_ceildivpow2(tcy1 - (1U << (nb - 1)) * y0b, nb);
    }
}

static void segment_grow(uint32_t filter_width,
                                 uint32_t max_size,
                                 uint32_t* start,
                                 uint32_t* end){
    *start = uint_subs(*start, filter_width);
    *end = uint_adds(*end, filter_width);
    *end = min<uint32_t>(*end, max_size);
}


static sparse_array_int32_t* init_sparse_array(
    TileComponent* tilec,
    uint32_t numres)
{
    grk_tcd_resolution* tr_max = &(tilec->resolutions[numres - 1]);
    uint32_t w = (uint32_t)(tr_max->x1 - tr_max->x0);
    uint32_t h = (uint32_t)(tr_max->y1 - tr_max->y0);
    uint32_t resno, bandno, precno, cblkno;
    sparse_array_int32_t* sa = sparse_array_int32_create(
                                       w, h, min<uint32_t>(w, 64), min<uint32_t>(h, 64));
    if (sa == NULL) {
        return NULL;
    }

    for (resno = 0; resno < numres; ++resno) {
        grk_tcd_resolution* res = &tilec->resolutions[resno];

        for (bandno = 0; bandno < res->numbands; ++bandno) {
            grk_tcd_band* band = &res->bands[bandno];

            for (precno = 0; precno < res->pw * res->ph; ++precno) {
                grk_tcd_precinct* precinct = &band->precincts[precno];
                for (cblkno = 0; cblkno < precinct->cw * precinct->ch; ++cblkno) {
                    grk_tcd_cblk_dec* cblk = &precinct->cblks.dec[cblkno];
                    if (cblk->unencoded_data != NULL) {
                        uint32_t x = (uint32_t)(cblk->x0 - band->x0);
                        uint32_t y = (uint32_t)(cblk->y0 - band->y0);
                        uint32_t cblk_w = (uint32_t)(cblk->x1 - cblk->x0);
                        uint32_t cblk_h = (uint32_t)(cblk->y1 - cblk->y0);

                        if (band->bandno & 1) {
                            grk_tcd_resolution* pres = &tilec->resolutions[resno - 1];
                            x += (uint32_t)(pres->x1 - pres->x0);
                        }
                        if (band->bandno & 2) {
                            grk_tcd_resolution* pres = &tilec->resolutions[resno - 1];
                            y += (uint32_t)(pres->y1 - pres->y0);
                        }

                        if (!sparse_array_int32_write(sa, x, y,
                                                          x + cblk_w, y + cblk_h,
														  cblk->unencoded_data,
                                                          1, cblk_w, true)) {
                            sparse_array_int32_free(sa);
                            return NULL;
                        }
                    }
                }
            }
        }
    }

    return sa;
}

static bool decode_partial_tile_53(
    TileComponent* tilec,
    uint32_t numres){
    sparse_array_int32_t* sa;
    dwt_data_53 h;
    dwt_data_53 v;
    uint32_t resno;
    /* This value matches the maximum left/right extension given in tables */
    /* F.2 and F.3 of the standard. */
    const uint32_t filter_width = 2U;

    grk_tcd_resolution* tr = tilec->resolutions;
    grk_tcd_resolution* tr_max = &(tilec->resolutions[numres - 1]);

    uint32_t rw = (uint32_t)(tr->x1 -
                                 tr->x0);  /* width of the resolution level computed */
    uint32_t rh = (uint32_t)(tr->y1 -
                                 tr->y0);  /* height of the resolution level computed */

    size_t h_mem_size;

    /* Compute the intersection of the area of interest, expressed in tile coordinates */
    /* with the tile coordinates */
    auto dim = tilec->buf->unreduced_image_dim;

    uint32_t win_tcx0 = (uint32_t)dim.x0;
    uint32_t win_tcy0 = (uint32_t)dim.y0;
    uint32_t win_tcx1 = (uint32_t)dim.x1;
    uint32_t win_tcy1 = (uint32_t)dim.y1;

    if (tr_max->x0 == tr_max->x1 || tr_max->y0 == tr_max->y1) {
        return true;
    }

    sa = init_sparse_array(tilec, numres);
    if (sa == NULL) {
        return false;
    }

    if (numres == 1U) {
        bool ret = sparse_array_int32_read(sa,
                       tr_max->win_x0 - (uint32_t)tr_max->x0,
                       tr_max->win_y0 - (uint32_t)tr_max->y0,
                       tr_max->win_x1 - (uint32_t)tr_max->x0,
                       tr_max->win_y1 - (uint32_t)tr_max->y0,
                       tilec->buf->get_ptr(0,0,0,0),
                       1, tr_max->win_x1 - tr_max->win_x0,
                       true);
        assert(ret);
        GRK_UNUSED(ret);
        sparse_array_int32_free(sa);
        return true;
    }
    h_mem_size = max_resolution(tr, numres);
    /* overflow check */
    /* in vertical pass, we process 4 columns at a time */
    if (h_mem_size > (SIZE_MAX / (4 * sizeof(int32_t)))) {
        /* FIXME event manager error callback */
        sparse_array_int32_free(sa);
        return false;
    }

    h_mem_size *= 4 * sizeof(int32_t);
    h.mem = (int32_t*)grok_aligned_malloc(h_mem_size);
    if (! h.mem) {
        /* FIXME event manager error callback */
        sparse_array_int32_free(sa);
        return false;
    }

    v.mem = h.mem;

    for (resno = 1; resno < numres; resno ++) {
        uint32_t i, j;
        /* Window of interest subband-based coordinates */
        uint32_t win_ll_x0, win_ll_y0, win_ll_x1, win_ll_y1;
        uint32_t win_hl_x0, win_hl_x1;
        uint32_t win_lh_y0, win_lh_y1;
        /* Window of interest tile-resolution-based coordinates */
        uint32_t win_tr_x0, win_tr_x1, win_tr_y0, win_tr_y1;
        /* Tile-resolution subband-based coordinates */
        uint32_t tr_ll_x0, tr_ll_y0, tr_hl_x0, tr_lh_y0;

        ++tr;

        h.sn = (int32_t)rw;
        v.sn = (int32_t)rh;

        rw = (uint32_t)(tr->x1 - tr->x0);
        rh = (uint32_t)(tr->y1 - tr->y0);

        h.dn = (int32_t)(rw - (uint32_t)h.sn);
        h.cas = tr->x0 % 2;

        v.dn = (int32_t)(rh - (uint32_t)v.sn);
        v.cas = tr->y0 % 2;

        /* Get the subband coordinates for the window of interest */
        /* LL band */
        get_band_coordinates(tilec, resno, 0,
                                     win_tcx0, win_tcy0, win_tcx1, win_tcy1,
                                     &win_ll_x0, &win_ll_y0,
                                     &win_ll_x1, &win_ll_y1);

        /* HL band */
        get_band_coordinates(tilec, resno, 1,
                                     win_tcx0, win_tcy0, win_tcx1, win_tcy1,
                                     &win_hl_x0, NULL, &win_hl_x1, NULL);

        /* LH band */
        get_band_coordinates(tilec, resno, 2,
                                     win_tcx0, win_tcy0, win_tcx1, win_tcy1,
                                     NULL, &win_lh_y0, NULL, &win_lh_y1);

        /* Beware: band index for non-LL0 resolution are 0=HL, 1=LH and 2=HH */
        tr_ll_x0 = (uint32_t)tr->bands[1].x0;
        tr_ll_y0 = (uint32_t)tr->bands[0].y0;
        tr_hl_x0 = (uint32_t)tr->bands[0].x0;
        tr_lh_y0 = (uint32_t)tr->bands[1].y0;

        /* Subtract the origin of the bands for this tile, to the subwindow */
        /* of interest band coordinates, so as to get them relative to the */
        /* tile */
        win_ll_x0 = uint_subs(win_ll_x0, tr_ll_x0);
        win_ll_y0 = uint_subs(win_ll_y0, tr_ll_y0);
        win_ll_x1 = uint_subs(win_ll_x1, tr_ll_x0);
        win_ll_y1 = uint_subs(win_ll_y1, tr_ll_y0);
        win_hl_x0 = uint_subs(win_hl_x0, tr_hl_x0);
        win_hl_x1 = uint_subs(win_hl_x1, tr_hl_x0);
        win_lh_y0 = uint_subs(win_lh_y0, tr_lh_y0);
        win_lh_y1 = uint_subs(win_lh_y1, tr_lh_y0);

        segment_grow(filter_width, (uint32_t)h.sn, &win_ll_x0, &win_ll_x1);
        segment_grow(filter_width, (uint32_t)h.dn, &win_hl_x0, &win_hl_x1);

        segment_grow(filter_width, (uint32_t)v.sn, &win_ll_y0, &win_ll_y1);
        segment_grow(filter_width, (uint32_t)v.dn, &win_lh_y0, &win_lh_y1);

        /* Compute the tile-resolution-based coordinates for the window of interest */
        if (h.cas == 0) {
            win_tr_x0 = min<uint32_t>(2 * win_ll_x0, 2 * win_hl_x0 + 1);
            win_tr_x1 = min<uint32_t>(max<uint32_t>(2 * win_ll_x1, 2 * win_hl_x1 + 1), rw);
        } else {
            win_tr_x0 = min<uint32_t>(2 * win_hl_x0, 2 * win_ll_x0 + 1);
            win_tr_x1 = min<uint32_t>(max<uint32_t>(2 * win_hl_x1, 2 * win_ll_x1 + 1), rw);
        }

        if (v.cas == 0) {
            win_tr_y0 = min<uint32_t>(2 * win_ll_y0, 2 * win_lh_y0 + 1);
            win_tr_y1 = min<uint32_t>(max<uint32_t>(2 * win_ll_y1, 2 * win_lh_y1 + 1), rh);
        } else {
            win_tr_y0 = min<uint32_t>(2 * win_lh_y0, 2 * win_ll_y0 + 1);
            win_tr_y1 = min<uint32_t>(max<uint32_t>(2 * win_lh_y1, 2 * win_ll_y1 + 1), rh);
        }

        for (j = 0; j < rh; ++j) {
            if ((j >= win_ll_y0 && j < win_ll_y1) ||
                    (j >= win_lh_y0 + (uint32_t)v.sn && j < win_lh_y1 + (uint32_t)v.sn)) {

                /* Avoids dwt.c:1584:44 (in dwt_decode_partial_1): runtime error: */
                /* signed integer overflow: -1094795586 + -1094795586 cannot be represented in type 'int' */
                /* on decompress -i  ../../openjpeg/MAPA.jp2 -o out.tif -d 0,0,256,256 */
                /* This is less extreme than memsetting the whole buffer to 0 */
                /* although we could potentially do better with better handling of edge conditions */
                if (win_tr_x1 >= 1 && win_tr_x1 < rw) {
                    h.mem[win_tr_x1 - 1] = 0;
                }
                if (win_tr_x1 < rw) {
                    h.mem[win_tr_x1] = 0;
                }

                interleave_partial_h_53(h.mem,
                                             h.cas,
                                             sa,
                                             j,
                                             (uint32_t)h.sn,
                                             win_ll_x0,
                                             win_ll_x1,
                                             win_hl_x0,
                                             win_hl_x1);
                decode_partial_1_53(h.mem, h.dn, h.sn, h.cas,
                                         (int32_t)win_ll_x0,
                                         (int32_t)win_ll_x1,
                                         (int32_t)win_hl_x0,
                                         (int32_t)win_hl_x1);
                if (!sparse_array_int32_write(sa,
                                                  win_tr_x0, j,
                                                  win_tr_x1, j + 1,
                                                  h.mem + win_tr_x0,
                                                  1, 0, true)) {
                    /* FIXME event manager error callback */
                    sparse_array_int32_free(sa);
                    grok_aligned_free(h.mem);
                    return false;
                }
            }
        }

        for (i = win_tr_x0; i < win_tr_x1;) {
            uint32_t nb_cols = min<uint32_t>(4U, win_tr_x1 - i);
            interleave_partial_v_53(v.mem,
                                         v.cas,
                                         sa,
                                         i,
                                         nb_cols,
                                         (uint32_t)v.sn,
                                         win_ll_y0,
                                         win_ll_y1,
                                         win_lh_y0,
                                         win_lh_y1);
            decode_partial_1_parallel_53(v.mem, nb_cols, v.dn, v.sn, v.cas,
                                              (int32_t)win_ll_y0,
                                              (int32_t)win_ll_y1,
                                              (int32_t)win_lh_y0,
                                              (int32_t)win_lh_y1);
            if (!sparse_array_int32_write(sa,
                                              i, win_tr_y0,
                                              i + nb_cols, win_tr_y1,
                                              v.mem + 4 * win_tr_y0,
                                              1, 4, true)) {
                /* FIXME event manager error callback */
                sparse_array_int32_free(sa);
                grok_aligned_free(h.mem);
                return false;
            }

            i += nb_cols;
        }
    }
    grok_aligned_free(h.mem);
	bool ret = sparse_array_int32_read(sa,
				   tr_max->win_x0 - (uint32_t)tr_max->x0,
				   tr_max->win_y0 - (uint32_t)tr_max->y0,
				   tr_max->win_x1 - (uint32_t)tr_max->x0,
				   tr_max->win_y1 - (uint32_t)tr_max->y0,
				   tilec->buf->get_ptr(0,0,0,0),
				   1, tr_max->win_x1 - tr_max->win_x0,
				   true);
	assert(ret);
	GRK_UNUSED(ret);
    sparse_array_int32_free(sa);

    return true;
}

static void interleave_h_97(dwt_data_97* restrict dwt,
                                   float* restrict a,
                                   uint32_t width,
                                   uint32_t remaining_height){
    float* restrict bi = (float*)(dwt->wavelet + dwt->cas);
    uint32_t i, k;
    uint32_t x0 = dwt->win_l_x0;
    uint32_t x1 = dwt->win_l_x1;

    for (k = 0; k < 2; ++k) {
        if (remaining_height >= 4 && ((size_t) a & 0x0f) == 0 &&
                ((size_t) bi & 0x0f) == 0 && (width & 0x0f) == 0) {
            /* Fast code path */
            for (i = x0; i < x1; ++i) {
                uint32_t j = i;
                bi[i * 8    ] = a[j];
                j += width;
                bi[i * 8 + 1] = a[j];
                j += width;
                bi[i * 8 + 2] = a[j];
                j += width;
                bi[i * 8 + 3] = a[j];
            }
        } else {
            /* Slow code path */
            for (i = x0; i < x1; ++i) {
                uint32_t j = i;
                bi[i * 8    ] = a[j];
                j += width;
                if (remaining_height == 1) {
                    continue;
                }
                bi[i * 8 + 1] = a[j];
                j += width;
                if (remaining_height == 2) {
                    continue;
                }
                bi[i * 8 + 2] = a[j];
                j += width;
                if (remaining_height == 3) {
                    continue;
                }
                bi[i * 8 + 3] = a[j]; /* This one*/
            }
        }

        bi = (float*)(dwt->wavelet + 1 - dwt->cas);
        a += dwt->sn;
        x0 = dwt->win_h_x0;
        x1 = dwt->win_h_x1;
    }
}

static void interleave_partial_h_97(dwt_data_97* dwt,
        sparse_array_int32_t* sa,
        uint32_t sa_line,
        uint32_t remaining_height){
    uint32_t i;
    for (i = 0; i < remaining_height; i++) {
        bool ret;
        ret = sparse_array_int32_read(sa,
                                          dwt->win_l_x0, sa_line + i,
                                          dwt->win_l_x1, sa_line + i + 1,
                                          /* Nasty cast from float* to int32* */
                                          (int32_t*)(dwt->wavelet + dwt->cas + 2 * dwt->win_l_x0) + i,
                                          8, 0, true);
        assert(ret);
        ret = sparse_array_int32_read(sa,
                                          (uint32_t)dwt->sn + dwt->win_h_x0, sa_line + i,
                                          (uint32_t)dwt->sn + dwt->win_h_x1, sa_line + i + 1,
                                          /* Nasty cast from float* to int32* */
                                          (int32_t*)(dwt->wavelet + 1 - dwt->cas + 2 * dwt->win_h_x0) + i,
                                          8, 0, true);
        assert(ret);
        GRK_UNUSED(ret);
    }
}

static void interleave_v_97(dwt_data_97* restrict dwt,
                                   float* restrict a,
                                   uint32_t width,
                                   uint32_t nb_elts_read){
    v4_data* restrict bi = dwt->wavelet + dwt->cas;
    uint32_t i;

    for (i = dwt->win_l_x0; i < dwt->win_l_x1; ++i) {
        memcpy(&bi[i * 2], &a[i * (size_t)width],
               (size_t)nb_elts_read * sizeof(float));
    }

    a += (uint32_t)dwt->sn * (size_t)width;
    bi = dwt->wavelet + 1 - dwt->cas;

    for (i = dwt->win_h_x0; i < dwt->win_h_x1; ++i) {
        memcpy(&bi[i * 2], &a[i * (size_t)width],
               (size_t)nb_elts_read * sizeof(float));
    }
}

static void interleave_partial_v_97(dwt_data_97* restrict dwt,
        sparse_array_int32_t* sa,
        uint32_t sa_col,
        uint32_t nb_elts_read){
    bool ret;
    ret = sparse_array_int32_read(sa,
                                      sa_col, dwt->win_l_x0,
                                      sa_col + nb_elts_read, dwt->win_l_x1,
                                      (int32_t*)(dwt->wavelet + dwt->cas + 2 * dwt->win_l_x0),
                                      1, 8, true);
    assert(ret);
    ret = sparse_array_int32_read(sa,
                                      sa_col, (uint32_t)dwt->sn + dwt->win_h_x0,
                                      sa_col + nb_elts_read, (uint32_t)dwt->sn + dwt->win_h_x1,
                                      (int32_t*)(dwt->wavelet + 1 - dwt->cas + 2 * dwt->win_h_x0),
                                      1, 8, true);
    assert(ret);
    GRK_UNUSED(ret);
}

#ifdef __SSE__
static void decode_step1_sse_97(v4_data* w,
                                       uint32_t start,
                                       uint32_t end,
                                       const __m128 c){
    __m128* restrict vw = (__m128*) w;
    uint32_t i;
    /* 4x unrolled loop */
    vw += 2 * start;
    for (i = start; i + 3 < end; i += 4, vw += 8) {
        __m128 xmm0 = _mm_mul_ps(vw[0], c);
        __m128 xmm2 = _mm_mul_ps(vw[2], c);
        __m128 xmm4 = _mm_mul_ps(vw[4], c);
        __m128 xmm6 = _mm_mul_ps(vw[6], c);
        vw[0] = xmm0;
        vw[2] = xmm2;
        vw[4] = xmm4;
        vw[6] = xmm6;
    }
    for (; i < end; ++i, vw += 2) {
        vw[0] = _mm_mul_ps(vw[0], c);
    }
}

static void decode_step2_sse_97(v4_data* l, v4_data* w,
                                       uint32_t start,
                                       uint32_t end,
                                       uint32_t m,
                                       __m128 c){
    __m128* restrict vl = (__m128*) l;
    __m128* restrict vw = (__m128*) w;
    uint32_t i;
    uint32_t imax = min<uint32_t>(end, m);
    __m128 tmp1, tmp2, tmp3;
    if (start == 0) {
        tmp1 = vl[0];
    } else {
        vw += start * 2;
        tmp1 = vw[-3];
    }

    i = start;

    /* 4x loop unrolling */
    for (; i + 3 < imax; i += 4) {
        __m128 tmp4, tmp5, tmp6, tmp7, tmp8, tmp9;
        tmp2 = vw[-1];
        tmp3 = vw[ 0];
        tmp4 = vw[ 1];
        tmp5 = vw[ 2];
        tmp6 = vw[ 3];
        tmp7 = vw[ 4];
        tmp8 = vw[ 5];
        tmp9 = vw[ 6];
        vw[-1] = _mm_add_ps(tmp2, _mm_mul_ps(_mm_add_ps(tmp1, tmp3), c));
        vw[ 1] = _mm_add_ps(tmp4, _mm_mul_ps(_mm_add_ps(tmp3, tmp5), c));
        vw[ 3] = _mm_add_ps(tmp6, _mm_mul_ps(_mm_add_ps(tmp5, tmp7), c));
        vw[ 5] = _mm_add_ps(tmp8, _mm_mul_ps(_mm_add_ps(tmp7, tmp9), c));
        tmp1 = tmp9;
        vw += 8;
    }

    for (; i < imax; ++i) {
        tmp2 = vw[-1];
        tmp3 = vw[ 0];
        vw[-1] = _mm_add_ps(tmp2, _mm_mul_ps(_mm_add_ps(tmp1, tmp3), c));
        tmp1 = tmp3;
        vw += 2;
    }
    if (m < end) {
        assert(m + 1 == end);
        c = _mm_add_ps(c, c);
        c = _mm_mul_ps(c, vw[-2]);
        vw[-1] = _mm_add_ps(vw[-1], c);
    }
}
#else
static void decode_step1_97(v4_data* w,
                                   uint32_t start,
                                   uint32_t end,
                                   const float c){
    float* restrict fw = (float*) w;
    uint32_t i;
    for (i = start; i < end; ++i) {
        float tmp1 = fw[i * 8    ];
        float tmp2 = fw[i * 8 + 1];
        float tmp3 = fw[i * 8 + 2];
        float tmp4 = fw[i * 8 + 3];
        fw[i * 8    ] = tmp1 * c;
        fw[i * 8 + 1] = tmp2 * c;
        fw[i * 8 + 2] = tmp3 * c;
        fw[i * 8 + 3] = tmp4 * c;
    }
}
static void decode_step2_97(v4_data* l, v4_data* w,
                                   uint32_t start,
                                   uint32_t end,
                                   uint32_t m,
                                   float c){
    float* fl = (float*) l;
    float* fw = (float*) w;
    uint32_t i;
    uint32_t imax = min<uint32_t>(end, m);
    if (start > 0) {
        fw += 8 * start;
        fl = fw - 8;
    }
    for (i = start; i < imax; ++i) {
        float tmp1_1 = fl[0];
        float tmp1_2 = fl[1];
        float tmp1_3 = fl[2];
        float tmp1_4 = fl[3];
        float tmp2_1 = fw[-4];
        float tmp2_2 = fw[-3];
        float tmp2_3 = fw[-2];
        float tmp2_4 = fw[-1];
        float tmp3_1 = fw[0];
        float tmp3_2 = fw[1];
        float tmp3_3 = fw[2];
        float tmp3_4 = fw[3];
        fw[-4] = tmp2_1 + ((tmp1_1 + tmp3_1) * c);
        fw[-3] = tmp2_2 + ((tmp1_2 + tmp3_2) * c);
        fw[-2] = tmp2_3 + ((tmp1_3 + tmp3_3) * c);
        fw[-1] = tmp2_4 + ((tmp1_4 + tmp3_4) * c);
        fl = fw;
        fw += 8;
    }
    if (m < end) {
        assert(m + 1 == end);
        c += c;
        fw[-4] = fw[-4] + fl[0] * c;
        fw[-3] = fw[-3] + fl[1] * c;
        fw[-2] = fw[-2] + fl[2] * c;
        fw[-1] = fw[-1] + fl[3] * c;
    }
}
#endif

/* <summary>                             */
/* Inverse 9-7 wavelet transform in 1-D. */
/* </summary>                            */
static void decode_step_97(dwt_data_97* restrict dwt)
{
    int32_t a, b;

    if (dwt->cas == 0) {
        if (!((dwt->dn > 0) || (dwt->sn > 1))) {
            return;
        }
        a = 0;
        b = 1;
    } else {
        if (!((dwt->sn > 0) || (dwt->dn > 1))) {
            return;
        }
        a = 1;
        b = 0;
    }
#ifdef __SSE__
    decode_step1_sse_97(dwt->wavelet + a, dwt->win_l_x0, dwt->win_l_x1,
                               _mm_set1_ps(K));
    decode_step1_sse_97(dwt->wavelet + b, dwt->win_h_x0, dwt->win_h_x1,
                               _mm_set1_ps(c13318));
    decode_step2_sse_97(dwt->wavelet + b, dwt->wavelet + a + 1,
                               dwt->win_l_x0, dwt->win_l_x1,
                               (uint32_t)min<int32_t>(dwt->sn, dwt->dn - a),
                               _mm_set1_ps(dwt_delta));
    decode_step2_sse_97(dwt->wavelet + a, dwt->wavelet + b + 1,
                               dwt->win_h_x0, dwt->win_h_x1,
                               (uint32_t)min<int32_t>(dwt->dn, dwt->sn - b),
                               _mm_set1_ps(dwt_gamma));
    decode_step2_sse_97(dwt->wavelet + b, dwt->wavelet + a + 1,
                               dwt->win_l_x0, dwt->win_l_x1,
                               (uint32_t)min<int32_t>(dwt->sn, dwt->dn - a),
                               _mm_set1_ps(dwt_beta));
    decode_step2_sse_97(dwt->wavelet + a, dwt->wavelet + b + 1,
                               dwt->win_h_x0, dwt->win_h_x1,
                               (uint32_t)min<int32_t>(dwt->dn, dwt->sn - b),
                               _mm_set1_ps(dwt_alpha));
#else
    decode_step1_97(dwt->wavelet + a, dwt->win_l_x0, dwt->win_l_x1,
                           K);
    decode_step1_97(dwt->wavelet + b, dwt->win_h_x0, dwt->win_h_x1,
                           c13318);
    decode_step2_97(dwt->wavelet + b, dwt->wavelet + a + 1,
                           dwt->win_l_x0, dwt->win_l_x1,
                           (uint32_t)min<int32_t>(dwt->sn, dwt->dn - a),
                           dwt_delta);
    decode_step2_97(dwt->wavelet + a, dwt->wavelet + b + 1,
                           dwt->win_h_x0, dwt->win_h_x1,
                           (uint32_t)min<int32_t>(dwt->dn, dwt->sn - b),
                           dwt_gamma);
    decode_step2_97(dwt->wavelet + b, dwt->wavelet + a + 1,
                           dwt->win_l_x0, dwt->win_l_x1,
                           (uint32_t)min<int32_t>(dwt->sn, dwt->dn - a),
                           dwt_beta);
    decode_step2_97(dwt->wavelet + a, dwt->wavelet + b + 1,
                           dwt->win_h_x0, dwt->win_h_x1,
                           (uint32_t)min<int32_t>(dwt->dn, dwt->sn - b),
                           dwt_alpha);
#endif
}


/* <summary>                             */
/* Inverse 9-7 wavelet transform in 2-D. */
/* </summary>                            */
static
bool decode_tile_97(TileComponent* restrict tilec,uint32_t numres){
    if (numres == 1U)
        return true;

    grk_tcd_resolution* res = tilec->resolutions;
    /* width of the resolution level computed */
    uint32_t rw = (uint32_t)(res->x1 - res->x0);
    /* height of the resolution level computed */
    uint32_t rh = (uint32_t)(res->y1 - res->y0);
    uint32_t w = (uint32_t)(tilec->resolutions[tilec->minimum_num_resolutions - 1].x1 -
                            tilec->resolutions[tilec->minimum_num_resolutions - 1].x0);

    size_t l_data_size = max_resolution(res, numres);
    /* overflow check */
    if (l_data_size > (SIZE_MAX - 5U)) {
        /* FIXME event manager error callback */
        return false;
    }
    l_data_size += 5U;
    /* overflow check */
    if (l_data_size > (SIZE_MAX / sizeof(v4_data))) {
        /* FIXME event manager error callback */
        return false;
    }
    dwt_data_97 h;
    dwt_data_97 v;
    h.wavelet = (v4_data*) grok_aligned_malloc(l_data_size * sizeof(v4_data));
    if (!h.wavelet) {
        /* FIXME event manager error callback */
        return false;
    }
    v.wavelet = h.wavelet;
    while (--numres) {
        h.sn = (int32_t)rw;
        v.sn = (int32_t)rh;
        ++res;
        /* width of the resolution level computed */
        rw = (uint32_t)(res->x1 -  res->x0);
        /* height of the resolution level computed */
        rh = (uint32_t)(res->y1 -  res->y0);
        h.dn = (int32_t)(rw - (uint32_t)h.sn);
        h.cas = res->x0 % 2;
        h.win_l_x0 = 0;
        h.win_l_x1 = (uint32_t)h.sn;
        h.win_h_x0 = 0;
        h.win_h_x1 = (uint32_t)h.dn;
        uint32_t j;
        float * restrict tiledp = (float*) tilec->buf->get_ptr( 0, 0, 0, 0);
        for (j = 0; j + 3 < rh; j += 4) {
            interleave_h_97(&h, tiledp, w, rh - j);
            decode_step_97(&h);
            for (uint32_t k = 0; k < rw; k++) {
                tiledp[k      ] 			= h.wavelet[k].f[0];
                tiledp[k + (size_t)w  ] 	= h.wavelet[k].f[1];
                tiledp[k + (size_t)w * 2] 	= h.wavelet[k].f[2];
                tiledp[k + (size_t)w * 3] 	= h.wavelet[k].f[3];
            }
            tiledp += w * 4;
        }
        if (j < rh) {
            interleave_h_97(&h, tiledp, w, rh - j);
            decode_step_97(&h);
            for (uint32_t k = 0; k < rw; k++) {
                switch (rh - j) {
                case 3:
                    tiledp[k + (size_t)w * 2] = h.wavelet[k].f[2];
                /* FALLTHRU */
                case 2:
                    tiledp[k + (size_t)w  ] = h.wavelet[k].f[1];
                /* FALLTHRU */
                case 1:
                    tiledp[k] = h.wavelet[k].f[0];
                }
            }
        }
        v.dn = (int32_t)(rh - v.sn);
        v.cas = res->y0 % 2;
        v.win_l_x0 = 0;
        v.win_l_x1 = (uint32_t)v.sn;
        v.win_h_x0 = 0;
        v.win_h_x1 = (uint32_t)v.dn;
        tiledp = (float*) tilec->buf->get_ptr( 0, 0, 0, 0);
        for (j = rw; j > 3; j -= 4) {
            interleave_v_97(&v, tiledp, w, 4);
            decode_step_97(&v);
            for (uint32_t k = 0; k < rh; ++k)
                memcpy(&tiledp[k * (size_t)w], &v.wavelet[k], 4 * sizeof(float));
             tiledp += 4;
        }
        if (rw & 0x03) {
            j = rw & 0x03;
            interleave_v_97(&v, tiledp, w, j);
            decode_step_97(&v);
            for (uint32_t k = 0; k < rh; ++k)
                memcpy(&tiledp[k * (size_t)w], &v.wavelet[k],(size_t)j * sizeof(float));
        }
    }
    grok_aligned_free(h.wavelet);

    return true;
}

static
bool decode_partial_tile_97(TileComponent* restrict tilec,
                                   uint32_t numres)
{
    sparse_array_int32_t* sa;
    dwt_data_97 h;
    dwt_data_97 v;
    uint32_t resno;
    /* This value matches the maximum left/right extension given in tables */
    /* F.2 and F.3 of the standard. Note: in tcd_is_subband_area_of_interest() */
    /* we currently use 3. */
    const uint32_t filter_width = 4U;

    grk_tcd_resolution* tr = tilec->resolutions;
    grk_tcd_resolution* tr_max = &(tilec->resolutions[numres - 1]);

    uint32_t rw = (uint32_t)(tr->x1 -
                                 tr->x0);    /* width of the resolution level computed */
    uint32_t rh = (uint32_t)(tr->y1 -
                                 tr->y0);    /* height of the resolution level computed */

    size_t l_data_size;

    /* Compute the intersection of the area of interest, expressed in tile coordinates */
    /* with the tile coordinates */
    auto dim = tilec->buf->unreduced_image_dim;

    uint32_t win_tcx0 = (uint32_t)dim.x0;
    uint32_t win_tcy0 = (uint32_t)dim.y0;
    uint32_t win_tcx1 = (uint32_t)dim.x1;
    uint32_t win_tcy1 = (uint32_t)dim.y1;

    if (tr_max->x0 == tr_max->x1 || tr_max->y0 == tr_max->y1) {
        return true;
    }

    sa = init_sparse_array(tilec, numres);
    if (sa == NULL) {
        return false;
    }

    if (numres == 1U) {
        bool ret = sparse_array_int32_read(sa,
                       tr_max->win_x0 - (uint32_t)tr_max->x0,
                       tr_max->win_y0 - (uint32_t)tr_max->y0,
                       tr_max->win_x1 - (uint32_t)tr_max->x0,
                       tr_max->win_y1 - (uint32_t)tr_max->y0,
					   tilec->buf->get_ptr(0,0,0,0),
                       1, tr_max->win_x1 - tr_max->win_x0,
                       true);
        assert(ret);
        GRK_UNUSED(ret);
        sparse_array_int32_free(sa);
        return true;
    }

    l_data_size = max_resolution(tr, numres);
    /* overflow check */
    if (l_data_size > (SIZE_MAX - 5U)) {
        /* FIXME event manager error callback */
        sparse_array_int32_free(sa);
        return false;
    }
    l_data_size += 5U;
    /* overflow check */
    if (l_data_size > (SIZE_MAX / sizeof(v4_data))) {
        /* FIXME event manager error callback */
        sparse_array_int32_free(sa);
        return false;
    }
    h.wavelet = (v4_data*) grok_aligned_malloc(l_data_size * sizeof(v4_data));
    if (!h.wavelet) {
        /* FIXME event manager error callback */
        sparse_array_int32_free(sa);
        return false;
    }
    v.wavelet = h.wavelet;

    for (resno = 1; resno < numres; resno ++) {
        uint32_t j;
        /* Window of interest subband-based coordinates */
        uint32_t win_ll_x0, win_ll_y0, win_ll_x1, win_ll_y1;
        uint32_t win_hl_x0, win_hl_x1;
        uint32_t win_lh_y0, win_lh_y1;
        /* Window of interest tile-resolution-based coordinates */
        uint32_t win_tr_x0, win_tr_x1, win_tr_y0, win_tr_y1;
        /* Tile-resolution subband-based coordinates */
        uint32_t tr_ll_x0, tr_ll_y0, tr_hl_x0, tr_lh_y0;

        ++tr;

        h.sn = (int32_t)rw;
        v.sn = (int32_t)rh;

        rw = (uint32_t)(tr->x1 - tr->x0);
        rh = (uint32_t)(tr->y1 - tr->y0);

        h.dn = (int32_t)(rw - (uint32_t)h.sn);
        h.cas = tr->x0 % 2;

        v.dn = (int32_t)(rh - (uint32_t)v.sn);
        v.cas = tr->y0 % 2;

        /* Get the subband coordinates for the window of interest */
        /* LL band */
        get_band_coordinates(tilec, resno, 0,
                                     win_tcx0, win_tcy0, win_tcx1, win_tcy1,
                                     &win_ll_x0, &win_ll_y0,
                                     &win_ll_x1, &win_ll_y1);
        /* HL band */
        get_band_coordinates(tilec, resno, 1,
                                     win_tcx0, win_tcy0, win_tcx1, win_tcy1,
                                     &win_hl_x0, NULL, &win_hl_x1, NULL);
        /* LH band */
        get_band_coordinates(tilec, resno, 2,
                                     win_tcx0, win_tcy0, win_tcx1, win_tcy1,
                                     NULL, &win_lh_y0, NULL, &win_lh_y1);

        /* Beware: band index for non-LL0 resolution are 0=HL, 1=LH and 2=HH */
        tr_ll_x0 = (uint32_t)tr->bands[1].x0;
        tr_ll_y0 = (uint32_t)tr->bands[0].y0;
        tr_hl_x0 = (uint32_t)tr->bands[0].x0;
        tr_lh_y0 = (uint32_t)tr->bands[1].y0;

        /* Subtract the origin of the bands for this tile, to the subwindow */
        /* of interest band coordinates, so as to get them relative to the */
        /* tile */
        win_ll_x0 = uint_subs(win_ll_x0, tr_ll_x0);
        win_ll_y0 = uint_subs(win_ll_y0, tr_ll_y0);
        win_ll_x1 = uint_subs(win_ll_x1, tr_ll_x0);
        win_ll_y1 = uint_subs(win_ll_y1, tr_ll_y0);
        win_hl_x0 = uint_subs(win_hl_x0, tr_hl_x0);
        win_hl_x1 = uint_subs(win_hl_x1, tr_hl_x0);
        win_lh_y0 = uint_subs(win_lh_y0, tr_lh_y0);
        win_lh_y1 = uint_subs(win_lh_y1, tr_lh_y0);

        segment_grow(filter_width, (uint32_t)h.sn, &win_ll_x0, &win_ll_x1);
        segment_grow(filter_width, (uint32_t)h.dn, &win_hl_x0, &win_hl_x1);

        segment_grow(filter_width, (uint32_t)v.sn, &win_ll_y0, &win_ll_y1);
        segment_grow(filter_width, (uint32_t)v.dn, &win_lh_y0, &win_lh_y1);

        /* Compute the tile-resolution-based coordinates for the window of interest */
        if (h.cas == 0) {
            win_tr_x0 = min<uint32_t>(2 * win_ll_x0, 2 * win_hl_x0 + 1);
            win_tr_x1 = min<uint32_t>(max<uint32_t>(2 * win_ll_x1, 2 * win_hl_x1 + 1), rw);
        } else {
            win_tr_x0 = min<uint32_t>(2 * win_hl_x0, 2 * win_ll_x0 + 1);
            win_tr_x1 = min<uint32_t>(max<uint32_t>(2 * win_hl_x1, 2 * win_ll_x1 + 1), rw);
        }
        if (v.cas == 0) {
            win_tr_y0 = min<uint32_t>(2 * win_ll_y0, 2 * win_lh_y0 + 1);
            win_tr_y1 = min<uint32_t>(max<uint32_t>(2 * win_ll_y1, 2 * win_lh_y1 + 1), rh);
        } else {
            win_tr_y0 = min<uint32_t>(2 * win_lh_y0, 2 * win_ll_y0 + 1);
            win_tr_y1 = min<uint32_t>(max<uint32_t>(2 * win_lh_y1, 2 * win_ll_y1 + 1), rh);
        }
        h.win_l_x0 = win_ll_x0;
        h.win_l_x1 = win_ll_x1;
        h.win_h_x0 = win_hl_x0;
        h.win_h_x1 = win_hl_x1;
        for (j = 0; j + 3 < rh; j += 4) {
            if ((j + 3 >= win_ll_y0 && j < win_ll_y1) ||
                    (j + 3 >= win_lh_y0 + (uint32_t)v.sn &&
                     j < win_lh_y1 + (uint32_t)v.sn)) {
                interleave_partial_h_97(&h, sa, j, min<uint32_t>(4U, rh - j));
                decode_step_97(&h);
                if (!sparse_array_int32_write(sa,
                                                  win_tr_x0, j,
                                                  win_tr_x1, j + 4,
                                                  (int32_t*)&h.wavelet[win_tr_x0].f[0],
                                                  4, 1, true)) {
                    /* FIXME event manager error callback */
                    sparse_array_int32_free(sa);
                    grok_aligned_free(h.wavelet);
                    return false;
                }
            }
        }
        if (j < rh &&
                ((j + 3 >= win_ll_y0 && j < win_ll_y1) ||
                 (j + 3 >= win_lh_y0 + (uint32_t)v.sn &&
                  j < win_lh_y1 + (uint32_t)v.sn))) {
            interleave_partial_h_97(&h, sa, j, rh - j);
            decode_step_97(&h);
            if (!sparse_array_int32_write(sa,
                                              win_tr_x0, j,
                                              win_tr_x1, rh,
                                              (int32_t*)&h.wavelet[win_tr_x0].f[0],
                                              4, 1, true)) {
                /* FIXME event manager error callback */
                sparse_array_int32_free(sa);
                grok_aligned_free(h.wavelet);
                return false;
            }
        }
        v.win_l_x0 = win_ll_y0;
        v.win_l_x1 = win_ll_y1;
        v.win_h_x0 = win_lh_y0;
        v.win_h_x1 = win_lh_y1;
        for (j = win_tr_x0; j < win_tr_x1; j += 4) {
            uint32_t nb_elts = min<uint32_t>(4U, win_tr_x1 - j);

            interleave_partial_v_97(&v, sa, j, nb_elts);
            decode_step_97(&v);
            if (!sparse_array_int32_write(sa,
                                              j, win_tr_y0,
                                              j + nb_elts, win_tr_y1,
                                              (int32_t*)&h.wavelet[win_tr_y0].f[0],
                                              1, 4, true)) {
                /* FIXME event manager error callback */
                sparse_array_int32_free(sa);
                grok_aligned_free(h.wavelet);
                return false;
            }
        }
    }
	bool ret = sparse_array_int32_read(sa,
				   tr_max->win_x0 - (uint32_t)tr_max->x0,
				   tr_max->win_y0 - (uint32_t)tr_max->y0,
				   tr_max->win_x1 - (uint32_t)tr_max->x0,
				   tr_max->win_y1 - (uint32_t)tr_max->y0,
				   tilec->buf->get_ptr(0,0,0,0),
				   1, tr_max->win_x1 - tr_max->win_x0,
				   true);
	assert(ret);
	GRK_UNUSED(ret);
    sparse_array_int32_free(sa);
    grok_aligned_free(h.wavelet);

    return true;
}

bool decode_97(TileProcessor *p_tcd,
                             TileComponent* restrict tilec,
                             uint32_t numres){
    if (p_tcd->whole_tile_decoding) {
        return decode_tile_97(tilec, numres);
    } else {
        return decode_partial_tile_97(tilec, numres);
    }
}

}
