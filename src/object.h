#ifndef OBJECT_H
#define OBJECT_H

#include <stddef.h>
#include "sha1.h"

/*
 * Object store: every object is saved as
 *     "<type> <size>\0<content>"
 * in .minigit/objects/<first 2 hex>/<remaining 38 hex>,
 * where the hex name is the SHA-1 of those bytes.
 */
enum obj_type { OBJ_BLOB, OBJ_TREE, OBJ_COMMIT, OBJ_BAD };

const char *type_name(enum obj_type type);

void object_path(const char *hex, char *path);

/* compute the hash without storing anything */
void hash_object(enum obj_type type, const void *data, size_t len, char *hex);

/* store an object; returns 1 if it was new, 0 if it already existed */
int write_object(enum obj_type type, const void *data, size_t len, char *hex);

/* returns the content (without header) or NULL if the object is missing */
unsigned char *read_object(const char *hex, enum obj_type *type, size_t *len);

/* 1 = content matches its name, 0 = corrupted, -1 = missing */
int verify_object(const char *hex);

/* call fn for every object in the store */
typedef void (*object_fn)(const char *hex, void *ctx);
void for_each_object(object_fn fn, void *ctx);

/* turn "HEAD", a branch name, a full hash or a short hash (4+ chars)
 * into a full hash; returns 1 if found */
int resolve(const char *name, char *hex);

#endif
