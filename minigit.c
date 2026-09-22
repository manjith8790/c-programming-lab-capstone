/*
 * minigit.c - a tiny Content-Addressable Storage (CAS) demo in C,
 * using the same blob format and hashing as real Git.
 *
 * Build:  gcc -Wall -o minigit minigit.c
 *
 * Usage:
 *   ./minigit init                 create .minigit/objects
 *   ./minigit hash-object <file>   store a file, print its hash (address)
 *   ./minigit cat-file <hash>      print the content stored at that hash
 *   ./minigit verify <hash>        re-hash the object and check integrity
 *
 * Blob format (same as Git):   "blob <size>\0<file bytes>"
 * Address:                     SHA-1 of that whole buffer
 * Stored at:                   .minigit/objects/<first 2 hex>/<remaining 38 hex>
 * (Real Git also zlib-compresses the object; we skip that to keep it simple.)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

/* ---------------- SHA-1 (small, self-contained) ---------------- */

#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void sha1(const unsigned char *data, size_t len, unsigned char out[20])
{
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

    /* Padding: message + 0x80 + zeros + 64-bit big-endian bit length */
    size_t total = ((len + 8) / 64 + 1) * 64;
    unsigned char *msg = calloc(total, 1);
    memcpy(msg, data, len);
    msg[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++)
        msg[total - 1 - i] = (unsigned char)(bits >> (8 * i));

    for (size_t off = 0; off < total; off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t)msg[off + 4*i] << 24 | (uint32_t)msg[off + 4*i + 1] << 16 |
                   (uint32_t)msg[off + 4*i + 2] << 8 | (uint32_t)msg[off + 4*i + 3];
        for (int i = 16; i < 80; i++)
            w[i] = ROL(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | (~b & d);          k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else             { f = b ^ c ^ d;                   k = 0xCA62C1D6; }
            uint32_t t = ROL(a, 5) + f + e + k + w[i];
            e = d; d = c; c = ROL(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    free(msg);

    for (int i = 0; i < 5; i++)
        for (int j = 0; j < 4; j++)
            out[4*i + j] = (unsigned char)(h[i] >> (24 - 8*j));
}

static void to_hex(const unsigned char hash[20], char hex[41])
{
    for (int i = 0; i < 20; i++)
        sprintf(hex + 2*i, "%02x", hash[i]);
    hex[40] = '\0';
}

/* ---------------- helpers ---------------- */

static unsigned char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    unsigned char *buf = malloc(size > 0 ? size : 1);
    *len = fread(buf, 1, size, f);
    fclose(f);
    return buf;
}

/* ".minigit/objects/ab/cdef..." from a 40-char hex hash */
static void object_path(const char *hex, char *dir, char *path)
{
    sprintf(dir, ".minigit/objects/%.2s", hex);
    sprintf(path, "%s/%s", dir, hex + 2);
}

/* ---------------- commands ---------------- */

static int cmd_init(void)
{
    mkdir(".minigit", 0755);
    mkdir(".minigit/objects", 0755);
    printf("Initialized empty repository in .minigit/\n");
    return 0;
}

static int cmd_hash_object(const char *file)
{
    size_t len;
    unsigned char *content = read_file(file, &len);
    if (!content) return 1;

    /* 1. Build the object: header + content */
    char header[64];
    int hlen = sprintf(header, "blob %zu", len) + 1;   /* +1 keeps the '\0' */
    size_t olen = hlen + len;
    unsigned char *obj = malloc(olen);
    memcpy(obj, header, hlen);
    memcpy(obj + hlen, content, len);

    /* 2. Hash it -> this is the object's address */
    unsigned char hash[20];
    char hex[41];
    sha1(obj, olen, hash);
    to_hex(hash, hex);

    /* 3. Store it at objects/xx/yyyy... (skip if already there = dedup) */
    char dir[64], path[128];
    object_path(hex, dir, path);
    struct stat st;
    if (stat(path, &st) == 0) {
        fprintf(stderr, "already stored (deduplicated)\n");
    } else {
        mkdir(dir, 0755);
        FILE *f = fopen(path, "wb");
        if (!f) { perror(path); return 1; }
        fwrite(obj, 1, olen, f);
        fclose(f);
    }
    printf("%s\n", hex);

    free(content);
    free(obj);
    return 0;
}

static int cmd_cat_file(const char *hex)
{
    char dir[64], path[128];
    object_path(hex, dir, path);
    size_t len;
    unsigned char *obj = read_file(path, &len);
    if (!obj) return 1;

    /* skip the "blob <size>\0" header and print the content */
    size_t hlen = strlen((char *)obj) + 1;
    fwrite(obj + hlen, 1, len - hlen, stdout);
    free(obj);
    return 0;
}

static int cmd_verify(const char *hex)
{
    char dir[64], path[128];
    object_path(hex, dir, path);
    size_t len;
    unsigned char *obj = read_file(path, &len);
    if (!obj) return 1;

    unsigned char hash[20];
    char actual[41];
    sha1(obj, len, hash);
    to_hex(hash, actual);
    free(obj);

    if (strcmp(actual, hex) == 0) {
        printf("OK: object is intact\n");
        return 0;
    }
    printf("CORRUPTED!\n  expected %s\n  actual   %s\n", hex, actual);
    return 1;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "init") == 0)
        return cmd_init();
    if (argc >= 3 && strcmp(argv[1], "hash-object") == 0)
        return cmd_hash_object(argv[2]);
    if (argc >= 3 && strcmp(argv[1], "cat-file") == 0)
        return cmd_cat_file(argv[2]);
    if (argc >= 3 && strcmp(argv[1], "verify") == 0)
        return cmd_verify(argv[2]);

    fprintf(stderr, "usage: %s init | hash-object <file> | cat-file <hash> | verify <hash>\n", argv[0]);
    return 1;
}
