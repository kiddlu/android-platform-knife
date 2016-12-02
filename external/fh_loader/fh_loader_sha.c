/**************************************************************************
 *
 * This module contains the software implementation for sha256.
 *
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 *
 *************************************************************************/

/*  from OpenBSD: sha2.c,v 1.11 2005/08/08 08:05:35 espie Exp    */
/*
* FILE:   sha2.c
* AUTHOR:   Aaron D. Gifford <me@aarongifford.com>
*
* Copyright (c) 2000-2001, Aaron D. Gifford
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* 3. Neither the name of the copyright holder nor the names of contributors
* may be used to endorse or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTOR(S) "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTOR(S) BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*
* $From: sha2.c,v 1.1 2001/11/08 00:01:51 adg Exp adg $
*/


/*===========================================================================

                        EDIT HISTORY FOR MODULE

  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.

  $Header: //source/qcom/qct/core/storage/tools/fh_loader/vs2010/fh_loader/fh_loader_sha.c#1 $
  $DateTime: 2016/01/14 18:37:20 $
  $Author: wkimberl $

when         who   what, where, why
----------   ---   ---------------------------------------------------------
2016-01-14   wek   Create. Move SHA functions from security to a new file.

===========================================================================*/

#include "fh_loader_sha.h"
#include <string.h>

#define SHA256_SHORT_BLOCK_LENGTH (SHA256_BLOCK_LENGTH - 8)
#define HSHSHA_COUNTER_SIZE 8
#define SHA256_BLOCK_LENGTH    64
#define Sigma0_256(x) (SHA_S32(2,  (x)) ^ SHA_S32(13, (x)) ^ SHA_S32(22, (x)))
#define Sigma1_256(x) (SHA_S32(6,  (x)) ^ SHA_S32(11, (x)) ^ SHA_S32(25, (x)))
#define sigma0_256(x) (SHA_S32(7,  (x)) ^ SHA_S32(18, (x)) ^ R(3 ,   (x)))
#define sigma1_256(x) (SHA_S32(17, (x)) ^ SHA_S32(19, (x)) ^ R(10,   (x)))

// Initial hash value H for SHA-256:
static const uint32 sha256_initial_hash_value[8] =
{
    0x6a09e667UL,
    0xbb67ae85UL,
    0x3c6ef372UL,
    0xa54ff53aUL,
    0x510e527fUL,
    0x9b05688cUL,
    0x1f83d9abUL,
    0x5be0cd19UL
};

void sechsharm_sha256_init (struct __sechsh_ctx_s* context)
{
    if (NULL == context)
        return;

    memset ( context, 0, sizeof ( struct __sechsh_ctx_s) );

    /* SHA256 initialization constants */
    //memcpy((uint8*)context->iv, sizeof(sha256_initial_hash_value), (uint8*)sha256_initial_hash_value, sizeof(sha256_initial_hash_value));
    memcpy ( (uint8*) context->iv, (uint8*) sha256_initial_hash_value, sizeof (sha256_initial_hash_value) );

    context->counter[0] = 0;
    context->counter[1] = 0;

}

static const uint32 K256[64] =
{
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
    0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
    0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
    0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
    0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
    0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
    0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
    0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
    0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
    0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
    0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

#define Maj(x,y,z)  (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define R(b,x)    ((x) >> (b))

#define BE_8_TO_32(dst, cp) do {        \
      (dst) = (uint32)(cp)[3] | ((uint32)(cp)[2] << 8) |  \
   ((uint32)(cp)[1] << 16) | ((uint32)(cp)[0] << 24); \
   } while(0)


#define Ch(x,y,z) (((x) & (y)) ^ ((~(x)) & (z)))
#define SHA_S32(b,x)  (((x) >> (b)) | ((x) << (32 - (b))))

#define ROUND256_0_TO_15(a,b,c,d,e,f,g,h) do {        \
      BE_8_TO_32(W256[j], data);          \
      data += 4;              \
      T1 = (h) + Sigma1_256((e)) + Ch((e), (f), (g)) + K256[j] + W256[j]; \
      (d) += T1;              \
      (h) = T1 + Sigma0_256((a)) + Maj((a), (b), (c));      \
      j++;                \
   } while(0)

#define ROUND256(a,b,c,d,e,f,g,h) do {          \
      s0 = W256[(j+1)&0x0f];            \
      s0 = sigma0_256(s0);            \
      s1 = W256[(j+14)&0x0f];           \
      s1 = sigma1_256(s1);            \
      T1 = (h) + Sigma1_256((e)) + Ch((e), (f), (g)) + K256[j] +  \
   (W256[j&0x0f] += s1 + W256[(j+9)&0x0f] + s0);      \
      (d) += T1;              \
      (h) = T1 + Sigma0_256((a)) + Maj((a), (b), (c));      \
      j++;                \
   } while(0)

static void sechsharm_sha256_transform
(
    uint32 state[8],
    const uint8 data[SHA256_BLOCK_LENGTH]
)
{
    uint32 a, b, c, d, e, f, g, h, s0, s1;
    uint32 T1, W256[16];
    int    j;

    /* Initialize registers with the prev. intermediate value */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];

    j = 0;

    /* Klocwork: Filter. For the second iteration of the loop j
     * is 15 for ROUND256_0_TO_15(b,c,d,e,f,g,h,a) and is then
     * incremented to 16 at which time it breaks out of the loop.
     */
    do
    {
        /* Rounds 0 to 15 (unrolled): */
        ROUND256_0_TO_15 (a, b, c, d, e, f, g, h);
        ROUND256_0_TO_15 (h, a, b, c, d, e, f, g);
        ROUND256_0_TO_15 (g, h, a, b, c, d, e, f);
        ROUND256_0_TO_15 (f, g, h, a, b, c, d, e);
        ROUND256_0_TO_15 (e, f, g, h, a, b, c, d);
        ROUND256_0_TO_15 (d, e, f, g, h, a, b, c);
        ROUND256_0_TO_15 (c, d, e, f, g, h, a, b);
        ROUND256_0_TO_15 (b, c, d, e, f, g, h, a);
    }
    while (j < 16);

    /* Now for the remaining rounds up to 63: */
    /* Klocwork: Filter. The first time through the loop
     * j is 16 for ROUND256(a,b,c,d,e,f,g,h) and 23
     * for ROUND256(b,c,d,e,f,g,h,a) and the last time
     * through the loop j is 63 for ROUND256(b,c,d,e,f,g,h,a)
     * which will not overflow the array.
     */
    do
    {
        ROUND256 (a, b, c, d, e, f, g, h);
        ROUND256 (h, a, b, c, d, e, f, g);
        ROUND256 (g, h, a, b, c, d, e, f);
        ROUND256 (f, g, h, a, b, c, d, e);

        ROUND256 (e, f, g, h, a, b, c, d);
        ROUND256 (d, e, f, g, h, a, b, c);
        ROUND256 (c, d, e, f, g, h, a, b);
        ROUND256 (b, c, d, e, f, g, h, a);
    }
    while (j < 64);

    /* Compute the current intermediate hash value */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;

    /* Clean up */
    a = b = c = d = e = f = g = h = T1 = 0;
}

#define BE_32_TO_8(cp, src) do {    \
      (cp)[0] = (uint8)((src) >> 24);   \
      (cp)[1] = (uint8)((src) >> 16);   \
      (cp)[2] = (uint8)((src) >> 8);    \
      (cp)[3] = (uint8)((src));     \
   } while (0)


#define BE_64_TO_8(cp, src) do {    \
      (cp)[0] = (uint8)((src) >> 56);   \
      (cp)[1] = (uint8)((src) >> 48);   \
      (cp)[2] = (uint8)((src) >> 40);   \
      (cp)[3] = (uint8)((src) >> 32);   \
      (cp)[4] = (uint8)((src) >> 24);   \
      (cp)[5] = (uint8)((src) >> 16);   \
      (cp)[6] = (uint8)((src) >> 8);    \
      (cp)[7] = (uint8)((src));     \
   } while (0)

void sechsharm_sha256_update
(
    struct __sechsh_ctx_s* context,            /* context */
    uint8*                 leftover,           /* leftover input in context */
    uint32*                leftover_size,
    uint8*                 data,
    uint32                 len
)
{
    uint32 freespace, usedspace;
    uint64    bitcounter;

    /* Calling with no data is valid (we do nothing) */
    if (len == 0)
        return;

    memcpy (&bitcounter, context->counter, HSHSHA_COUNTER_SIZE);
    usedspace = ( (uint32) bitcounter >> 3) % SHA256_BLOCK_LENGTH;

    if (usedspace > 0)
    {
        /* Calculate how much free space is available in the buffer */
        freespace = SHA256_BLOCK_LENGTH - usedspace;

        if (len >= freespace)
        {
            /* Fill the buffer completely and process it */
            memcpy ( (uint8*) &leftover[usedspace], data, freespace);
            bitcounter += (uint32) (freespace << 3);
            len -= freespace;
            data += freespace;
            sechsharm_sha256_transform (context->iv, leftover);
        }
        else
        {
            /* The buffer is not yet full */
            memcpy ( (uint8*) &leftover[usedspace], data, len);
            bitcounter += (uint32) (len << 3);
            /* Clean up: */
            usedspace = freespace = 0;
            memcpy (context->counter, &bitcounter, HSHSHA_COUNTER_SIZE);
            return;
        }
    }

    while (len >= SHA256_BLOCK_LENGTH)
    {
        /* Process as many complete blocks as we can */
        sechsharm_sha256_transform (context->iv, data);
        bitcounter += SHA256_BLOCK_LENGTH << 3;
        len -= SHA256_BLOCK_LENGTH;
        data += SHA256_BLOCK_LENGTH;
    }

    if (len > 0)
    {
        /* There's left-overs, so save 'em */
        memcpy ( (uint8*) leftover, data, len);
        bitcounter += (uint32) (len << 3);
    }

    /* Clean up: */
    usedspace = freespace = 0;
    memcpy (context->counter, &bitcounter, HSHSHA_COUNTER_SIZE);
}

static void sechsharm_sha256_pad
(
    struct __sechsh_ctx_s* context,
    uint8*                leftover           /* leftover input in context */
)
{
    uint32 usedspace;
    uint64 bitcounter;

    memcpy (&bitcounter, context->counter, 8);
    usedspace = (context->counter[0] >> 3) % SHA256_BLOCK_LENGTH;

    if (usedspace > 0)
    {
        /* Begin padding with a 1 bit: */
        leftover[usedspace++] = 0x80;

        if (usedspace <= SHA256_SHORT_BLOCK_LENGTH)
        {
            /* Set-up for the last transform: */
            memset (&leftover[usedspace], 0,
                    SHA256_SHORT_BLOCK_LENGTH - usedspace);
        }
        else
        {
            if (usedspace < SHA256_BLOCK_LENGTH)
            {
                memset (&leftover[usedspace], 0,
                        SHA256_BLOCK_LENGTH - usedspace);
            }

            /* Do second-to-last transform: */
            sechsharm_sha256_transform (context->iv, leftover);

            /* Prepare for last transform: */
            memset (leftover, 0, SHA256_SHORT_BLOCK_LENGTH);
        }
    }
    else
    {
        /* Set-up for the last transform: */
        memset (leftover, 0, SHA256_SHORT_BLOCK_LENGTH);

        /* Begin padding with a 1 bit: */
        *leftover = 0x80;
    }

    /* Store the length of input data (in bits) in big endian format: */
    BE_64_TO_8 (&leftover[SHA256_SHORT_BLOCK_LENGTH], bitcounter);

    /* Final transform: */
    sechsharm_sha256_transform (context->iv, leftover);

    memcpy (context->counter, &bitcounter, HSHSHA_COUNTER_SIZE);
    /* Clean up: */
    usedspace = 0;
}

void sechsharm_sha256_final
(
    struct __sechsh_ctx_s* context,            /* context */
    uint8*                 leftover,           /* leftover input in context */
    uint32*                leftover_size,
    uint8*                 digest
)
{
    sechsharm_sha256_pad (context, leftover);

    /* If no digest buffer is passed, we don't bother doing this: */
    if (digest != NULL)
    {
        int  i;

        /* Convert TO host byte order */
        for (i = 0; i < 8; i++)
            BE_32_TO_8 (digest + i * 4, context->iv[i]);

        /* zeroize context structure */
        //BM: ???
        //our_memset((uint8*)context, 0, sizeof(*context));

        sechsharm_sha256_transform ( context->iv, /*context->*/leftover );
    }
}



