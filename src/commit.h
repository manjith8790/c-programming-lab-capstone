#ifndef COMMIT_H
#define COMMIT_H

#include "sha1.h"

/*
 * A commit object is plain text:
 *     tree <hash>
 *     parent <hash>          (missing for the first commit)
 *     author Name <email> <unix time> +0000
 *     committer Name <email> <unix time> +0000
 *
 *     message
 */
struct commit {
    char tree[HEX_LEN + 1];
    char parent[HEX_LEN + 1];   /* "" if this is the first commit */
    char author[256];           /* "Name <email>" */
    long time;
    char *message;
};

/* returns 0 on success, -1 if the object is missing or not a commit */
int commit_read(const char *hex, struct commit *c);
void commit_free(struct commit *c);

void commit_write(const char *tree, const char *parent, const char *author,
                  const char *message, char *hex);

#endif
