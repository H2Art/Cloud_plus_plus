#include "cleanup.h"
#include "fileops.h"
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_DEPTH 64

static int walk(const char *dir, int depth)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    char full[PATH_MAX];
    struct stat st;
    int removed = 0;

    if (!d)
        return 0;
    while ((e = readdir(d)) != NULL) {
        int n;
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        n = snprintf(full, sizeof full, "%s/%s", dir, e->d_name);
        if (n < 0 || (size_t)n >= sizeof full)
            continue;
        if (lstat(full, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode)) {
            if (depth < MAX_DEPTH)
                removed += walk(full, depth + 1);
        } else if ((S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) &&
                   fileops_is_tmp_name(e->d_name)) {
            if (unlink(full) == 0)
                removed++;
        }
    }
    closedir(d);
    return removed;
}

int cleanup_tmp(const char *root)
{
    DIR *d;
    if (!root)
        return -1;
    d = opendir(root);
    if (!d)
        return -1;
    closedir(d);
    return walk(root, 0);
}
