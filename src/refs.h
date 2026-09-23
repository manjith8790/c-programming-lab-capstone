#ifndef REFS_H
#define REFS_H

/*
 * HEAD is either "ref: refs/heads/<branch>" (on a branch)
 * or a commit hash (detached, after checking out an old commit).
 * A branch file just holds the hash of its latest commit.
 */

/* copies the current branch name; returns 0 if HEAD is detached */
int head_branch(char *branch);

/* hash of the current commit; returns 0 if there are no commits yet */
int head_commit(char *hex);

/* move the current branch (or detached HEAD) to a new commit */
void update_head(const char *hex);

void set_head_branch(const char *branch);
void set_head_detached(const char *hex);

/* hash stored in refs/heads/<name>; returns 0 if there is none */
int read_branch(const char *name, char *hex);
int branch_exists(const char *name);

#endif
