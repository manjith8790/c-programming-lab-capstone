#include "object.h"
#include "refs.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

static const char *names[] = { "blob", "tree", "commit" };

const char *type_name(enum obj_type type)
{
    return type < OBJ_BAD ? names[type] : "unknown";
}

static enum obj_type parse_type(const char *s)
{
    for (int t = 0; t < OBJ_BAD; t++)
        if (!strcmp(s, names[t]))
            return (enum obj_type)t;
    return OBJ_BAD;
}

void object_path(const char *hex, char *path)
{
    snprintf(path, PATH_LEN, "%s/objects/%.2s/%s", REPO_DIR, hex, hex + 2);
}

/* header + content in one buffer, exactly the bytes that get hashed */
static unsigned char *build_object(enum obj_type type, const void *data, size_t len,
                                   size_t *out_len)
{
    char header[32];
    int hlen = snprintf(header, sizeof(header), "%s %lu", type_name(type),
                        (unsigned long)len) + 1;   /* keep the '\0' */

    unsigned char *obj = xmalloc(hlen + len);
    memcpy(obj, header, hlen);
    memcpy(obj + hlen, data, len);
    *out_len = hlen + len;
    return obj;
}

void hash_object(enum obj_type type, const void *data, size_t len, char *hex)
{
    size_t n;
    unsigned char hash[HASH_LEN];
    unsigned char *obj = build_object(type, data, len, &n);

    sha1(obj, n, hash);
    hash_to_hex(hash, hex);
    free(obj);
}

int write_object(enum obj_type type, const void *data, size_t len, char *hex)
{
    size_t n;
    unsigned char hash[HASH_LEN];
    char path[PATH_LEN];
    unsigned char *obj = build_object(type, data, len, &n);

    sha1(obj, n, hash);
    hash_to_hex(hash, hex);
    object_path(hex, path);

    /* same content -> same name: nothing to do (deduplication) */
    if (file_exists(path)) {
        free(obj);
        return 0;
    }

    make_parent_dirs(path);
    write_file_atomic(path, obj, n);
    free(obj);
    return 1;
}

unsigned char *read_object(const char *hex, enum obj_type *type, size_t *len)
{
    char path[PATH_LEN];
    size_t n;

    object_path(hex, path);
    unsigned char *raw = read_file(path, &n);
    if (!raw)
        return NULL;

    unsigned char *nul = memchr(raw, '\0', n);
    char tname[16];
    unsigned long size;
    if (!nul || sscanf((char *)raw, "%15s %lu", tname, &size) != 2)
        die("object %s is damaged (bad header)", hex);

    size_t hlen = (size_t)(nul - raw) + 1;
    if (hlen + size != n)
        die("object %s is damaged (size does not match)", hex);

    *type = parse_type(tname);
    if (*type == OBJ_BAD)
        die("object %s has unknown type '%s'", hex, tname);

    unsigned char *content = xmalloc(size + 1);
    memcpy(content, raw + hlen, size);
    content[size] = '\0';
    free(raw);

    *len = size;
    return content;
}

int verify_object(const char *hex)
{
    char path[PATH_LEN];
    char actual[HEX_LEN + 1];
    unsigned char hash[HASH_LEN];
    size_t n;

    object_path(hex, path);
    unsigned char *raw = read_file(path, &n);
    if (!raw)
        return -1;

    sha1(raw, n, hash);
    hash_to_hex(hash, actual);
    free(raw);
    return strcmp(actual, hex) == 0;
}

void for_each_object(object_fn fn, void *ctx)
{
    char dir[PATH_LEN];
    snprintf(dir, sizeof(dir), "%s/objects", REPO_DIR);

    DIR *top = opendir(dir);
    if (!top)
        return;

    struct dirent *de;
    while ((de = readdir(top)) != NULL) {
        if (strlen(de->d_name) != 2 || de->d_name[0] == '.')
            continue;

        char sub[PATH_LEN];
        snprintf(sub, sizeof(sub), "%s/objects/%.2s", REPO_DIR, de->d_name);
        DIR *d = opendir(sub);
        if (!d)
            continue;

        struct dirent *fe;
        while ((fe = readdir(d)) != NULL) {
            char hex[HEX_LEN + 1];
            if (strlen(fe->d_name) != HEX_LEN - 2)
                continue;
            snprintf(hex, sizeof(hex), "%.2s%.38s", de->d_name, fe->d_name);
            if (is_full_hex(hex))
                fn(hex, ctx);
        }
        closedir(d);
    }
    closedir(top);
}

struct prefix_search {
    const char *prefix;
    char found[HEX_LEN + 1];
    int matches;
};

static void match_prefix(const char *hex, void *ctx)
{
    struct prefix_search *s = ctx;
    if (!strncmp(hex, s->prefix, strlen(s->prefix))) {
        strcpy(s->found, hex);
        s->matches++;
    }
}

int resolve(const char *name, char *hex)
{
    if (!strcmp(name, "HEAD"))
        return head_commit(hex);

    if (read_branch(name, hex))
        return 1;

    size_t len = strlen(name);
    if (len < 4 || len > HEX_LEN || strspn(name, "0123456789abcdef") != len)
        return 0;

    struct prefix_search s = { name, "", 0 };
    for_each_object(match_prefix, &s);
    if (s.matches > 1)
        die("short hash '%s' is ambiguous, use more characters", name);
    if (s.matches == 0)
        return 0;

    strcpy(hex, s.found);
    return 1;
}
