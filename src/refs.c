#include "refs.h"
#include "sha1.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REF_PREFIX "ref: refs/heads/"

static void head_path(char *path)
{
    snprintf(path, PATH_LEN, "%s/HEAD", REPO_DIR);
}

static void branch_path(const char *name, char *path)
{
    snprintf(path, PATH_LEN, "%s/refs/heads/%.200s", REPO_DIR, name);
}

/* first line of a small text file, without the newline */
static int read_line(const char *path, char *out, size_t size)
{
    size_t len;
    unsigned char *data = read_file(path, &len);
    if (!data)
        return 0;

    data[strcspn((char *)data, "\r\n")] = '\0';
    snprintf(out, size, "%s", (char *)data);
    free(data);
    return 1;
}

int head_branch(char *branch)
{
    char path[PATH_LEN], line[PATH_LEN];
    head_path(path);
    if (!read_line(path, line, sizeof(line)))
        die("HEAD is missing, is this a minigit repository?");

    if (strncmp(line, REF_PREFIX, strlen(REF_PREFIX)) != 0)
        return 0;
    strcpy(branch, line + strlen(REF_PREFIX));
    return 1;
}

int read_branch(const char *name, char *hex)
{
    char path[PATH_LEN], line[PATH_LEN];

    if (strchr(name, '/') || strchr(name, '.'))
        return 0;
    branch_path(name, path);
    if (!read_line(path, line, sizeof(line)) || !is_full_hex(line))
        return 0;
    strcpy(hex, line);
    return 1;
}

int branch_exists(const char *name)
{
    char hex[HEX_LEN + 1];
    return read_branch(name, hex);
}

int head_commit(char *hex)
{
    char branch[PATH_LEN], path[PATH_LEN], line[PATH_LEN];

    if (head_branch(branch))
        return read_branch(branch, hex);

    head_path(path);
    if (!read_line(path, line, sizeof(line)) || !is_full_hex(line))
        die("HEAD is damaged");
    strcpy(hex, line);
    return 1;
}

static void write_line(const char *path, const char *text)
{
    char line[PATH_LEN];
    int n = snprintf(line, sizeof(line), "%s\n", text);
    write_file_atomic(path, line, (size_t)n);
}

void update_head(const char *hex)
{
    char branch[PATH_LEN], path[PATH_LEN];

    if (head_branch(branch))
        branch_path(branch, path);
    else
        head_path(path);
    write_line(path, hex);
}

void set_head_branch(const char *branch)
{
    char path[PATH_LEN], line[PATH_LEN];
    head_path(path);
    snprintf(line, sizeof(line), REF_PREFIX "%s", branch);
    write_line(path, line);
}

void set_head_detached(const char *hex)
{
    char path[PATH_LEN];
    head_path(path);
    write_line(path, hex);
}
