/* umd5 - MD5 digest calculating routines
 *
 * Copyright 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include <inttypes.h>
#include <stddef.h>

#include "umd5.h"

uint32_t T[64] = {
 /* Round 1 */
    0xd76aa478,
    0xe8c7b756,
    0x242070db,
    0xc1bdceee,
    0xf57c0faf,
    0x4787c62a,
    0xa8304613,
    0xfd469501,
    0x698098d8,
    0x8b44f7af,
    0xffff5bb1,
    0x895cd7be,
    0x6b901122,
    0xfd987193,
    0xa679438e,
    0x49b40821,

 /* Round 2 */
    0xf61e2562,
    0xc040b340,
    0x265e5a51,
    0xe9b6c7aa,
    0xd62f105d,
    0x02441453,
    0xd8a1e681,
    0xe7d3fbc8,
    0x21e1cde6,
    0xc33707d6,
    0xf4d50d87,
    0x455a14ed,
    0xa9e3e905,
    0xfcefa3f8,
    0x676f02d9,
    0x8d2a4c8a,

  /* Round 3 */
    0xfffa3942,
    0x8771f681,
    0x6d9d6122,
    0xfde5380c,
    0xa4beea44,
    0x4bdecfa9,
    0xf6bb4b60,
    0xbebfbc70,
    0x289b7ec6,
    0xeaa127fa,
    0xd4ef3085,
    0x04881d05,
    0xd9d4d039,
    0xe6db99e5,
    0x1fa27cf8,
    0xc4ac5665,

  /* Round 4 */
    0xf4292244,
    0x432aff97,
    0xab9423a7,
    0xfc93a039,
    0x655b59c3,
    0x8f0ccc92,
    0xffeff47d,
    0x85845dd1,
    0x6fa87e4f,
    0xfe2ce6e0,
    0xa3014314,
    0x4e0811a1,
    0xf7537e82,
    0xbd3af235,
    0x2ad7d2bb,
    0xeb86d391,
};

#define F(X,Y,Z) (((X) & (Y)) | (~(X) & (Z)))
#define G(X,Y,Z) (((X) & (Z)) | ((Y) & ~(Z)))
#define H(X,Y,Z) ((X) ^ (Y) ^ (Z))
#define I(X,Y,Z) ((Y) ^ ((X) | ~(Z)))

#define XX(X, ctx, a, b, c, d, k, s, i) \
    (a) = (b) + umd5_rol(((a) + X((b),(c),(d)) + \
        ((uint32_t *)((ctx)->block))[(k)] + T[(i)]), (s))

#define FF(ctx,a,b,c,d,k,s) XX(F,ctx,a,b,c,d,            k, s,      k)
#define GG(ctx,a,b,c,d,k,s) XX(G,ctx,a,b,c,d, (1+(k)*5)%16, s, (k)+16)
#define HH(ctx,a,b,c,d,k,s) XX(H,ctx,a,b,c,d, (5+(k)*3)%16, s, (k)+32)
#define II(ctx,a,b,c,d,k,s) XX(I,ctx,a,b,c,d,   ((k)*7)%16, s, (k)+48)

static uint32_t umd5_rol (uint32_t val, unsigned int shift)
{
    return (val << shift) | (val >> (32 - shift));
}

static void umd5_update_block(struct umd5_ctx *ctx)
{
    uint32_t a, b, c, d;
    int i;
    
    a = ctx->A;
    b = ctx->B;
    c = ctx->C;
    d = ctx->D;

    /* Round 1 */
    for (i = 0; i < 4; i++) {
        FF(ctx, a, b, c, d, i*4 + 0,  7);
        FF(ctx, d, a, b, c, i*4 + 1, 12);
        FF(ctx, c, d, a, b, i*4 + 2, 17);
        FF(ctx, b, c, d, a, i*4 + 3, 22);
    }
     
    /* Round 2 */
    for (i = 0; i < 4; i++) {
        GG(ctx, a, b, c, d, i*4 + 0,  5);
        GG(ctx, d, a, b, c, i*4 + 1,  9);
        GG(ctx, c, d, a, b, i*4 + 2, 14);
        GG(ctx, b, c, d, a, i*4 + 3, 20);
    }

    /* Round 3 */
    for (i = 0; i < 4; i++) {
        HH(ctx, a, b, c, d, i*4 + 0,  4);
        HH(ctx, d, a, b, c, i*4 + 1, 11);
        HH(ctx, c, d, a, b, i*4 + 2, 16);
        HH(ctx, b, c, d, a, i*4 + 3, 23);
    }

    /* Round 4 */
    for (i = 0; i < 4; i++) {
        II(ctx, a, b, c, d, i*4 + 0,  6);
        II(ctx, d, a, b, c, i*4 + 1, 10);
        II(ctx, c, d, a, b, i*4 + 2, 15);
        II(ctx, b, c, d, a, i*4 + 3, 21);
    }

    ctx->A += a;
    ctx->B += b;
    ctx->C += c;
    ctx->D += d;
}

void umd5_init(struct umd5_ctx *ctx)
{
    ctx->len = 0;
    ctx->A = 0x67452301;
    ctx->B = 0xefcdab89;
    ctx->C = 0x98badcfe;
    ctx->D = 0x10325476;
}

void umd5_update(struct umd5_ctx *ctx, const char *s, size_t size)
{
    /*TODO: assert len < 0xffffffffffffffff - size - UMD5_BLOCK_SIZE */

    while (size > 0) {
        while (size > 0) {
            ctx->block[ctx->len % UMD5_BLOCK_SIZE] = *s++;
            size--;
            ctx->len++;
            if (ctx->len % UMD5_BLOCK_SIZE == 0)
                break;
        }
        if (ctx->len % UMD5_BLOCK_SIZE != 0)
            return;

        /* block is full now, processing */
        umd5_update_block(ctx);
    }
}

void umd5_finish(struct umd5_ctx *ctx)
{
    char pad[UMD5_BLOCK_SIZE + 8];
    int i = 0;
    
    /* padding begins with bit 1 */
    pad[i++] = 0x80;

    while ((ctx->len + i) % UMD5_BLOCK_SIZE > UMD5_BLOCK_SIZE - 8)
        pad[i++] = 0;
    while ((ctx->len + i) % UMD5_BLOCK_SIZE < UMD5_BLOCK_SIZE - 8)
        pad[i++] = 0;
    
    /* append length of message in bits */
    *(uint64_t *) &pad[i] = ctx->len * 8;

    umd5_update(ctx, pad, i + 8);
}

static void umd5_bswap(char *buf, uint32_t w)
{
    buf[0] = (w      ) & 0xff;
    buf[1] = (w >>  8) & 0xff;
    buf[2] = (w >> 16) & 0xff;
    buf[3] = (w >> 24) & 0xff;
}        

void umd5_value(struct umd5_ctx *ctx, char *buf)
{
    umd5_bswap(buf, ctx->A);
    umd5_bswap(&buf[4], ctx->B);
    umd5_bswap(&buf[8], ctx->C);
    umd5_bswap(&buf[12], ctx->D);
}
