#include "tree.h"
#include "object.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void add_entry(struct buf *b, const char *mode, const char *name, const char *hex)
{
    unsigned char hash[HASH_LEN];
    if (hex_to_hash(hex, hash) != 0)
        die("bad hash %s", hex);

    buf_addstr(b, mode);
    buf_addstr(b, " ");
    buf_add(b, name, strlen(name) + 1);     /* name including its '\0' */
    buf_add(b, hash, HASH_LEN);
}

/*
 * Write the tree for files[start..end). All of them share the same
 * directory prefix of length plen (e.g. "src/" -> plen 4).
 *
 * Because the list is sorted by full path, files of one sub-directory are
 * next to each other, and the order already matches Git's tree order
 * (a directory "a" sorts as if it were "a/").
 */
static void write_level(const struct filelist *files, int start, int end,
                        size_t plen, char *hex)
{
    struct buf b = { 0 };
    int i = start;

    while (i < end) {
        const char *name = files->items[i].path + plen;
        const char *slash = strchr(name, '/');

        if (!slash) {
            add_entry(&b, "100644", name, files->items[i].hex);
            i++;
            continue;
        }

        /* sub-directory: collect every entry that starts with "<dir>/" */
        size_t dlen = (size_t)(slash - name);
        int j = i;
        while (j < end && !strncmp(files->items[j].path + plen, name, dlen + 1))
            j++;

        char dirname[PATH_LEN], subhex[HEX_LEN + 1];
        memcpy(dirname, name, dlen);
        dirname[dlen] = '\0';

        write_level(files, i, j, plen + dlen + 1, subhex);
        add_entry(&b, "40000", dirname, subhex);
        i = j;
    }

    write_object(OBJ_TREE, b.data ? (void *)b.data : "", b.len, hex);
    free(b.data);
}

void write_tree(const struct filelist *files, char *hex)
{
    write_level(files, 0, files->count, 0, hex);
}

static void read_level(const char *hex, const char *prefix, struct filelist *out)
{
    enum obj_type type;
    size_t len;
    unsigned char *data = read_object(hex, &type, &len);
    if (!data)
        die("tree %s is missing", hex);
    if (type != OBJ_TREE)
        die("%s is a %s, not a tree", hex, type_name(type));

    unsigned char *p = data, *end = data + len;
    while (p < end) {
        char *mode = (char *)p;
        char *space = memchr(p, ' ', end - p);
        if (!space)
            die("tree %s is damaged", hex);
        *space = '\0';

        char *name = space + 1;
        unsigned char *nul = memchr(name, '\0', end - (unsigned char *)name);
        if (!nul || nul + 1 + HASH_LEN > end)
            die("tree %s is damaged", hex);

        char child[HEX_LEN + 1], path[PATH_LEN];
        hash_to_hex(nul + 1, child);
        snprintf(path, sizeof(path), "%s%s", prefix, name);

        if (!strcmp(mode, "40000")) {
            strcat(path, "/");
            read_level(child, path, out);
        } else {
            fl_set(out, path, child);
        }
        p = nul + 1 + HASH_LEN;
    }
    free(data);
}

void read_tree(const char *hex, struct filelist *out)
{
    read_level(hex, "", out);
}
