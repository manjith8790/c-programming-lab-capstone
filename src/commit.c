#include "commit.h"
#include "object.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int commit_read(const char *hex, struct commit *c)
{
    enum obj_type type;
    size_t len;
    char *data = (char *)read_object(hex, &type, &len);
    if (!data)
        return -1;
    if (type != OBJ_COMMIT) {
        free(data);
        return -1;
    }

    memset(c, 0, sizeof(*c));

    /* header lines end at the first empty line; the message follows */
    char *line = data;
    while (*line && *line != '\n') {
        char *end = strchr(line, '\n');
        if (!end)
            break;
        *end = '\0';

        if (!strncmp(line, "tree ", 5))
            snprintf(c->tree, sizeof(c->tree), "%s", line + 5);
        else if (!strncmp(line, "parent ", 7) && !c->parent[0])
            snprintf(c->parent, sizeof(c->parent), "%s", line + 7);
        else if (!strncmp(line, "author ", 7)) {
            char *gt = strrchr(line, '>');
            if (gt) {
                c->time = strtol(gt + 1, NULL, 10);
                gt[1] = '\0';
            }
            snprintf(c->author, sizeof(c->author), "%s", line + 7);
        }
        line = end + 1;
    }

    if (*line == '\n')
        line++;
    size_t mlen = strlen(line);
    while (mlen > 0 && line[mlen - 1] == '\n')
        mlen--;

    c->message = xmalloc(mlen + 1);
    memcpy(c->message, line, mlen);
    c->message[mlen] = '\0';

    free(data);
    return 0;
}

void commit_free(struct commit *c)
{
    free(c->message);
    c->message = NULL;
}

void commit_write(const char *tree, const char *parent, const char *author,
                  const char *message, char *hex)
{
    struct buf b = { 0 };
    char line[512];
    long now = (long)time(NULL);

    snprintf(line, sizeof(line), "tree %s\n", tree);
    buf_addstr(&b, line);
    if (parent && parent[0]) {
        snprintf(line, sizeof(line), "parent %s\n", parent);
        buf_addstr(&b, line);
    }
    snprintf(line, sizeof(line), "author %s %ld +0000\n", author, now);
    buf_addstr(&b, line);
    snprintf(line, sizeof(line), "committer %s %ld +0000\n\n", author, now);
    buf_addstr(&b, line);
    buf_addstr(&b, message);
    buf_addstr(&b, "\n");

    write_object(OBJ_COMMIT, b.data, b.len, hex);
    free(b.data);
}
