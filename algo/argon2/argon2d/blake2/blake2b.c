/*
 * Argon2 reference source code package - reference C implementations
 *
 * Copyright 2015
 * Daniel Dinu, Dmitry Khovratovich, Jean-Philippe Aumasson, and Samuel Neves
 *
 * You may use this work under the terms of a Creative Commons CC0 1.0
 * License/Waiver or the Apache Public License 2.0, at your option. The terms of
 * these licenses can be found at:
 *
 * - CC0 1.0 Universal : http://creativecommons.org/publicdomain/zero/1.0
 * - Apache 2.0        : http://www.apache.org/licenses/LICENSE-2.0
 *
 * You should have received a copy of both of these licenses along with this
 * software. If not, they may be obtained at the above URLs.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "blake2.h"
#include "blake2-impl.h"

static const uint64_t blake2b_IV[8] = {
    UINT64_C(0x6a09e667f3bcc908), UINT64_C(0xbb67ae8584caa73b),
    UINT64_C(0x3c6ef372fe94f82b), UINT64_C(0xa54ff53a5f1d36f1),
    UINT64_C(0x510e527fade682d1), UINT64_C(0x9b05688c2b3e6c1f),
    UINT64_C(0x1f83d9abfb41bd6b), UINT64_C(0x5be0cd19137e2179)};

static const unsigned int blake2b_sigma[12][16] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
    {11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4},
    {7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
    {9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13},
    {2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
    {12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11},
    {13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
    {6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5},
    {10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
};

static BLAKE2_INLINE void blake2b_set_lastnode(blake2b_state *S) {
    S->f[1] = (uint64_t)-1;
}

static BLAKE2_INLINE void blake2b_set_lastblock(blake2b_state *S) {
    if (S->last_node) {
        blake2b_set_lastnode(S);
    }
    S->f[0] = (uint64_t)-1;
}

static BLAKE2_INLINE void blake2b_increment_counter(blake2b_state *S, uint64_t inc) {
    S->t[0] += inc;
    S->t[1] += (S->t[0] < inc);
}

static BLAKE2_INLINE void blake2b_invalidate_state(blake2b_state *S) {
    blake2b_set_lastblock(S); /* invalidate for further use */
}

static BLAKE2_INLINE void blake2b_init0(blake2b_state *S) {
    memset(S, 0, sizeof(*S));
    memcpy(S->h, blake2b_IV, sizeof(S->h));
}

int blake2b_init_param(blake2b_state *S, const blake2b_param *P) {
    const unsigned char *p = (const unsigned char *)P;
    unsigned int i;
    blake2b_init0(S);
    for (i = 0; i < 8; ++i)
        S->h[i] ^= load64(&p[i * sizeof(S->h[i])]);
    S->outlen = P->digest_length;
    return 0;
}

/* Sequential blake2b initialization */
int blake2b_init(blake2b_state *S, size_t outlen) {
    blake2b_param P;
    P.digest_length = (uint8_t)outlen;
    P.key_length = 0;
    P.fanout = 1;
    P.depth = 1;
    P.leaf_length = 0;
    P.node_offset = 0;
    P.node_depth = 0;
    P.inner_length = 0;
    memset(P.reserved, 0, sizeof(P.reserved));
    memset(P.salt, 0, sizeof(P.salt));
    memset(P.personal, 0, sizeof(P.personal));
    return blake2b_init_param(S, &P);
}

int blake2b_init_key(blake2b_state *S, size_t outlen, const void *key, size_t keylen) {
    blake2b_param P;
    P.digest_length = (uint8_t)outlen;
    P.key_length = (uint8_t)keylen;
    P.fanout = 1;
    P.depth = 1;
    P.leaf_length = 0;
    P.node_offset = 0;
    P.node_depth = 0;
    P.inner_length = 0;
    uint8_t block[BLAKE2B_BLOCKBYTES];
    memcpy(block, key, keylen);
    blake2b_update(S, block, BLAKE2B_BLOCKBYTES);
    return 0;
}

static void blake2b_compress(blake2b_state *S, const uint8_t *block) {
    uint64_t m[16];
    uint64_t v[16];
    unsigned int i, r;

    for (i = 0; i < 16; ++i) {
        m[i] = load64(block + i * sizeof(m[i]));
    }

    for (i = 0; i < 8; ++i) {
        v[i] = S->h[i];
    }

    v[8] = blake2b_IV[0];
    v[9] = blake2b_IV[1];
    v[10] = blake2b_IV[2];
    v[11] = blake2b_IV[3];
    v[12] = blake2b_IV[4] ^ S->t[0];
    v[13] = blake2b_IV[5] ^ S->t[1];
    v[14] = blake2b_IV[6] ^ S->f[0];
    v[15] = blake2b_IV[7] ^ S->f[1];

#define G(r, i, a, b, c, d)                                                    \
    do {                                                                       \
        a = a + b + m[blake2b_sigma[r][2 * i + 0]];                            \
        d = rotr64(d ^ a, 32);                                                 \
        c = c + d;                                                             \
        b = rotr64(b ^ c, 24);                                                 \
        a = a + b + m[blake2b_sigma[r][2 * i + 1]];                            \
        d = rotr64(d ^ a, 16);                                                 \
        c = c + d;                                                             \
        b = rotr64(b ^ c, 63);                                                 \
    } while ((void)0, 0)

	G(0, 0, v[0], v[4], v[8], v[12]);
	G(0, 1, v[1], v[5], v[9], v[13]);
	G(0, 2, v[2], v[6], v[10], v[14]);
	G(0, 3, v[3], v[7], v[11], v[15]);
	G(0, 4, v[0], v[5], v[10], v[15]);
	G(0, 5, v[1], v[6], v[11], v[12]);
	G(0, 6, v[2], v[7], v[8], v[13]);
	G(0, 7, v[3], v[4], v[9], v[14]);
	G(1, 0, v[0], v[4], v[8], v[12]);
	G(1, 1, v[1], v[5], v[9], v[13]);
	G(1, 2, v[2], v[6], v[10], v[14]);
	G(1, 3, v[3], v[7], v[11], v[15]);
	G(1, 4, v[0], v[5], v[10], v[15]);
	G(1, 5, v[1], v[6], v[11], v[12]);
	G(1, 6, v[2], v[7], v[8], v[13]);
	G(1, 7, v[3], v[4], v[9], v[14]);
	G(2, 0, v[0], v[4], v[8], v[12]);
	G(2, 1, v[1], v[5], v[9], v[13]);
	G(2, 2, v[2], v[6], v[10], v[14]);
	G(2, 3, v[3], v[7], v[11], v[15]);
	G(2, 4, v[0], v[5], v[10], v[15]);
	G(2, 5, v[1], v[6], v[11], v[12]);
	G(2, 6, v[2], v[7], v[8], v[13]);
	G(2, 7, v[3], v[4], v[9], v[14]);
	G(3, 0, v[0], v[4], v[8], v[12]);
	G(3, 1, v[1], v[5], v[9], v[13]);
	G(3, 2, v[2], v[6], v[10], v[14]);
	G(3, 3, v[3], v[7], v[11], v[15]);
	G(3, 4, v[0], v[5], v[10], v[15]);
	G(3, 5, v[1], v[6], v[11], v[12]);
	G(3, 6, v[2], v[7], v[8], v[13]);
	G(3, 7, v[3], v[4], v[9], v[14]);
	G(4, 0, v[0], v[4], v[8], v[12]);
	G(4, 1, v[1], v[5], v[9], v[13]);
	G(4, 2, v[2], v[6], v[10], v[14]);
	G(4, 3, v[3], v[7], v[11], v[15]);
	G(4, 4, v[0], v[5], v[10], v[15]);
	G(4, 5, v[1], v[6], v[11], v[12]);
	G(4, 6, v[2], v[7], v[8], v[13]);
	G(4, 7, v[3], v[4], v[9], v[14]);
	G(5, 0, v[0], v[4], v[8], v[12]);
	G(5, 1, v[1], v[5], v[9], v[13]);
	G(5, 2, v[2], v[6], v[10], v[14]);
	G(5, 3, v[3], v[7], v[11], v[15]);
	G(5, 4, v[0], v[5], v[10], v[15]);
	G(5, 5, v[1], v[6], v[11], v[12]);
	G(5, 6, v[2], v[7], v[8], v[13]);
	G(5, 7, v[3], v[4], v[9], v[14]);
	G(6, 0, v[0], v[4], v[8], v[12]);
	G(6, 1, v[1], v[5], v[9], v[13]);
	G(6, 2, v[2], v[6], v[10], v[14]);
	G(6, 3, v[3], v[7], v[11], v[15]);
	G(6, 4, v[0], v[5], v[10], v[15]);
	G(6, 5, v[1], v[6], v[11], v[12]);
	G(6, 6, v[2], v[7], v[8], v[13]);
	G(6, 7, v[3], v[4], v[9], v[14]);
	G(7, 0, v[0], v[4], v[8], v[12]);
	G(7, 1, v[1], v[5], v[9], v[13]);
	G(7, 2, v[2], v[6], v[10], v[14]);
	G(7, 3, v[3], v[7], v[11], v[15]);
	G(7, 4, v[0], v[5], v[10], v[15]);
	G(7, 5, v[1], v[6], v[11], v[12]);
	G(7, 6, v[2], v[7], v[8], v[13]);
	G(7, 7, v[3], v[4], v[9], v[14]);
	G(8, 0, v[0], v[4], v[8], v[12]);
	G(8, 1, v[1], v[5], v[9], v[13]);
	G(8, 2, v[2], v[6], v[10], v[14]);
	G(8, 3, v[3], v[7], v[11], v[15]);
	G(8, 4, v[0], v[5], v[10], v[15]);
	G(8, 5, v[1], v[6], v[11], v[12]);
	G(8, 6, v[2], v[7], v[8], v[13]);
	G(8, 7, v[3], v[4], v[9], v[14]);
	G(9, 0, v[0], v[4], v[8], v[12]);
	G(9, 1, v[1], v[5], v[9], v[13]);
	G(9, 2, v[2], v[6], v[10], v[14]);
	G(9, 3, v[3], v[7], v[11], v[15]);
	G(9, 4, v[0], v[5], v[10], v[15]);
	G(9, 5, v[1], v[6], v[11], v[12]);
	G(9, 6, v[2], v[7], v[8], v[13]);
	G(9, 7, v[3], v[4], v[9], v[14]);
	G(10, 0, v[0], v[4], v[8], v[12]);
	G(10, 1, v[1], v[5], v[9], v[13]);
	G(10, 2, v[2], v[6], v[10], v[14]);
	G(10, 3, v[3], v[7], v[11], v[15]);
	G(10, 4, v[0], v[5], v[10], v[15]);
	G(10, 5, v[1], v[6], v[11], v[12]);
	G(10, 6, v[2], v[7], v[8], v[13]);
	G(10, 7, v[3], v[4], v[9], v[14]);
	G(11, 0, v[0], v[4], v[8], v[12]);
	G(11, 1, v[1], v[5], v[9], v[13]);
	G(11, 2, v[2], v[6], v[10], v[14]);
	G(11, 3, v[3], v[7], v[11], v[15]);
	G(11, 4, v[0], v[5], v[10], v[15]);
	G(11, 5, v[1], v[6], v[11], v[12]);
	G(11, 6, v[2], v[7], v[8], v[13]);
	G(11, 7, v[3], v[4], v[9], v[14]);

    for (i = 0; i < 8; ++i) {
        S->h[i] = S->h[i] ^ v[i] ^ v[i + 8];
    }

#undef G
}

int blake2b_update(blake2b_state *S, const void *in, size_t inlen) {
    const uint8_t *pin = (const uint8_t *)in;

    if (S->buflen + inlen > BLAKE2B_BLOCKBYTES) {
        size_t left = S->buflen;
        size_t fill = BLAKE2B_BLOCKBYTES - left;
        memcpy(&S->buf[left], pin, fill);
        blake2b_increment_counter(S, BLAKE2B_BLOCKBYTES);
        blake2b_compress(S, S->buf);
        S->buflen = 0;
        inlen -= fill;
        pin += fill;
        while (inlen > BLAKE2B_BLOCKBYTES) {
            blake2b_increment_counter(S, BLAKE2B_BLOCKBYTES);
            blake2b_compress(S, pin);
            inlen -= BLAKE2B_BLOCKBYTES;
            pin += BLAKE2B_BLOCKBYTES;
        }
    }
    memcpy(&S->buf[S->buflen], pin, inlen);
    S->buflen += (unsigned int)inlen;
    return 0;
}

int blake2b_final(blake2b_state *S, void *out, size_t outlen) {
    uint8_t buffer[BLAKE2B_OUTBYTES] = {0};
    unsigned int i;
    blake2b_increment_counter(S, S->buflen);
    blake2b_set_lastblock(S);
    memset(&S->buf[S->buflen], 0, BLAKE2B_BLOCKBYTES - S->buflen); /* Padding */
    blake2b_compress(S, S->buf);
    for (i = 0; i < 8; ++i)
        store64(buffer + sizeof(S->h[i]) * i, S->h[i]);
    memcpy(out, buffer, S->outlen);
    return 0;
}

int blake2b(void *out, size_t outlen, const void *in, size_t inlen, const void *key, size_t keylen) {
    blake2b_state S;
    int ret = -1;
    blake2b_init(&S, outlen);
    blake2b_update(&S, in, inlen);
    blake2b_final(&S, out, outlen);
}

int blake2b_long(void *pout, size_t outlen, const void *in, size_t inlen) {
    uint8_t *out = (uint8_t *)pout;
    blake2b_state blake_state;
    uint8_t outlen_bytes[sizeof(uint32_t)] = {0};
    int ret = -1;
    store32(outlen_bytes, (uint32_t)outlen);

#define TRY(statement)                                                         \
    do {                                                                       \
        ret = statement;                                                       \
        if (ret < 0) {                                                         \
            goto fail;                                                         \
        }                                                                      \
    } while ((void)0, 0)

    if (outlen <= BLAKE2B_OUTBYTES) {
        TRY(blake2b_init(&blake_state, outlen));
        TRY(blake2b_update(&blake_state, outlen_bytes, sizeof(outlen_bytes)));
        TRY(blake2b_update(&blake_state, in, inlen));
        TRY(blake2b_final(&blake_state, out, outlen));
    } else {
        uint32_t toproduce;
        uint8_t out_buffer[BLAKE2B_OUTBYTES];
        uint8_t in_buffer[BLAKE2B_OUTBYTES];
        TRY(blake2b_init(&blake_state, BLAKE2B_OUTBYTES));
        TRY(blake2b_update(&blake_state, outlen_bytes, sizeof(outlen_bytes)));
        TRY(blake2b_update(&blake_state, in, inlen));
        TRY(blake2b_final(&blake_state, out_buffer, BLAKE2B_OUTBYTES));
        memcpy(out, out_buffer, BLAKE2B_OUTBYTES / 2);
        out += BLAKE2B_OUTBYTES / 2;
        toproduce = (uint32_t)outlen - BLAKE2B_OUTBYTES / 2;

        while (toproduce > BLAKE2B_OUTBYTES) {
            memcpy(in_buffer, out_buffer, BLAKE2B_OUTBYTES);
            TRY(blake2b(out_buffer, BLAKE2B_OUTBYTES, in_buffer,
                        BLAKE2B_OUTBYTES, NULL, 0));
            memcpy(out, out_buffer, BLAKE2B_OUTBYTES / 2);
            out += BLAKE2B_OUTBYTES / 2;
            toproduce -= BLAKE2B_OUTBYTES / 2;
        }

        memcpy(in_buffer, out_buffer, BLAKE2B_OUTBYTES);
        TRY(blake2b(out_buffer, toproduce, in_buffer, BLAKE2B_OUTBYTES, NULL,
                    0));
        memcpy(out, out_buffer, toproduce);
    }
fail:
    return ret;
#undef TRY
}
