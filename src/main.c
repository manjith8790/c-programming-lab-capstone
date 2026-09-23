/*
 * minigit - a small content-addressable version control system.
 *
 * Objects (blobs, trees, commits) are stored under the SHA-1 of their
 * content, in the same format Git uses, so identical content is stored
 * only once and any change to a stored object can be detected.
 */
#include "commit.h"
#include "filelist.h"
#include "object.h"
#include "refs.h"
#include "tree.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_BRANCH "main"

static void short_hex(const char *hex, char *out)
{
    memcpy(out, hex, 7);
    out[7] = '\0';
}

static void require_repo(void)
{
    if (!is_dir(REPO_DIR))
        die("not a minigit repository (run 'minigit init' first)");
}

/* strip a leading "./" so "./a.txt" and "a.txt" are the same path */
static const char *clean_path(const char *path)
{
    while (path[0] == '.' && path[1] == '/')
        path += 2;
    return path;
}

/* ------------------------------------------------------------ init */

static int cmd_init(void)
{
    char path[PATH_LEN];

    if (is_dir(REPO_DIR)) {
        printf("Repository already exists in %s/\n", REPO_DIR);
        return 0;
    }

    make_dir(REPO_DIR);
    snprintf(path, sizeof(path), "%s/objects", REPO_DIR);
    make_dir(path);
    snprintf(path, sizeof(path), "%s/refs", REPO_DIR);
    make_dir(path);
    snprintf(path, sizeof(path), "%s/refs/heads", REPO_DIR);
    make_dir(path);
    set_head_branch(DEFAULT_BRANCH);

    printf("Initialized empty minigit repository in %s/\n", REPO_DIR);
    return 0;
}

/* ------------------------------------------------------------ add */

static void add_file(const char *path, void *ctx)
{
    struct filelist *index = ctx;
    char hex[HEX_LEN + 1], sh[8];
    size_t len;

    path = clean_path(path);
    unsigned char *data = read_file(path, &len);
    if (!data)
        die("cannot read %s", path);

    int is_new = write_object(OBJ_BLOB, data, len, hex);
    free(data);

    struct file_entry *old = fl_find(index, path);
    if (old && !strcmp(old->hex, hex))
        return;                 /* unchanged, nothing to report */

    fl_set(index, path, hex);
    short_hex(hex, sh);
    if (is_new)
        printf("  stored  %s  %s\n", sh, path);
    else
        printf("  reused  %s  %s  (same content already stored)\n", sh, path);
}

/* files that were deleted from disk inside dir are removed from the index */
static void stage_deletions(struct filelist *index, const char *dir)
{
    size_t dlen = strlen(dir);

    for (int i = index->count - 1; i >= 0; i--) {
        const char *p = index->items[i].path;
        int inside = !strcmp(dir, ".") ||
                     (!strncmp(p, dir, dlen) && p[dlen] == '/');
        if (inside && !file_exists(p)) {
            printf("  removed          %s\n", p);
            fl_remove(index, p);
        }
    }
}

static int cmd_add(int argc, char **argv)
{
    struct filelist index;

    if (argc < 1)
        die("usage: minigit add <file|dir>...");

    index_load(&index);
    for (int i = 0; i < argc; i++) {
        const char *path = clean_path(argv[i]);
        if (!*path)
            path = ".";

        if (is_dir(path)) {
            walk_files(path, add_file, &index);
            stage_deletions(&index, path);
        } else if (file_exists(path)) {
            add_file(path, &index);
        } else if (fl_find(&index, path)) {
            printf("  removed          %s\n", path);
            fl_remove(&index, path);
        } else {
            die("no such file: %s", path);
        }
    }
    index_save(&index);
    fl_free(&index);
    return 0;
}

/* ------------------------------------------------------------ commit */

static int cmd_commit(int argc, char **argv)
{
    const char *message = NULL;
    for (int i = 0; i < argc; i++)
        if (!strcmp(argv[i], "-m") && i + 1 < argc)
            message = argv[++i];
    if (!message || !*message)
        die("usage: minigit commit -m \"message\"");

    struct filelist index;
    index_load(&index);

    char tree[HEX_LEN + 1], parent[HEX_LEN + 1] = "";
    int has_parent = head_commit(parent);

    if (index.count == 0 && !has_parent) {
        fl_free(&index);
        printf("nothing to commit (use 'minigit add <file>' first)\n");
        return 1;
    }

    write_tree(&index, tree);
    fl_free(&index);

    if (has_parent) {
        struct commit prev;
        if (commit_read(parent, &prev) == 0) {
            int same = !strcmp(prev.tree, tree);
            commit_free(&prev);
            if (same) {
                printf("nothing to commit, no changes since the last commit\n");
                return 1;
            }
        }
    }

    const char *author = getenv("MINIGIT_AUTHOR");
    if (!author || !*author)
        author = "minigit user <user@localhost>";

    char hex[HEX_LEN + 1], sh[8], branch[PATH_LEN];
    commit_write(tree, parent, author, message, hex);
    update_head(hex);

    short_hex(hex, sh);
    if (!head_branch(branch))
        strcpy(branch, "detached HEAD");
    printf("[%s %s] %s\n", branch, sh, message);
    return 0;
}

/* ------------------------------------------------------------ log */

static int cmd_log(int argc, char **argv)
{
    int oneline = 0;
    const char *start = "HEAD";
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--oneline"))
            oneline = 1;
        else
            start = argv[i];
    }

    char hex[HEX_LEN + 1];
    if (!resolve(start, hex)) {
        if (strcmp(start, "HEAD"))
            die("unknown branch or commit: %s", start);
        printf("no commits yet\n");
        return 0;
    }

    while (hex[0]) {
        struct commit c;
        if (commit_read(hex, &c) != 0)
            die("%s is not a commit", hex);

        if (oneline) {
            char sh[8];
            short_hex(hex, sh);
            printf("%s %s\n", sh, strtok(c.message, "\n") ? c.message : "");
        } else {
            char date[64];
            time_t t = (time_t)c.time;
            strftime(date, sizeof(date), "%a %b %d %H:%M:%S %Y UTC", gmtime(&t));
            printf("commit %s\nAuthor: %s\nDate:   %s\n\n    %s\n\n",
                   hex, c.author, date, c.message);
        }

        strcpy(hex, c.parent);
        commit_free(&c);
    }
    return 0;
}

/* ------------------------------------------------------------ status */

struct status {
    struct filelist head;      /* files in the last commit */
    struct filelist index;     /* files staged for the next commit */
    struct filelist work;      /* files on disk (hash computed, not stored) */
};

static void collect_work(const char *path, void *ctx)
{
    struct filelist *work = ctx;
    char hex[HEX_LEN + 1];
    size_t len;

    unsigned char *data = read_file(path, &len);
    if (!data)
        return;
    hash_object(OBJ_BLOB, data, len, hex);
    free(data);
    fl_set(work, clean_path(path), hex);
}

static void status_load(struct status *s)
{
    char hex[HEX_LEN + 1];

    fl_init(&s->head);
    fl_init(&s->work);
    index_load(&s->index);

    if (head_commit(hex)) {
        struct commit c;
        if (commit_read(hex, &c) != 0)
            die("HEAD points to a missing commit");
        read_tree(c.tree, &s->head);
        commit_free(&c);
    }
    walk_files(".", collect_work, &s->work);
}

static void status_free(struct status *s)
{
    fl_free(&s->head);
    fl_free(&s->index);
    fl_free(&s->work);
}

/* staged = differences between the last commit and the index */
static int print_staged(const struct status *s, int print)
{
    int n = 0;
    for (int i = 0; i < s->index.count; i++) {
        const struct file_entry *e = &s->index.items[i];
        const struct file_entry *h = fl_find(&s->head, e->path);
        if (!h || strcmp(h->hex, e->hex)) {
            if (print)
                printf("        %-10s %s\n", h ? "modified:" : "new file:", e->path);
            n++;
        }
    }
    for (int i = 0; i < s->head.count; i++) {
        if (!fl_find(&s->index, s->head.items[i].path)) {
            if (print)
                printf("        %-10s %s\n", "deleted:", s->head.items[i].path);
            n++;
        }
    }
    return n;
}

/* unstaged = differences between the index and the files on disk */
static int print_unstaged(const struct status *s, int print)
{
    int n = 0;
    for (int i = 0; i < s->index.count; i++) {
        const struct file_entry *e = &s->index.items[i];
        const struct file_entry *w = fl_find(&s->work, e->path);
        if (!w || strcmp(w->hex, e->hex)) {
            if (print)
                printf("        %-10s %s\n", w ? "modified:" : "deleted:", e->path);
            n++;
        }
    }
    return n;
}

static int cmd_status(void)
{
    struct status s;
    char branch[PATH_LEN], hex[HEX_LEN + 1], sh[8];

    status_load(&s);

    if (head_branch(branch))
        printf("On branch %s\n", branch);
    else if (head_commit(hex)) {
        short_hex(hex, sh);
        printf("HEAD detached at %s\n", sh);
    }
    if (!head_commit(hex))
        printf("No commits yet\n");

    int staged = print_staged(&s, 0);
    int unstaged = print_unstaged(&s, 0);
    int untracked = 0;

    if (staged) {
        printf("\nChanges to be committed:\n");
        print_staged(&s, 1);
    }
    if (unstaged) {
        printf("\nChanges not staged for commit:\n");
        print_unstaged(&s, 1);
    }
    for (int i = 0; i < s.work.count; i++) {
        if (!fl_find(&s.index, s.work.items[i].path)) {
            if (!untracked++)
                printf("\nUntracked files:\n");
            printf("        %s\n", s.work.items[i].path);
        }
    }
    if (!staged && !unstaged && !untracked)
        printf("nothing to commit, working tree clean\n");

    status_free(&s);
    return 0;
}

/* ------------------------------------------------------------ checkout */

static int cmd_checkout(int argc, char **argv)
{
    if (argc != 1)
        die("usage: minigit checkout <branch|commit>");

    const char *target = argv[0];
    int to_branch = branch_exists(target);
    char hex[HEX_LEN + 1];
    if (!resolve(target, hex))
        die("unknown branch or commit: %s", target);

    struct commit c;
    if (commit_read(hex, &c) != 0)
        die("%s is not a commit", target);

    /* refuse to overwrite work that is not committed yet */
    struct status s;
    status_load(&s);
    if (print_staged(&s, 0) || print_unstaged(&s, 0)) {
        status_free(&s);
        commit_free(&c);
        die("you have uncommitted changes; commit them before checkout");
    }

    struct filelist target_files;
    fl_init(&target_files);
    read_tree(c.tree, &target_files);

    /* remove tracked files that do not exist in the target commit */
    for (int i = 0; i < s.index.count; i++)
        if (!fl_find(&target_files, s.index.items[i].path))
            remove_file(s.index.items[i].path);

    /* write every file of the target commit from its blob */
    for (int i = 0; i < target_files.count; i++) {
        const struct file_entry *e = &target_files.items[i];
        const struct file_entry *w = fl_find(&s.work, e->path);
        if (w && !strcmp(w->hex, e->hex))
            continue;           /* already has the right content */

        enum obj_type type;
        size_t len;
        unsigned char *data = read_object(e->hex, &type, &len);
        if (!data || type != OBJ_BLOB)
            die("blob %s for %s is missing", e->hex, e->path);
        make_parent_dirs(e->path);
        write_file_atomic(e->path, data, len);
        free(data);
    }

    index_save(&target_files);

    char sh[8];
    short_hex(hex, sh);
    if (to_branch) {
        set_head_branch(target);
        printf("Switched to branch '%s' (%s %s)\n", target, sh, c.message);
    } else {
        set_head_detached(hex);
        printf("HEAD is now at %s %s\n", sh, c.message);
        printf("(detached HEAD: run 'minigit checkout %s' to go back)\n", DEFAULT_BRANCH);
    }

    fl_free(&target_files);
    status_free(&s);
    commit_free(&c);
    return 0;
}

/* ------------------------------------------------------------ hash-object */

static int cmd_hash_object(int argc, char **argv)
{
    int write = 0;
    const char *file = NULL;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-w"))
            write = 1;
        else
            file = argv[i];
    }
    if (!file)
        die("usage: minigit hash-object [-w] <file>");

    size_t len;
    char hex[HEX_LEN + 1];
    unsigned char *data = read_file(file, &len);
    if (!data)
        die("cannot read %s", file);

    if (write) {
        require_repo();
        write_object(OBJ_BLOB, data, len, hex);
    } else {
        hash_object(OBJ_BLOB, data, len, hex);
    }
    printf("%s\n", hex);
    free(data);
    return 0;
}

/* ------------------------------------------------------------ cat-file */

static void print_tree(const unsigned char *data, size_t len)
{
    const unsigned char *p = data, *end = data + len;
    while (p < end) {
        const char *mode = (const char *)p;
        const char *name = strchr(mode, ' ') + 1;
        const unsigned char *hash = (const unsigned char *)name + strlen(name) + 1;
        char hex[HEX_LEN + 1];
        hash_to_hex(hash, hex);

        int is_tree = !strncmp(mode, "40000", 5);
        printf("%06d %s %s    %s\n", atoi(mode), is_tree ? "tree" : "blob", hex, name);
        p = hash + HASH_LEN;
    }
}

static int cmd_cat_file(int argc, char **argv)
{
    char mode = 'p';
    const char *name = NULL;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "-s") || !strcmp(argv[i], "-p"))
            mode = argv[i][1];
        else
            name = argv[i];
    }
    if (!name)
        die("usage: minigit cat-file [-t | -s | -p] <object>");

    char hex[HEX_LEN + 1];
    if (!resolve(name, hex))
        die("no such object: %s", name);

    enum obj_type type;
    size_t len;
    unsigned char *data = read_object(hex, &type, &len);
    if (!data)
        die("no such object: %s", name);

    if (mode == 't')
        printf("%s\n", type_name(type));
    else if (mode == 's')
        printf("%lu\n", (unsigned long)len);
    else if (type == OBJ_TREE)
        print_tree(data, len);
    else
        fwrite(data, 1, len, stdout);

    free(data);
    return 0;
}

/* ------------------------------------------------------------ verify */

struct verify_count {
    int ok, bad;
};

static void verify_one(const char *hex, void *ctx)
{
    struct verify_count *vc = ctx;
    if (verify_object(hex) == 1) {
        vc->ok++;
    } else {
        vc->bad++;
        printf("  CORRUPTED  %s\n", hex);
    }
}

static int cmd_verify(int argc, char **argv)
{
    if (argc == 1) {
        char hex[HEX_LEN + 1], path[PATH_LEN];
        if (!resolve(argv[0], hex))
            die("no such object: %s", argv[0]);

        if (verify_object(hex) == 1) {
            printf("OK: %s is intact\n", hex);
            return 0;
        }

        /* show what the content hashes to now */
        size_t n;
        unsigned char h[HASH_LEN];
        char actual[HEX_LEN + 1];
        object_path(hex, path);
        unsigned char *raw = read_file(path, &n);
        sha1(raw, n, h);
        hash_to_hex(h, actual);
        free(raw);
        printf("CORRUPTED: %s\n  expected  %s\n  actual    %s\n", hex, hex, actual);
        return 1;
    }

    struct verify_count vc = { 0, 0 };
    for_each_object(verify_one, &vc);
    printf("checked %d objects: %d ok, %d corrupted\n", vc.ok + vc.bad, vc.ok, vc.bad);
    return vc.bad ? 1 : 0;
}

/* ------------------------------------------------------------ stats */

struct store_stats {
    int count[OBJ_BAD];
    long disk_bytes;
};

static void count_object(const char *hex, void *ctx)
{
    struct store_stats *st = ctx;
    char path[PATH_LEN];
    enum obj_type type;
    size_t len;

    unsigned char *data = read_object(hex, &type, &len);
    if (!data)
        return;
    free(data);
    st->count[type]++;
    object_path(hex, path);
    st->disk_bytes += file_size(path);
}

static int cmd_stats(void)
{
    struct store_stats st;
    memset(&st, 0, sizeof(st));
    for_each_object(count_object, &st);

    printf("Objects stored:   %d  (%d blobs, %d trees, %d commits)\n",
           st.count[OBJ_BLOB] + st.count[OBJ_TREE] + st.count[OBJ_COMMIT],
           st.count[OBJ_BLOB], st.count[OBJ_TREE], st.count[OBJ_COMMIT]);
    printf("Size on disk:     %ld bytes\n", st.disk_bytes);

    /* walk the history and compare "full copy of every version"
     * with "each unique content once" */
    char hex[HEX_LEN + 1];
    if (!head_commit(hex))
        return 0;

    struct filelist seen;
    fl_init(&seen);
    long full_copies = 0, unique = 0;
    int commits = 0, versions = 0;

    while (hex[0]) {
        struct commit c;
        struct filelist files;
        if (commit_read(hex, &c) != 0)
            break;
        commits++;

        fl_init(&files);
        read_tree(c.tree, &files);
        for (int i = 0; i < files.count; i++) {
            enum obj_type type;
            size_t len;
            unsigned char *data = read_object(files.items[i].hex, &type, &len);
            if (!data)
                continue;
            free(data);

            versions++;
            full_copies += (long)len;
            if (!fl_find(&seen, files.items[i].hex)) {
                fl_set(&seen, files.items[i].hex, files.items[i].hex);
                unique += (long)len;
            }
        }
        fl_free(&files);
        strcpy(hex, c.parent);
        commit_free(&c);
    }

    printf("History:          %d commits, %d file versions\n", commits, versions);
    printf("Full copies:      %ld bytes (every version saved separately)\n", full_copies);
    printf("With CAS:         %ld bytes (each unique content saved once)\n", unique);
    if (full_copies > 0)
        printf("Saved:            %.1f%%\n", 100.0 * (full_copies - unique) / full_copies);

    fl_free(&seen);
    return 0;
}

/* ------------------------------------------------------------ main */

static void usage(void)
{
    printf("usage: minigit <command> [arguments]\n\n"
           "  init                        create an empty repository\n"
           "  add <file|dir>...           stage files for the next commit\n"
           "  commit -m <message>         save a snapshot of the staged files\n"
           "  log [--oneline] [commit]    show the commit history\n"
           "  status                      show changed and untracked files\n"
           "  checkout <branch|commit>    restore the files of a commit\n"
           "  hash-object [-w] <file>     print the hash of a file (-w: store it)\n"
           "  cat-file [-t|-s|-p] <obj>   show an object's type, size or content\n"
           "  verify [obj]                check objects for corruption\n"
           "  stats                       show storage and deduplication numbers\n");
}

int main(int argc, char **argv)
{
    if (argc < 2 || !strcmp(argv[1], "help") || !strcmp(argv[1], "--help")) {
        usage();
        return argc < 2 ? 1 : 0;
    }

    const char *cmd = argv[1];
    int n = argc - 2;
    char **args = argv + 2;

    if (!strcmp(cmd, "init"))
        return cmd_init();
    if (!strcmp(cmd, "hash-object"))
        return cmd_hash_object(n, args);

    require_repo();
    if (!strcmp(cmd, "add"))
        return cmd_add(n, args);
    if (!strcmp(cmd, "commit"))
        return cmd_commit(n, args);
    if (!strcmp(cmd, "log"))
        return cmd_log(n, args);
    if (!strcmp(cmd, "status"))
        return cmd_status();
    if (!strcmp(cmd, "checkout"))
        return cmd_checkout(n, args);
    if (!strcmp(cmd, "cat-file"))
        return cmd_cat_file(n, args);
    if (!strcmp(cmd, "verify"))
        return cmd_verify(n, args);
    if (!strcmp(cmd, "stats"))
        return cmd_stats();

    fprintf(stderr, "minigit: unknown command '%s'\n\n", cmd);
    usage();
    return 1;
}
