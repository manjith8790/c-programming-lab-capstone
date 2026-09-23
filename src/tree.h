#ifndef TREE_H
#define TREE_H

#include "filelist.h"

/*
 * A tree object is one directory. Each entry is stored as
 *     "<mode> <name>\0<20 raw hash bytes>"
 * mode 100644 = file (blob), 40000 = sub-directory (tree).
 * This is the same binary format Git uses, so tree hashes match Git.
 */

/* build tree objects for every directory in the list; returns the root hash */
void write_tree(const struct filelist *files, char *hex);

/* flatten a tree (and all its sub-trees) into "dir/file" -> blob hash */
void read_tree(const char *hex, struct filelist *out);

#endif
