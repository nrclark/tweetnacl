#include "saline.h"
#include <stdint.h>

typedef int64_t gf[16];

extern void randombytes(uint8_t *, uint64_t);
void crypto_hashblocks(uint8_t *x, const uint8_t *m, uint64_t n);

static const uint8_t _0[16] = {0};
static const uint8_t _9[32] = {9};

static const gf gf0 = {0};
static const gf gf1 = {1};
static const gf _121665 = {0xDB41, 0x0001};

static const gf D = {0x78a3, 0x1359, 0x4dca, 0x75eb, 0xd8ab, 0x4141,
                     0x0a4d, 0x0070, 0xe898, 0x7779, 0x4079, 0x8cc7,
                     0xfe73, 0x2b6f, 0x6cee, 0x5203};

static const gf D2 = {0xf159, 0x26b2, 0x9b94, 0xebd6, 0xb156, 0x8283,
                      0x149a, 0x00e0, 0xd130, 0xeef3, 0x80f2, 0x198e,
                      0xfce7, 0x56df, 0xd9dc, 0x2406};

static const gf X = {0xd51a, 0x8f25, 0x2d60, 0xc956, 0xa7b2, 0x9525,
                     0xc760, 0x692c, 0xdc5c, 0xfdd6, 0xe231, 0xc0a4,
                     0x53fe, 0xcd6e, 0x36d3, 0x2169};

static const gf Y = {0x6658, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666,
                     0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666,
                     0x6666, 0x6666, 0x6666, 0x6666};

static const gf I = {0xa0b0, 0x4a0e, 0x1b27, 0xc4ee, 0xe478, 0xad2f,
                     0x1806, 0x2f43, 0xd7a7, 0x3dfb, 0x0099, 0x2b4d,
                     0xdf0b, 0x4fc1, 0x2480, 0x2b83};

static uint32_t L32(uint32_t x, int c)
{
    return (x << c) | ((x & 0xffffffff) >> (32 - c));
}

static uint32_t ld32(const uint8_t *x)
{
    uint32_t u = x[3];
    u = (u << 8) | x[2];
    u = (u << 8) | x[1];
    return (u << 8) | x[0];
}

static uint64_t dl64(const uint8_t *x)
{
    uint64_t i, u = 0;

    for (i = 0; i < 8; ++i) {
        u = (u << 8) | x[i];
    }

    return u;
}

static void st32(uint8_t *x, uint32_t u)
{
    for (int i = 0; i < 4; ++i) {
        x[i] = (uint8_t) u;
        u >>= 8;
    }
}

static void ts64(uint8_t *x, uint64_t u)
{
    for (int i = 7; i >= 0; --i) {
        x[i] = (uint8_t) u;
        u >>= 8;
    }
}

static int vn(const uint8_t *x, const uint8_t *y, int n)
{
    uint32_t d = 0;

    for (int i = 0; i < n; ++i) {
        d |= x[i] ^ y[i];
    }

    return (int)(1 & ((d - 1) >> 8)) - 1;
}

int crypto_verify_16(const unsigned char *x, const uint8_t *y)
{
    return vn(x, y, 16);
}

int crypto_verify_32(const unsigned char *x, const uint8_t *y)
{
    return vn(x, y, 32);
}

static void core(uint8_t *out, const uint8_t *in, const uint8_t *k,
                 const uint8_t *c, int h)
{
    uint32_t w[16], x[16], y[16], t[4];
    int i, j, m;

    for (i = 0; i < 4; ++i) {
        x[5 * i] = ld32(c + 4 * i);
        x[1 + i] = ld32(k + 4 * i);
        x[6 + i] = ld32(in + 4 * i);
        x[11 + i] = ld32(k + 16 + 4 * i);
    }

    for (i = 0; i < 16; ++i) {
        y[i] = x[i];
    }

    for (i = 0; i < 20; ++i) {
        for (j = 0; j < 4; ++j) {
            for (m = 0; m < 4; ++m) {
                t[m] = x[(5 * j + 4 * m) % 16];
            }

            t[1] ^= L32(t[0] + t[3], 7);
            t[2] ^= L32(t[1] + t[0], 9);
            t[3] ^= L32(t[2] + t[1], 13);
            t[0] ^= L32(t[3] + t[2], 18);

            for (m = 0; m < 4; ++m) {
                w[4 * j + (j + m) % 4] = t[m];
            }
        }

        for (m = 0; m < 16; ++m) {
            x[m] = w[m];
        }
    }

    if (h) {
        for (i = 0; i < 16; ++i) {
            x[i] += y[i];
        }

        for (i = 0; i < 4; ++i) {
            x[5 * i] -= ld32(c + 4 * i);
            x[6 + i] -= ld32(in + 4 * i);
        }

        for (i = 0; i < 4; ++i) {
            st32(out + 4 * i, x[5 * i]);
            st32(out + 16 + 4 * i, x[6 + i]);
        }
    } else {
        for (i = 0; i < 16; ++i) {
            st32(out + 4 * i, x[i] + y[i]);
        }
    }
}

static int crypto_core_salsa20(uint8_t *out, const uint8_t *in,
                               const uint8_t *k, const uint8_t *c)
{
    core(out, in, k, c, 0);
    return 0;
}

static int crypto_core_hsalsa20(uint8_t *out, const uint8_t *in,
                                const uint8_t *k, const uint8_t *c)
{
    core(out, in, k, c, 1);
    return 0;
}

static const uint8_t sigma[17] = "expand 32-byte k";

static int crypto_stream_salsa20_xor(uint8_t *c, const uint8_t *m, uint64_t b,
                                     const uint8_t *n, const uint8_t *k)
{
    uint8_t z[16], x[64];
    uint32_t u, i;

    if (!b) {
        return 0;
    }

    for (i = 0; i < 16; ++i) {
        z[i] = 0;
    }

    for (i = 0; i < 8; ++i) {
        z[i] = n[i];
    }

    while (b >= 64) {
        crypto_core_salsa20(x, z, k, sigma);

        for (i = 0; i < 64; ++i) {
            c[i] = (uint8_t) ((m ? m[i] : 0) ^ x[i]);
        }

        u = 1;

        for (i = 8; i < 16; ++i) {
            u += (uint32_t) z[i];
            z[i] = (uint8_t) u;
            u >>= 8;
        }

        b -= 64;
        c += 64;

        if (m) {
            m += 64;
        }
    }

    if (b) {
        crypto_core_salsa20(x, z, k, sigma);

        for (i = 0; i < b; ++i) {
            c[i] = (uint8_t) ((m ? m[i] : 0) ^ x[i]);
        }
    }

    return 0;
}

static int crypto_stream_salsa20(uint8_t *c, uint64_t d, const uint8_t *n,
                                 const uint8_t *k)
{
    return crypto_stream_salsa20_xor(c, 0, d, n, k);
}

int crypto_stream(unsigned char *c, unsigned long long d,
                  const unsigned char *n, const unsigned char *k)
{
    uint8_t s[32];
    crypto_core_hsalsa20(s, n, k, sigma);
    return crypto_stream_salsa20(c, d, n + 16, s);
}

int crypto_stream_xor(unsigned char *c, const unsigned char *m,
                      unsigned long long d, const unsigned char *n,
                      const unsigned char *k)
{
    uint8_t s[32];
    crypto_core_hsalsa20(s, n, k, sigma);
    return crypto_stream_salsa20_xor(c, m, d, n + 16, s);
}

static void add1305(uint32_t *h, const uint32_t *c)
{
    uint32_t j, u = 0;

    for (j = 0; j < 17; ++j) {
        u += h[j] + c[j];
        h[j] = u & 255;
        u >>= 8;
    }
}

static const uint32_t minusp[17] = {5, 0, 0, 0, 0, 0, 0, 0, 0,
                                    0, 0, 0, 0, 0, 0, 0, 252
                                   };

int crypto_onetimeauth(unsigned char *out, const unsigned char *m,
                       unsigned long long n, const unsigned char *k)
{
    uint32_t s, i, j, u, x[17], r[17], h[17], c[17], g[17];

    for (j = 0; j < 17; ++j) {
        r[j] = h[j] = 0;
    }

    for (j = 0; j < 16; ++j) {
        r[j] = k[j];
    }

    r[3] &= 15;
    r[4] &= 252;
    r[7] &= 15;
    r[8] &= 252;
    r[11] &= 15;
    r[12] &= 252;
    r[15] &= 15;

    while (n > 0) {
        for (j = 0; j < 17; ++j) {
            c[j] = 0;
        }

        for (j = 0; (j < 16) && (j < n); ++j) {
            c[j] = m[j];
        }

        c[j] = 1;
        m += j;
        n -= j;
        add1305(h, c);

        for (i = 0; i < 17; ++i) {
            x[i] = 0;

            for (j = 0; j < 17; ++j) {
                x[i] += h[j] * ((j <= i) ? r[i - j] : 320 * r[i + 17 - j]);
            }
        }

        for (i = 0; i < 17; ++i) {
            h[i] = x[i];
        }

        u = 0;

        for (j = 0; j < 16; ++j) {
            u += h[j];
            h[j] = u & 255;
            u >>= 8;
        }

        u += h[16];
        h[16] = u & 3;
        u = 5 * (u >> 2);

        for (j = 0; j < 16; ++j) {
            u += h[j];
            h[j] = u & 255;
            u >>= 8;
        }

        u += h[16];
        h[16] = u;
    }

    for (j = 0; j < 17; ++j) {
        g[j] = h[j];
    }

    add1305(h, minusp);
    s = -(h[16] >> 7);

    for (j = 0; j < 17; ++j) {
        h[j] ^= s & (g[j] ^ h[j]);
    }

    for (j = 0; j < 16; ++j) {
        c[j] = k[j + 16];
    }

    c[16] = 0;
    add1305(h, c);

    for (j = 0; j < 16; ++j) {
        out[j] = (uint8_t) h[j];
    }

    return 0;
}

int crypto_onetimeauth_verify(const unsigned char *h, const unsigned char *m,
                              unsigned long long n, const unsigned char *k)
{
    uint8_t x[16];
    crypto_onetimeauth(x, m, n, k);
    return crypto_verify_16(h, x);
}

int crypto_secretbox(unsigned char *c, const unsigned char *m,
                     unsigned long long d, const unsigned char *n,
                     const unsigned char *k)
{
    int i;

    if (d < 32) {
        return -1;
    }

    crypto_stream_xor(c, m, d, n, k);
    crypto_onetimeauth(c + 16, c + 32, d - 32, c);

    for (i = 0; i < 16; ++i) {
        c[i] = 0;
    }

    return 0;
}

int crypto_secretbox_open(unsigned char *m, const unsigned char *c,
                          unsigned long long d, const unsigned char *n,
                          const unsigned char *k)
{
    int i;
    uint8_t x[32];

    if (d < 32) {
        return -1;
    }

    crypto_stream(x, 32, n, k);

    if (crypto_onetimeauth_verify(c + 16, c + 32, d - 32, x) != 0) {
        return -1;
    }

    crypto_stream_xor(m, c, d, n, k);

    for (i = 0; i < 32; ++i) {
        m[i] = 0;
    }

    return 0;
}

static void set25519(gf r, const gf a)
{
    int i;

    for (i = 0; i < 16; ++i) {
        r[i] = a[i];
    }
}

static void car25519(gf o)
{
    int i;
    int64_t c;

    for (i = 0; i < 16; ++i) {
        o[i] += (1LL << 16);
        c = o[i] >> 16;
        o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
        o[i] -= c << 16;
    }
}

static void sel25519(gf p, gf q, int b)
{
    int64_t t, i, c = ~(b - 1);

    for (i = 0; i < 16; ++i) {
        t = c & (p[i] ^ q[i]);
        p[i] ^= t;
        q[i] ^= t;
    }
}

static void pack25519(uint8_t *o, const gf n)
{
    int i, j, b;
    gf m, t;

    for (i = 0; i < 16; ++i) {
        t[i] = n[i];
    }

    car25519(t);
    car25519(t);
    car25519(t);

    for (j = 0; j < 2; ++j) {
        m[0] = t[0] - 0xffed;

        for (i = 1; i < 15; i++) {
            m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
            m[i - 1] &= 0xffff;
        }

        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
        b = (m[15] >> 16) & 1;
        m[14] &= 0xffff;
        sel25519(t, m, 1 - b);
    }

    for (i = 0; i < 16; ++i) {
        o[2 * i] = (uint8_t) (t[i]);
        o[2 * i + 1] = (uint8_t) (t[i] >> 8);
    }
}

static int neq25519(const gf a, const gf b)
{
    uint8_t c[32], d[32];
    pack25519(c, a);
    pack25519(d, b);
    return crypto_verify_32(c, d);
}

static uint8_t par25519(const gf a)
{
    uint8_t d[32];
    pack25519(d, a);
    return d[0] & 1;
}

static void unpack25519(gf o, const uint8_t *n)
{
    int i;

    for (i = 0; i < 16; ++i) {
        o[i] = n[2 * i] + ((int64_t)n[2 * i + 1] << 8);
    }

    o[15] &= 0x7fff;
}

static void A(gf o, const gf a, const gf b)
{
    int i;

    for (i = 0; i < 16; ++i) {
        o[i] = a[i] + b[i];
    }
}

static void Z(gf o, const gf a, const gf b)
{
    int i;

    for (i = 0; i < 16; ++i) {
        o[i] = a[i] - b[i];
    }
}

static void M(gf o, const gf a, const gf b)
{
    int64_t i, j, t[31];

    for (i = 0; i < 31; ++i) {
        t[i] = 0;
    }

    for (i = 0; i < 16; ++i) {
        for (j = 0; j < 16; ++j) {
            t[i + j] += a[i] * b[j];
        }
    }

    for (i = 0; i < 15; ++i) {
        t[i] += 38 * t[i + 16];
    }

    for (i = 0; i < 16; ++i) {
        o[i] = t[i];
    }

    car25519(o);
    car25519(o);
}

static void S(gf o, const gf a)
{
    M(o, a, a);
}

static void inv25519(gf o, const gf i)
{
    gf c;
    int a;

    for (a = 0; a < 16; ++a) {
        c[a] = i[a];
    }

    for (a = 253; a >= 0; a--) {
        S(c, c);

        if (a != 2 && a != 4) {
            M(c, c, i);
        }
    }

    for (a = 0; a < 16; ++a) {
        o[a] = c[a];
    }
}

static void pow2523(gf o, const gf i)
{
    gf c;
    int a;

    for (a = 0; a < 16; ++a) {
        c[a] = i[a];
    }

    for (a = 250; a >= 0; a--) {
        S(c, c);

        if (a != 1) {
            M(c, c, i);
        }
    }

    for (a = 0; a < 16; ++a) {
        o[a] = c[a];
    }
}

int crypto_scalarmult(unsigned char *q, const unsigned char *n,
                      const unsigned char *p)
{
    uint8_t z[32];
    int64_t x[80], r, i;
    gf a, b, c, d, e, f;

    for (i = 0; i < 31; ++i) {
        z[i] = n[i];
    }

    z[31] = (uint8_t) ((n[31] & 127U) | 64);
    z[0] &= 248;
    unpack25519(x, p);

    for (i = 0; i < 16; ++i) {
        b[i] = x[i];
        d[i] = a[i] = c[i] = 0;
    }

    a[0] = d[0] = 1;

    for (i = 254; i >= 0; --i) {
        r = (z[i >> 3] >> (i & 7)) & 1;
        sel25519(a, b, (int) r);
        sel25519(c, d, (int) r);
        A(e, a, c);
        Z(a, a, c);
        A(c, b, d);
        Z(b, b, d);
        S(d, e);
        S(f, a);
        M(a, c, a);
        M(c, b, e);
        A(e, a, c);
        Z(a, a, c);
        S(b, a);
        Z(c, d, f);
        M(a, c, _121665);
        A(a, a, d);
        M(c, c, a);
        M(a, d, f);
        M(d, b, x);
        S(b, e);
        sel25519(a, b, (int) r);
        sel25519(c, d, (int) r);
    }

    for (i = 0; i < 16; ++i) {
        x[i + 16] = a[i];
        x[i + 32] = c[i];
        x[i + 48] = b[i];
        x[i + 64] = d[i];
    }

    inv25519(x + 32, x + 32);
    M(x + 16, x + 16, x + 32);
    pack25519(q, x + 16);
    return 0;
}

int crypto_scalarmult_base(unsigned char *q, const unsigned char *n)
{
    return crypto_scalarmult(q, n, _9);
}

int crypto_box_keypair(unsigned char *y, unsigned char *x)
{
    randombytes(x, 32);
    return crypto_scalarmult_base(y, x);
}

int crypto_box_beforenm(unsigned char *k, const unsigned char *y,
                        const unsigned char *x)
{
    uint8_t s[32];
    crypto_scalarmult(s, x, y);
    return crypto_core_hsalsa20(k, _0, s, sigma);
}

int crypto_box_afternm(unsigned char *c, const unsigned char *m,
                       unsigned long long d, const unsigned char *n,
                       const unsigned char *k)
{
    return crypto_secretbox(c, m, d, n, k);
}

int crypto_box_open_afternm(unsigned char *m, const unsigned char *c,
                            unsigned long long d, const unsigned char *n,
                            const unsigned char *k)
{
    return crypto_secretbox_open(m, c, d, n, k);
}

int crypto_box(unsigned char *c, const unsigned char *m, unsigned long long d,
               const unsigned char *n, const unsigned char *y,
               const unsigned char *x)
{
    uint8_t k[32];
    crypto_box_beforenm(k, y, x);
    return crypto_box_afternm(c, m, d, n, k);
}

int crypto_box_open(unsigned char *m, const unsigned char *c,
                    unsigned long long d, const unsigned char *n,
                    const unsigned char *y, const unsigned char *x)
{
    uint8_t k[32];
    crypto_box_beforenm(k, y, x);
    return crypto_box_open_afternm(m, c, d, n, k);
}

static uint64_t R(uint64_t x, int c)
{
    return (x >> c) | (x << (64 - c));
}

static uint64_t Ch(uint64_t x, uint64_t y, uint64_t z)
{
    return (x & y) ^ (~x & z);
}

static uint64_t Maj(uint64_t x, uint64_t y, uint64_t z)
{
    return (x & y) ^ (x & z) ^ (y & z);
}

static uint64_t Sigma0(uint64_t x)
{
    return R(x, 28) ^ R(x, 34) ^ R(x, 39);
}

static uint64_t Sigma1(uint64_t x)
{
    return R(x, 14) ^ R(x, 18) ^ R(x, 41);
}

static uint64_t sigma0(uint64_t x)
{
    return R(x, 1) ^ R(x, 8) ^ (x >> 7);
}

static uint64_t sigma1(uint64_t x)
{
    return R(x, 19) ^ R(x, 61) ^ (x >> 6);
}

static const uint64_t K[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL,
    0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
    0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL,
    0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
    0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
    0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL, 0x2de92c6f592b0275ULL,
    0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL,
    0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
    0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL,
    0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL,
    0x92722c851482353bULL, 0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
    0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
    0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL,
    0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
    0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL,
    0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL,
    0xc67178f2e372532bULL, 0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
    0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL,
    0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
    0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
    0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

void crypto_hashblocks(uint8_t *x, const uint8_t *m, uint64_t n)
{
    uint64_t z[8], b[8], a[8], w[16], t;
    int i, j;

    for (i = 0; i < 8; ++i) {
        z[i] = a[i] = dl64(x + 8 * i);
    }

    while (n >= 128) {
        for (i = 0; i < 16; ++i) {
            w[i] = dl64(m + 8 * i);
        }

        for (i = 0; i < 80; ++i) {
            for (j = 0; j < 8; ++j) {
                b[j] = a[j];
            }

            t = a[7] + Sigma1(a[4]) + Ch(a[4], a[5], a[6]) + K[i] + w[i % 16];
            b[7] = t + Sigma0(a[0]) + Maj(a[0], a[1], a[2]);
            b[3] += t;

            for (j = 0; j < 8; ++j) {
                a[(j + 1) % 8] = b[j];
            }

            if (i % 16 == 15) {
                for (j = 0; j < 16; ++j) {
                    w[j] += w[(j + 9) % 16] + sigma0(w[(j + 1) % 16]) +
                            sigma1(w[(j + 14) % 16]);
                }
            }
        }

        for (i = 0; i < 8; ++i) {
            a[i] += z[i];
            z[i] = a[i];
        }

        m += 128;
        n -= 128;
    }

    for (i = 0; i < 8; ++i) {
        ts64(x + 8 * i, z[i]);
    }
}

static const uint8_t iv[64] = {
    0x6a, 0x09, 0xe6, 0x67, 0xf3, 0xbc, 0xc9, 0x08, 0xbb, 0x67, 0xae,
    0x85, 0x84, 0xca, 0xa7, 0x3b, 0x3c, 0x6e, 0xf3, 0x72, 0xfe, 0x94,
    0xf8, 0x2b, 0xa5, 0x4f, 0xf5, 0x3a, 0x5f, 0x1d, 0x36, 0xf1, 0x51,
    0x0e, 0x52, 0x7f, 0xad, 0xe6, 0x82, 0xd1, 0x9b, 0x05, 0x68, 0x8c,
    0x2b, 0x3e, 0x6c, 0x1f, 0x1f, 0x83, 0xd9, 0xab, 0xfb, 0x41, 0xbd,
    0x6b, 0x5b, 0xe0, 0xcd, 0x19, 0x13, 0x7e, 0x21, 0x79
};

int crypto_hash(unsigned char *out, const unsigned char *m,
                unsigned long long n)
{
    uint8_t h[64], x[256];
    uint64_t i, b = n;

    for (i = 0; i < 64; ++i) {
        h[i] = iv[i];
    }

    crypto_hashblocks(h, m, n);
    m += n;
    n &= 127;
    m -= n;

    for (i = 0; i < 256; ++i) {
        x[i] = 0;
    }

    for (i = 0; i < n; ++i) {
        x[i] = m[i];
    }

    x[n] = 128;

    n = 256 - 128 * (n < 112);
    x[n - 9] = (uint8_t) (b >> 61);
    ts64(x + n - 8, b << 3);
    crypto_hashblocks(h, x, n);

    for (i = 0; i < 64; ++i) {
        out[i] = h[i];
    }

    return 0;
}

static void add(gf p[4], gf q[4])
{
    gf a, b, c, d, t, e, f, g, h;

    Z(a, p[1], p[0]);
    Z(t, q[1], q[0]);
    M(a, a, t);
    A(b, p[0], p[1]);
    A(t, q[0], q[1]);
    M(b, b, t);
    M(c, p[3], q[3]);
    M(c, c, D2);
    M(d, p[2], q[2]);
    A(d, d, d);
    Z(e, b, a);
    Z(f, d, c);
    A(g, d, c);
    A(h, b, a);

    M(p[0], e, f);
    M(p[1], h, g);
    M(p[2], g, f);
    M(p[3], e, h);
}

static void cswap(gf p[4], gf q[4], uint8_t b)
{
    int i;

    for (i = 0; i < 4; ++i) {
        sel25519(p[i], q[i], b);
    }
}

static void pack(uint8_t *r, gf p[4])
{
    gf tx, ty, zi;
    inv25519(zi, p[2]);
    M(tx, p[0], zi);
    M(ty, p[1], zi);
    pack25519(r, ty);
    r[31] ^= (uint8_t) (par25519(tx) << 7);
}

static void scalarmult(gf p[4], gf q[4], const uint8_t *s)
{
    int i;
    set25519(p[0], gf0);
    set25519(p[1], gf1);
    set25519(p[2], gf1);
    set25519(p[3], gf0);

    for (i = 255; i >= 0; --i) {
        uint8_t b = (s[i / 8] >> (i & 7)) & 1;
        cswap(p, q, b);
        add(q, p);
        add(p, p);
        cswap(p, q, b);
    }
}

static void scalarbase(gf p[4], const uint8_t *s)
{
    gf q[4];
    set25519(q[0], X);
    set25519(q[1], Y);
    set25519(q[2], gf1);
    M(q[3], X, Y);
    scalarmult(p, q, s);
}

int crypto_sign_keypair(unsigned char *pk, unsigned char *sk)
{
    uint8_t d[64];
    gf p[4];
    int i;

    randombytes(sk, 32);
    crypto_hash(d, sk, 32);
    d[0] &= 248;
    d[31] &= 127;
    d[31] |= 64;

    scalarbase(p, d);
    pack(pk, p);

    for (i = 0; i < 32; ++i) {
        sk[32 + i] = pk[i];
    }

    return 0;
}

static const uint64_t L[32] = {0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
                               0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
                              };

static void modL(uint8_t *r, int64_t x[64])
{
    int64_t carry, i, j;

    for (i = 63; i >= 32; --i) {
        carry = 0;

        for (j = i - 32; j < i - 12; ++j) {
            x[j] += carry - 16 * x[i] * (int64_t) L[j - (i - 32)];
            carry = (x[j] + 128) >> 8;
            x[j] -= carry << 8;
        }

        x[j] += carry;
        x[i] = 0;
    }

    carry = 0;

    for (j = 0; j < 32; ++j) {
        x[j] += carry - (x[31] >> 4) * (int64_t) L[j];
        carry = x[j] >> 8;
        x[j] &= 255;
    }

    for (j = 0; j < 32; ++j) {
        x[j] -= carry * (int64_t) L[j];
    }

    for (i = 0; i < 32; ++i) {
        x[i + 1] += x[i] >> 8;
        r[i] = (uint8_t) x[i];
    }
}

static void reduce(uint8_t *r)
{
    int64_t x[64], i;

    for (i = 0; i < 64; ++i) {
        x[i] = (uint64_t) r[i];
    }

    for (i = 0; i < 64; ++i) {
        r[i] = 0;
    }

    modL(r, x);
}

int crypto_sign(unsigned char *sm, unsigned long long *smlen,
                const unsigned char *m, unsigned long long n,
                const unsigned char *sk)
{
    uint8_t d[64], h[64], r[64];
    int64_t x[64];
    gf p[4];

    crypto_hash(d, sk, 32);
    d[0] &= 248;
    d[31] &= 127;
    d[31] |= 64;

    *smlen = n + 64;

    for (unsigned long long i = 0; i < n; ++i) {
        sm[64 + i] = m[i];
    }

    for (int i = 0; i < 32; ++i) {
        sm[32 + i] = d[32 + i];
    }

    crypto_hash(r, sm + 32, n + 32);
    reduce(r);
    scalarbase(p, r);
    pack(sm, p);

    for (int i = 0; i < 32; ++i) {
        sm[i + 32] = sk[i + 32];
    }

    crypto_hash(h, sm, n + 64);
    reduce(h);

    for (int i = 0; i < 64; ++i) {
        x[i] = 0;
    }

    for (int i = 0; i < 32; ++i) {
        x[i] = (uint64_t)r[i];
    }

    for (int i = 0; i < 32; ++i) {
        for (int j = 0; j < 32; ++j) {
            x[i + j] += h[i] * (int64_t) d[j];
        }
    }

    modL(sm + 32, x);

    return 0;
}

static int unpackneg(gf r[4], const uint8_t p[32])
{
    gf t, chk, num, den, den2, den4, den6;
    set25519(r[2], gf1);
    unpack25519(r[1], p);
    S(num, r[1]);
    M(den, num, D);
    Z(num, num, r[2]);
    A(den, r[2], den);

    S(den2, den);
    S(den4, den2);
    M(den6, den4, den2);
    M(t, den6, num);
    M(t, t, den);

    pow2523(t, t);
    M(t, t, num);
    M(t, t, den);
    M(t, t, den);
    M(r[0], t, den);

    S(chk, r[0]);
    M(chk, chk, den);

    if (neq25519(chk, num)) {
        M(r[0], r[0], I);
    }

    S(chk, r[0]);
    M(chk, chk, den);

    if (neq25519(chk, num)) {
        return -1;
    }

    if (par25519(r[0]) == (p[31] >> 7)) {
        Z(r[0], gf0, r[0]);
    }

    M(r[3], r[0], r[1]);
    return 0;
}

int crypto_sign_open(unsigned char *m, unsigned long long *mlen,
                     const unsigned char *sm, unsigned long long n,
                     const unsigned char *pk)
{
    uint8_t t[32], h[64];
    gf p[4], q[4];

    *mlen = (unsigned long long) (-1);

    if (n < 64) {
        return -1;
    }

    if (unpackneg(q, pk)) {
        return -1;
    }

    for (unsigned long long i = 0; i < n; ++i) {
        m[i] = sm[i];
    }

    for (int i = 0; i < 32; ++i) {
        m[i + 32] = pk[i];
    }

    crypto_hash(h, m, n);
    reduce(h);
    scalarmult(p, q, h);

    scalarbase(q, sm + 32);
    add(p, q);
    pack(t, p);

    n -= 64;

    if (crypto_verify_32(sm, t)) {
        for (unsigned long long i = 0; i < n; ++i) {
            m[i] = 0;
        }

        return -1;
    }

    for (unsigned long long i = 0; i < n; ++i) {
        m[i] = sm[i + 64];
    }

    *mlen = n;
    return 0;
}
