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
 * Copyright (c) 2008, Jerome Fimes, Communications & Systemes <jerome.fimes@c-s.fr>
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

#include "opj_includes.h"

#include <assert.h>

static void opj_mqc_byteout(opj_mqc_t *mqc);
static void opj_mqc_renorme(opj_mqc_t *mqc);
static void opj_mqc_codemps(opj_mqc_t *mqc);
static void opj_mqc_codelps(opj_mqc_t *mqc);
static void opj_mqc_setbits(opj_mqc_t *mqc);
const opj_mqc_state_t mqc_states[47 * 2] = {
    {0x5601, 2, 3},
    {0x5601, 3, 2},
    {0x3401, 4, 12},
    {0x3401, 5, 13},
    {0x1801, 6, 18},
    {0x1801, 7, 19},
    {0x0ac1, 8, 24},
    {0x0ac1, 9, 25},
    {0x0521, 10, 58},
    {0x0521, 11, 59},
    {0x0221, 76, 66},
    {0x0221, 77, 67},
    {0x5601, 14, 13},
    {0x5601, 15, 12},
    {0x5401, 16, 28},
    {0x5401, 17, 29},
    {0x4801, 18, 28},
    {0x4801, 19, 29},
    {0x3801, 20, 28},
    {0x3801, 21, 29},
    {0x3001, 22, 34},
    {0x3001, 23, 35},
    {0x2401, 24, 36},
    {0x2401, 25, 37},
    {0x1c01, 26, 40},
    {0x1c01, 27, 41},
    {0x1601, 58, 42},
    {0x1601, 59, 43},
    {0x5601, 30, 29},
    {0x5601, 31, 28},
    {0x5401, 32, 28},
    {0x5401, 33, 29},
    {0x5101, 34, 30},
    {0x5101, 35, 31},
    {0x4801, 36, 32},
    {0x4801, 37, 33},
    {0x3801, 38, 34},
    {0x3801, 39, 35},
    {0x3401, 40, 36},
    {0x3401, 41, 37},
    {0x3001, 42, 38},
    {0x3001, 43, 39},
    {0x2801, 44, 38},
    {0x2801, 45, 39},
    {0x2401, 46, 40},
    {0x2401, 47, 41},
    {0x2201, 48, 42},
    {0x2201, 49, 43},
    {0x1c01, 50, 44},
    {0x1c01, 51, 45},
    {0x1801, 52, 46},
    {0x1801, 53, 47},
    {0x1601, 54, 48},
    {0x1601, 55, 49},
    {0x1401, 56, 50},
    {0x1401, 57, 51},
    {0x1201, 58, 52},
    {0x1201, 59, 53},
    {0x1101, 60, 54},
    {0x1101, 61, 55},
    {0x0ac1, 62, 56},
    {0x0ac1, 63, 57},
    {0x09c1, 64, 58},
    {0x09c1, 65, 59},
    {0x08a1, 66, 60},
    {0x08a1, 67, 61},
    {0x0521, 68, 62},
    {0x0521, 69, 63},
    {0x0441, 70, 64},
    {0x0441, 71, 65},
    {0x02a1, 72, 66},
    {0x02a1, 73, 67},
    {0x0221, 74, 68},
    {0x0221, 75, 69},
    {0x0141, 76, 70},
    {0x0141, 77, 71},
    {0x0111, 78, 72},
    {0x0111, 79, 73},
    {0x0085, 80, 74},
    {0x0085, 81, 75},
    {0x0049, 82, 76},
    {0x0049, 83, 77},
    {0x0025, 84, 78},
    {0x0025, 85, 79},
    {0x0015, 86, 80},
    {0x0015, 87, 81},
    {0x0009, 88, 82},
    {0x0009, 89, 83},
    {0x0005, 90, 84},
    {0x0005, 91, 85},
    {0x0001, 90, 86},
    {0x0001, 91, 87},
    {0x5601, 92, 92},
    {0x5601, 93, 93},
};
static void opj_mqc_byteout(opj_mqc_t *mqc)
{
    /* bp is initialized to start - 1 in opj_mqc_init_enc() */
    /* but this is safe, see opj_tcd_code_block_enc_allocate_data() */
    assert(mqc->bp >= mqc->start - 1);
    if (*mqc->bp == 0xff) {
        mqc->bp++;
        *mqc->bp = (uint8_t)(mqc->c >> 20);
        mqc->c &= 0xfffff;
        mqc->ct = 7;
    } else {
        if ((mqc->c & 0x8000000) == 0) {
            mqc->bp++;
            *mqc->bp = (uint8_t)(mqc->c >> 19);
            mqc->c &= 0x7ffff;
            mqc->ct = 8;
        } else {
            (*mqc->bp)++;
            if (*mqc->bp == 0xff) {
                mqc->c &= 0x7ffffff;
                mqc->bp++;
                *mqc->bp = (uint8_t)(mqc->c >> 20);
                mqc->c &= 0xfffff;
                mqc->ct = 7;
            } else {
                mqc->bp++;
                *mqc->bp = (uint8_t)(mqc->c >> 19);
                mqc->c &= 0x7ffff;
                mqc->ct = 8;
            }
        }
    }
}

static void opj_mqc_renorme(opj_mqc_t *mqc)
{
    do {
        mqc->a <<= 1;
        mqc->c <<= 1;
        mqc->ct--;
        if (mqc->ct == 0) {
            opj_mqc_byteout(mqc);
        }
    } while ((mqc->a & 0x8000) == 0);
}

static void opj_mqc_codemps(opj_mqc_t *mqc)
{
	auto state = (mqc_states + (*mqc->curctx));
    mqc->a -=  state->qeval;
    if ((mqc->a & 0x8000) == 0) {
        if (mqc->a < state->qeval) {
            mqc->a = state->qeval;
        } else {
            mqc->c += state->qeval;
        }
        *mqc->curctx = state->nmps;
        opj_mqc_renorme(mqc);
    } else {
        mqc->c += state->qeval;
    }
}

static void opj_mqc_codelps(opj_mqc_t *mqc)
{
	auto state = (mqc_states + (*mqc->curctx));
    mqc->a -= state->qeval;
    if (mqc->a < state->qeval) {
        mqc->c += state->qeval;
    } else {
        mqc->a = state->qeval;
    }
    *mqc->curctx = state->nlps;
    opj_mqc_renorme(mqc);
}

static void opj_mqc_setbits(opj_mqc_t *mqc)
{
    uint32_t tempc = mqc->c + mqc->a;
    mqc->c |= 0xffff;
    if (mqc->c >= tempc) {
        mqc->c -= 0x8000;
    }
}
uint32_t opj_mqc_numbytes(opj_mqc_t *mqc)
{
    const ptrdiff_t diff = mqc->bp - mqc->start;
#if 0
    assert(diff <= 0xffffffff && diff >= 0);   /* UINT32_MAX */
#endif
    return (uint32_t)diff;
}

void opj_mqc_init_enc(opj_mqc_t *mqc, uint8_t *bp)
{
    /* To avoid the curctx pointer to be dangling, but not strictly */
    /* required as the current context is always set before encoding */
    opj_mqc_setcurctx(mqc, 0);

    /* As specified in Figure C.10 - Initialization of the encoder */
    /* (C.2.8 Initialization of the encoder (INITENC)) */
    mqc->a = 0x8000;
    mqc->c = 0;
    /* Yes, we point before the start of the buffer, but this is safe */
    /* given opj_tcd_code_block_enc_allocate_data() */
    mqc->bp = bp - 1;
    mqc->ct = 12;
    /* At this point we should test *(mqc->bp) against 0xFF, but this is not */
    /* necessary, as this is only used at the beginning of the code block */
    /* and our initial fake byte is set at 0 */
    assert(*(mqc->bp) != 0xff);

    mqc->start = bp;
    mqc->end_of_byte_stream_counter = 0;
}

void opj_mqc_encode(opj_mqc_t *mqc, uint32_t d)
{
    if ((*mqc->curctx & 1) == d) {
        opj_mqc_codemps(mqc);
    } else {
        opj_mqc_codelps(mqc);
    }
}

void opj_mqc_flush(opj_mqc_t *mqc)
{
    /* C.2.9 Termination of coding (FLUSH) */
    /* Figure C.11 – FLUSH procedure */
    opj_mqc_setbits(mqc);
    mqc->c <<= mqc->ct;
    opj_mqc_byteout(mqc);
    mqc->c <<= mqc->ct;
    opj_mqc_byteout(mqc);

    /* It is forbidden that a coding pass ends with 0xff */
    if (*mqc->bp != 0xff) {
        /* Advance pointer so that opj_mqc_numbytes() returns a valid value */
        mqc->bp++;
    }
}

#define BYPASS_CT_INIT  0xF

void opj_mqc_bypass_init_enc(opj_mqc_t *mqc)
{
    /* This function is normally called after at least one opj_mqc_flush() */
    /* which will have advance mqc->bp by at least 2 bytes beyond its */
    /* initial position */
    assert(mqc->bp >= mqc->start);
    mqc->c = 0;
    /* in theory we should initialize to 8, but use this special value */
    /* as a hint that opj_mqc_bypass_enc() has never been called, so */
    /* as to avoid the 0xff 0x7f elimination trick in opj_mqc_bypass_flush_enc() */
    /* to trigger when we don't have output any bit during this bypass sequence */
    /* Any value > 8 will do */
    mqc->ct = BYPASS_CT_INIT;
    /* Given that we are called after opj_mqc_flush(), the previous byte */
    /* cannot be 0xff. */
    assert(mqc->bp[-1] != 0xff);
}

void opj_mqc_bypass_enc(opj_mqc_t *mqc, uint32_t d)
{
    if (mqc->ct == BYPASS_CT_INIT) {
        mqc->ct = 8;
    }
    mqc->ct--;
    mqc->c = mqc->c + (d << mqc->ct);
    if (mqc->ct == 0) {
        *mqc->bp = (uint8_t)mqc->c;
        mqc->ct = 8;
        /* If the previous byte was 0xff, make sure that the next msb is 0 */
        if (*mqc->bp == 0xff) {
            mqc->ct = 7;
        }
        mqc->bp++;
        mqc->c = 0;
    }
}

uint32_t opj_mqc_bypass_get_extra_bytes(opj_mqc_t *mqc, bool erterm)
{
    return (mqc->ct < 7 ||
            (mqc->ct == 7 && (erterm || mqc->bp[-1] != 0xff))) ? (1 + 1) : (0 + 1);
}

void opj_mqc_bypass_flush_enc(opj_mqc_t *mqc, bool erterm)
{
    /* Is there any bit remaining to be flushed ? */
    /* If the last output byte is 0xff, we can discard it, unless */
    /* erterm is required (I'm not completely sure why in erterm */
    /* we must output 0xff 0x2a if the last byte was 0xff instead of */
    /* discarding it, but Kakadu requires it when decoding */
    /* in -fussy mode) */
    if (mqc->ct < 7 || (mqc->ct == 7 && (erterm || mqc->bp[-1] != 0xff))) {
        uint8_t bit_value = 0;
        /* If so, fill the remaining lsbs with an alternating sequence of */
        /* 0,1,... */
        /* Note: it seems the standard only requires that for a ERTERM flush */
        /* and doesn't specify what to do for a regular BYPASS flush */
        while (mqc->ct > 0) {
            mqc->ct--;
            mqc->c += (uint32_t)(bit_value << mqc->ct);
            bit_value = (uint8_t)(1U - bit_value);
        }
        *mqc->bp = (uint8_t)mqc->c;
        /* Advance pointer so that opj_mqc_numbytes() returns a valid value */
        mqc->bp++;
    } else if (mqc->ct == 7 && mqc->bp[-1] == 0xff) {
        /* Discard last 0xff */
        assert(!erterm);
        mqc->bp --;
    } else if (mqc->ct == 8 && !erterm &&
               mqc->bp[-1] == 0x7f && mqc->bp[-2] == 0xff) {
        /* Tiny optimization: discard terminating 0xff 0x7f since it is */
        /* interpreted as 0xff 0x7f [0xff 0xff] by the decoder, and given */
        /* the bit stuffing, in fact as 0xff 0xff [0xff ..] */
        /* Happens once on opj_compress -i ../MAPA.tif -o MAPA.j2k  -M 1 */
        mqc->bp -= 2;
    }

    assert(mqc->bp[-1] != 0xff);
}

void opj_mqc_reset_enc(opj_mqc_t *mqc)
{
    opj_mqc_resetstates(mqc);
    opj_mqc_setstate(mqc, T1_CTXNO_UNI, 0, 46);
    opj_mqc_setstate(mqc, T1_CTXNO_AGG, 0, 3);
    opj_mqc_setstate(mqc, T1_CTXNO_ZC, 0, 4);
}
void opj_mqc_restart_init_enc(opj_mqc_t *mqc)
{
    /* <Re-init part> */

    /* As specified in Figure C.10 - Initialization of the encoder */
    /* (C.2.8 Initialization of the encoder (INITENC)) */
    mqc->a = 0x8000;
    mqc->c = 0;
    mqc->ct = 12;
    /* This function is normally called after at least one opj_mqc_flush() */
    /* which will have advance mqc->bp by at least 2 bytes beyond its */
    /* initial position */
    mqc->bp --;
    assert(mqc->bp >= mqc->start - 1);
    assert(*mqc->bp != 0xff);
    if (*mqc->bp == 0xff) {
        mqc->ct = 13;
    }
}

void opj_mqc_erterm_enc(opj_mqc_t *mqc)
{
    int32_t k = (int32_t)(11 - mqc->ct + 1);

    while (k > 0) {
        mqc->c <<= mqc->ct;
        mqc->ct = 0;
        opj_mqc_byteout(mqc);
        k -= (int32_t)mqc->ct;
    }

    if (*mqc->bp != 0xff) {
        opj_mqc_byteout(mqc);
    }
}

void opj_mqc_segmark_enc(opj_mqc_t *mqc)
{
    uint32_t i;
    opj_mqc_setcurctx(mqc, 18);

    for (i = 1; i < 5; i++) {
        opj_mqc_encode(mqc, i % 2);
    }
}

static void opj_mqc_init_dec_common(opj_mqc_t *mqc,
                                    uint8_t *bp,
                                    uint32_t len,
                                    uint32_t extra_writable_bytes)
{
    (void)extra_writable_bytes;

    assert(extra_writable_bytes >= OPJ_COMMON_CBLK_DATA_EXTRA);
    mqc->start = bp;
    mqc->end = bp + len;
    /* Insert an artificial 0xFF 0xFF marker at end of the code block */
    /* data so that the bytein routines stop on it. This saves us comparing */
    /* the bp and end pointers */
    /* But before inserting it, backup th bytes we will overwrite */
    memcpy(mqc->backup, mqc->end, OPJ_COMMON_CBLK_DATA_EXTRA);
    mqc->end[0] = 0xFF;
    mqc->end[1] = 0xFF;
    mqc->bp = bp;
}
void opj_mqc_init_dec(opj_mqc_t *mqc, uint8_t *bp, uint32_t len,
                      uint32_t extra_writable_bytes)
{
    /* Implements ISO 15444-1 C.3.5 Initialization of the decoder (INITDEC) */
    /* Note: alternate "J.1 - Initialization of the software-conventions */
    /* decoder" has been tried, but does */
    /* not bring any improvement. */
    /* See https://github.com/uclouvain/openjpeg/issues/921 */
    opj_mqc_init_dec_common(mqc, bp, len, extra_writable_bytes);
    opj_mqc_setcurctx(mqc, 0);
    mqc->end_of_byte_stream_counter = 0;
    if (len == 0) {
        mqc->c = 0xff << 16;
    } else {
        mqc->c = (uint32_t)(*mqc->bp << 16);
    }

    opj_mqc_bytein(mqc);
    mqc->c <<= 7;
    mqc->ct -= 7;
    mqc->a = 0x8000;
}

void opj_mqc_raw_init_dec(opj_mqc_t *mqc, uint8_t *bp, uint32_t len,
                          uint32_t extra_writable_bytes)
{
    opj_mqc_init_dec_common(mqc, bp, len, extra_writable_bytes);
    mqc->c = 0;
    mqc->ct = 0;
}

void opq_mqc_finish_dec(opj_mqc_t *mqc)
{
    /* Restore the bytes overwritten by opj_mqc_init_dec_common() */
    memcpy(mqc->end, mqc->backup, OPJ_COMMON_CBLK_DATA_EXTRA);
}

void opj_mqc_resetstates(opj_mqc_t *mqc)
{
    uint32_t i;
    for (i = 0; i < MQC_NUMCTXS; i++) {
        mqc->ctxs[i] = 0;
    }
}

void opj_mqc_setstate(opj_mqc_t *mqc, uint32_t ctxno, uint32_t msb,
                      int32_t prob)
{
    mqc->ctxs[ctxno] = msb + (uint32_t)(prob << 1);
}
