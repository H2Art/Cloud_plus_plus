#include "core/fileops.h"
#include "test.h"
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[PATH_MAX], root[PATH_MAX + 8];

static char *P(const char *rel)
{
    static char b[4][2 * PATH_MAX];
    static int i;
    char *p = b[i++ & 3];
    snprintf(p, sizeof b[0], "%s/%s", root, rel);
    return p;
}
static void must(int ok, const char *what) { if (!ok) { perror(what); exit(2); } }

int main(void)
{
    char tmpl[] = "/tmp/fotmpXXXXXX", cmd[2 * PATH_MAX];
    char dir[64], name[NAME_MAX + 1], t1[NAME_MAX + 1], t2[NAME_MAX + 1];
    int fd = -1;
    FILE *f;

    must(mkdtemp(tmpl) != NULL, "mkdtemp");
    must(realpath(tmpl, base) != NULL, "realpath");
    snprintf(root, sizeof root, "%s/root", base);
    must(mkdir(root, 0700) == 0, "mkdir");
    must(mkdir(P("docs"), 0700) == 0, "mkdir docs");
    must(symlink(base, P("evil")) == 0, "symlink");

    /* is_tmp / valid_name */
    CHECK(fileops_is_tmp_name(".part-1-0"));
    CHECK(!fileops_is_tmp_name("a.part-1"));
    CHECK(!fileops_is_tmp_name(NULL));
    CHECK(!fileops_valid_name(".part-1-0"));
    CHECK(fileops_valid_name(".part"));

    /* split */
    CHECK(fileops_split("a/b/c.txt", dir, sizeof dir, name, sizeof name) == FOP_OK
          && !strcmp(dir, "a/b") && !strcmp(name, "c.txt"));
    CHECK(fileops_split("c.txt", dir, sizeof dir, name, sizeof name) == FOP_OK
          && !strcmp(dir, "") && !strcmp(name, "c.txt"));
    CHECK(fileops_split("/x/", dir, sizeof dir, name, sizeof name) == FOP_OK
          && !strcmp(dir, "") && !strcmp(name, "x"));
    CHECK(fileops_split("a/..", dir, sizeof dir, name, sizeof name) == FOP_EINVAL);
    CHECK(fileops_split("", dir, sizeof dir, name, sizeof name) == FOP_EINVAL);
    CHECK(fileops_split("a/.part-1", dir, sizeof dir, name, sizeof name) == FOP_EINVAL);
    CHECK(fileops_split("a/bcdef", dir, sizeof dir, name, 3) == FOP_EINVAL);
    CHECK(fileops_split(NULL, dir, sizeof dir, name, sizeof name) == FOP_EINVAL);

    /* exists */
    f = fopen(P("docs/a.txt"), "w"); must(f != NULL, "fopen"); fclose(f);
    CHECK(fileops_exists(root, "docs/a.txt") == 1);
    CHECK(fileops_exists(root, "docs") == 1);
    CHECK(fileops_exists(root, "docs/none") == 0);
    CHECK(fileops_exists(root, "evil") == 1);               /* сам симлинк */
    CHECK(fileops_exists(root, "nodir/x") == FOP_ENOENT);
    CHECK(fileops_exists(root, "../x") == FOP_EINVAL);
    CHECK(fileops_exists(root, "evil/x") == FOP_EINVAL);
    CHECK(fileops_exists(root, "") == FOP_EINVAL);

    /* create_tmp / commit / discard */
    CHECK(fileops_create_tmp(root, "docs", &fd, t1, sizeof t1) == FOP_OK && fd >= 0);
    CHECK(fileops_is_tmp_name(t1));
    CHECK(write(fd, "data", 4) == 4);
    close(fd);
    CHECK(fileops_create_tmp(root, "docs", &fd, t2, sizeof t2) == FOP_OK);
    close(fd);
    CHECK(strcmp(t1, t2) != 0);
    CHECK(fileops_create_tmp(root, "nodir", &fd, t2, sizeof t2) == FOP_ENOENT);
    CHECK(fileops_create_tmp(root, "evil", &fd, t2, sizeof t2) == FOP_EINVAL);
    CHECK(fileops_create_tmp(root, "docs", &fd, t2, 10) == FOP_EINVAL);

    CHECK(fileops_commit_tmp(root, "docs", t1, "a.txt") == FOP_EEXIST);   /* не затираем */
    CHECK(fileops_commit_tmp(root, "docs", t1, "b.txt") == FOP_OK);
    CHECK(access(P("docs/b.txt"), F_OK) == 0);
    {
        char path[2 * PATH_MAX];
        snprintf(path, sizeof path, "%s/docs/%s", root, t1);
        CHECK(access(path, F_OK) != 0);                      /* tmp убран */
    }
    CHECK(fileops_commit_tmp(root, "docs", "a.txt", "c.txt") == FOP_EINVAL); /* не tmp */
    CHECK(fileops_commit_tmp(root, "docs", t2, ".part-9") == FOP_EINVAL);
    CHECK(fileops_commit_tmp(root, "docs", "../.part-1", "c.txt") == FOP_EINVAL);

    CHECK(fileops_discard_tmp(root, "docs", t2) == FOP_OK);
    CHECK(fileops_discard_tmp(root, "docs", t2) == FOP_ENOENT);
    CHECK(fileops_discard_tmp(root, "docs", "a.txt") == FOP_EINVAL);   /* чужое не трогаем */
    CHECK(access(P("docs/a.txt"), F_OK) == 0);

    /* обычный API не работает с tmp-именами */
    CHECK(fileops_mkdir(root, ".part-1") == FOP_EINVAL);
    CHECK(fileops_remove(root, "docs/.part-1") == FOP_EINVAL);

    snprintf(cmd, sizeof cmd, "rm -rf %s", base);
    if (system(cmd) != 0) fprintf(stderr, "cleanup failed\n");
    DONE();
}
