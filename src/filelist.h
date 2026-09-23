#ifndef FILELIST_H
#define FILELIST_H

#include "sha1.h"
#include "util.h"

/*
 * A sorted list of (path, blob hash) pairs.
 * Used for the index (staging area) and for the flattened
 * contents of a commit's tree.
 */
struct file_entry {
    char path[PATH_LEN];
    char hex[HEX_LEN + 1];
};

struct filelist {
    struct file_entry *items;
    int count, cap;
};

void fl_init(struct filelist *fl);
void fl_free(struct filelist *fl);

struct file_entry *fl_find(const struct filelist *fl, const char *path);

/* add or update an entry, keeping the list sorted by path */
void fl_set(struct filelist *fl, const char *path, const char *hex);
void fl_remove(struct filelist *fl, const char *path);

/* the index file: one "<hash> <path>" line per staged file */
void index_load(struct filelist *fl);
void index_save(const struct filelist *fl);

#endif
