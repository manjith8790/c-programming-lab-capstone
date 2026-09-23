#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

#define REPO_DIR  ".minigit"
#define PATH_LEN  1024

/* print an error message and exit */
void die(const char *fmt, ...);

void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);

/* read a whole file; returns NULL if it cannot be opened.
 * The buffer gets an extra '\0' at the end so text can be parsed safely. */
unsigned char *read_file(const char *path, size_t *len);

/* write to "<path>.lock" first, then rename, so a crash never leaves
 * a half-written file behind */
void write_file_atomic(const char *path, const void *data, size_t len);

int  file_exists(const char *path);
int  is_dir(const char *path);
long file_size(const char *path);
void make_dir(const char *path);
void make_parent_dirs(const char *path);
void remove_file(const char *path);

/* call fn for every regular file below dir (skips .minigit and .git) */
typedef void (*file_fn)(const char *path, void *ctx);
void walk_files(const char *dir, file_fn fn, void *ctx);

/* growable byte buffer */
struct buf {
    unsigned char *data;
    size_t len, cap;
};
void buf_add(struct buf *b, const void *data, size_t len);
void buf_addstr(struct buf *b, const char *s);

#endif
