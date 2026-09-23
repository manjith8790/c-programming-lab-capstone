#include "filelist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void fl_init(struct filelist *fl)
{
    fl->items = NULL;
    fl->count = fl->cap = 0;
}

void fl_free(struct filelist *fl)
{
    free(fl->items);
    fl_init(fl);
}

/* binary search; returns the position where path is or should be */
static int fl_pos(const struct filelist *fl, const char *path, int *found)
{
    int lo = 0, hi = fl->count;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        int cmp = strcmp(fl->items[mid].path, path);
        if (cmp == 0) {
            *found = 1;
            return mid;
        }
        if (cmp < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    *found = 0;
    return lo;
}

struct file_entry *fl_find(const struct filelist *fl, const char *path)
{
    int found;
    int pos = fl_pos(fl, path, &found);
    return found ? &fl->items[pos] : NULL;
}

void fl_set(struct filelist *fl, const char *path, const char *hex)
{
    if (strlen(path) >= PATH_LEN)
        die("path too long: %s", path);

    int found;
    int pos = fl_pos(fl, path, &found);
    if (!found) {
        if (fl->count == fl->cap) {
            fl->cap = fl->cap ? fl->cap * 2 : 16;
            fl->items = xrealloc(fl->items, fl->cap * sizeof(*fl->items));
        }
        memmove(&fl->items[pos + 1], &fl->items[pos],
                (fl->count - pos) * sizeof(*fl->items));
        fl->count++;
        strcpy(fl->items[pos].path, path);
    }
    strcpy(fl->items[pos].hex, hex);
}

void fl_remove(struct filelist *fl, const char *path)
{
    int found;
    int pos = fl_pos(fl, path, &found);
    if (!found)
        return;
    memmove(&fl->items[pos], &fl->items[pos + 1],
            (fl->count - pos - 1) * sizeof(*fl->items));
    fl->count--;
}

static void index_path(char *path)
{
    snprintf(path, PATH_LEN, "%s/index", REPO_DIR);
}

void index_load(struct filelist *fl)
{
    char path[PATH_LEN];
    size_t len;

    fl_init(fl);
    index_path(path);
    unsigned char *data = read_file(path, &len);
    if (!data)
        return;

    char *line = (char *)data;
    while (*line) {
        char *end = strchr(line, '\n');
        if (end)
            *end = '\0';

        /* "<40 hex> <path>" */
        if (strlen(line) > HEX_LEN + 1 && line[HEX_LEN] == ' ') {
            char hex[HEX_LEN + 1];
            memcpy(hex, line, HEX_LEN);
            hex[HEX_LEN] = '\0';
            if (is_full_hex(hex))
                fl_set(fl, line + HEX_LEN + 1, hex);
        }

        if (!end)
            break;
        line = end + 1;
    }
    free(data);
}

void index_save(const struct filelist *fl)
{
    char path[PATH_LEN];
    struct buf b = { 0 };

    for (int i = 0; i < fl->count; i++) {
        buf_addstr(&b, fl->items[i].hex);
        buf_addstr(&b, " ");
        buf_addstr(&b, fl->items[i].path);
        buf_addstr(&b, "\n");
    }

    index_path(path);
    write_file_atomic(path, b.data ? (void *)b.data : "", b.len);
    free(b.data);
}
