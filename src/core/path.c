#include "path.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int inside(const char *root, const char *p)
{
    size_t n = strlen(root);
    if (strncmp(root, p, n) != 0)
        return 0;
    return p[n] == '\0' || p[n] == '/';
}

static int has_dotdot(const char *rel)
{
    const char *p = rel;
    while (*p) {
        while (*p == '/')
            p++;
        const char *s = p;
        while (*p && *p != '/')
            p++;
        if (p - s == 2 && s[0] == '.' && s[1] == '.')
            return 1;
    }
    return 0;
}

int path_resolve(const char *root, const char *rel, char *out, size_t outsz)
{
    char joined[PATH_MAX], resolved[PATH_MAX];
    int n;

    if (!root || !rel || !out || outsz == 0)
        return PATH_EINVAL;
    if (root[0] != '/' || strcmp(root, "/") == 0)
        return PATH_EINVAL;
    if (has_dotdot(rel))
        return PATH_EINVAL;

    n = snprintf(joined, sizeof joined, "%s/%s", root, rel);
    if (n < 0 || (size_t)n >= sizeof joined)
        return PATH_ETOOLONG;

    if (realpath(joined, resolved)) {
        if (!inside(root, resolved))
            return PATH_EINVAL;
    } else {
        struct stat st;
        char parent[PATH_MAX];
        size_t len;
        char *slash;
        const char *base;

        if (errno == ENAMETOOLONG)
            return PATH_ETOOLONG;
        if (errno != ENOENT)
            return PATH_ENOENT;
        /* realpath упал с ENOENT, но запись есть: битый симлинк */
        if (lstat(joined, &st) == 0)
            return PATH_EINVAL;

        len = strlen(joined);
        while (len > 1 && joined[len - 1] == '/')
            joined[--len] = '\0';
        slash = strrchr(joined, '/');
        if (!slash)
            return PATH_EINVAL;
        base = slash + 1;
        if (*base == '\0' || strcmp(base, ".") == 0)
            return PATH_EINVAL;
        *slash = '\0';
        if (!realpath(joined, parent))
            return PATH_ENOENT;
        n = snprintf(resolved, sizeof resolved, "%s/%s", parent, base);
        if (n < 0 || (size_t)n >= sizeof resolved)
            return PATH_ETOOLONG;
        if (!inside(root, resolved))
            return PATH_EINVAL;
    }

    if (strlen(resolved) >= outsz)
        return PATH_ETOOLONG;
    strcpy(out, resolved);
    return PATH_OK;
}