/*
 * SHA-1 as described in FIPS 180-4.
 *
 * The message is padded to a multiple of 64 bytes:
 *   message | 0x80 | zeros | 64-bit big-endian length in bits
 * then every 64-byte block is mixed into five 32-bit state words.
 */
#include "sha1.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void process_block(uint32_t h[5], const unsigned char *block)
{
    uint32_t w[80];

    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)block[4 * i] << 24 | (uint32_t)block[4 * i + 1] << 16 |
               (uint32_t)block[4 * i + 2] << 8 | (uint32_t)block[4 * i + 3];
    for (int i = 16; i < 80; i++)
        w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];

    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        uint32_t t = ROL(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = ROL(b, 30);
        b = a;
        a = t;
    }

    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
}

void sha1(const void *data, size_t len, unsigned char out[HASH_LEN])
{
    uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    const unsigned char *msg = data;

    /* all full blocks straight from the input */
    size_t full = len / 64 * 64;
    for (size_t off = 0; off < full; off += 64)
        process_block(h, msg + off);

    /* the rest plus padding fits in one or two blocks */
    unsigned char tail[128] = { 0 };
    size_t rest = len - full;
    memcpy(tail, msg + full, rest);
    tail[rest] = 0x80;
    size_t tail_len = (rest < 56) ? 64 : 128;

    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++)
        tail[tail_len - 1 - i] = (unsigned char)(bits >> (8 * i));

    for (size_t off = 0; off < tail_len; off += 64)
        process_block(h, tail + off);

    for (int i = 0; i < 5; i++)
        for (int j = 0; j < 4; j++)
            out[4 * i + j] = (unsigned char)(h[i] >> (24 - 8 * j));
}

void hash_to_hex(const unsigned char *hash, char *hex)
{
    static const char digits[] = "0123456789abcdef";
    for (int i = 0; i < HASH_LEN; i++) {
        hex[2 * i] = digits[hash[i] >> 4];
        hex[2 * i + 1] = digits[hash[i] & 0xf];
    }
    hex[HEX_LEN] = '\0';
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

int hex_to_hash(const char *hex, unsigned char *hash)
{
    for (int i = 0; i < HASH_LEN; i++) {
        int hi = hex_value(hex[2 * i]);
        int lo = hex_value(hex[2 * i + 1]);
        if (hi < 0 || lo < 0)
            return -1;
        hash[i] = (unsigned char)(hi << 4 | lo);
    }
    return 0;
}

int is_full_hex(const char *s)
{
    for (int i = 0; i < HEX_LEN; i++)
        if (hex_value(s[i]) < 0)
            return 0;
    return s[HEX_LEN] == '\0';
}
