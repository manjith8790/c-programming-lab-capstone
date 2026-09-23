#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#define MKDIR(p) mkdir(p, 0755)
#endif

void die(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "minigit: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p)
        die("out of memory");
    return p;
}

void *xrealloc(void *p, size_t n)
{
    p = realloc(p, n ? n : 1);
    if (!p)
        die("out of memory");
    return p;
}

unsigned char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    if (size < 0)
        die("cannot read %s", path);

    unsigned char *data = xmalloc((size_t)size + 1);
    size_t got = fread(data, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size)
        die("cannot read %s", path);

    data[size] = '\0';
    *len = (size_t)size;
    return data;
}

void write_file_atomic(const char *path, const void *data, size_t len)
{
    char tmp[PATH_LEN + 8];
    snprintf(tmp, sizeof(tmp), "%s.lock", path);

    FILE *f = fopen(tmp, "wb");
    if (!f)
        die("cannot write %s", tmp);
    if (fwrite(data, 1, len, f) != len || fclose(f) != 0)
        die("cannot write %s", tmp);

#ifdef _WIN32
    remove(path);           /* rename() on Windows does not overwrite */
#endif
    if (rename(tmp, path) != 0)
        die("cannot rename %s to %s", tmp, path);
}

int file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

long file_size(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 ? (long)st.st_size : -1;
}

void make_dir(const char *path)
{
    if (MKDIR(path) != 0 && errno != EEXIST)
        die("cannot create directory %s", path);
}

void make_parent_dirs(const char *path)
{
    char tmp[PATH_LEN];
    snprintf(tmp, sizeof(tmp), "%s", path);

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            make_dir(tmp);
            *p = '/';
        }
    }
}

void remove_file(const char *path)
{
    if (remove(path) != 0 && errno != ENOENT)
        die("cannot remove %s", path);
}

static int compare_names(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

void walk_files(const char *dir, file_fn fn, void *ctx)
{
    DIR *d = opendir(dir);
    if (!d)
        return;

    /* collect the names first so files are visited in sorted order */
    char **names = NULL;
    int count = 0, cap = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        const char *name = de->d_name;
        if (!strcmp(name, ".") || !strcmp(name, "..") ||
            !strcmp(name, REPO_DIR) || !strcmp(name, ".git"))
            continue;
        if (count == cap) {
            cap = cap ? cap * 2 : 16;
            names = xrealloc(names, cap * sizeof(*names));
        }
        names[count] = xmalloc(strlen(name) + 1);
        strcpy(names[count++], name);
    }
    closedir(d);
    if (count > 1)
        qsort(names, count, sizeof(*names), compare_names);

    for (int i = 0; i < count; i++) {
        char path[PATH_LEN];
        if (!strcmp(dir, "."))
            snprintf(path, sizeof(path), "%s", names[i]);
        else
            snprintf(path, sizeof(path), "%s/%s", dir, names[i]);

        if (is_dir(path))
            walk_files(path, fn, ctx);
        else
            fn(path, ctx);
        free(names[i]);
    }
    free(names);
}

void buf_add(struct buf *b, const void *data, size_t len)
{
    if (b->len + len > b->cap) {
        b->cap = (b->len + len) * 2 + 64;
        b->data = xrealloc(b->data, b->cap);
    }
    memcpy(b->data + b->len, data, len);
    b->len += len;
}

void buf_addstr(struct buf *b, const char *s)
{
    buf_add(b, s, strlen(s));
}
