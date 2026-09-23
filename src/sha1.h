#ifndef SHA1_H
#define SHA1_H

#include <stddef.h>

#define HASH_LEN 20     /* raw bytes */
#define HEX_LEN  40     /* printed as hex */

void sha1(const void *data, size_t len, unsigned char out[HASH_LEN]);

/* 20 raw bytes -> 40 hex chars + '\0' */
void hash_to_hex(const unsigned char *hash, char *hex);

/* 40 hex chars -> 20 raw bytes; returns 0 on success, -1 if not hex */
int hex_to_hash(const char *hex, unsigned char *hash);

/* 1 if s is exactly 40 lowercase hex characters */
int is_full_hex(const char *s);

#endif
