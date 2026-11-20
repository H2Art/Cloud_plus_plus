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

static char *P(const char *rel, const char *where)
{
    static char b[4][2 * PATH_MAX];
    static int i;
    char *p = b[i++ & 3];
    snprintf(p, sizeof b[0], "%s/%s", where, rel);
    return p;
}

static int exists(const char *p)      { struct stat st; return stat(p, &st) == 0; }
static int lexists(const char *p)     { struct stat st; return lstat(p, &st) == 0; }
static void must(int ok, const char *what) { if (!ok) { perror(what); exit(2); } }

int main(void)
{
    char tmpl[] = "/tmp/fopstestXXXXXX", cmd[2 * PATH_MAX], buf[8] = { 0 };
    char longname[300];
    int fd = -1;
    FILE *f;

    must(mkdtemp(tmpl) != NULL, "mkdtemp");
    must(realpath(tmpl, base) != NULL, "realpath");
    snprintf(root, sizeof root, "%s/root", base);
    must(mkdir(root, 0700) == 0, "mkdir root");
    must(mkdir(P("docs", root), 0700) == 0, "mkdir docs");
    must(mkdir(P("docs/full", root), 0700) == 0, "mkdir full");
    f = fopen(P("docs/full/x", root), "w"); must(f != NULL, "fopen"); fclose(f);
    f = fopen(P("outside", base), "w");     must(f != NULL, "fopen"); fclose(f);
    must(symlink(base, P("evil", root)) == 0, "symlink evil");
    must(symlink(P("docs", root), P("inner", root)) == 0, "symlink inner");
    must(symlink(P("outside_new", base), P("dang", root)) == 0, "symlink dang");

    /* valid_name */
    memset(longname, 'a', 255); longname[255] = '\0';
    CHECK(fileops_valid_name("a.txt"));
    CHECK(fileops_valid_name("..."));
    CHECK(fileops_valid_name(".hidden"));
    CHECK(fileops_valid_name("\xd0\xba\xd0\xb8\xd1\x80.txt"));
    CHECK(fileops_valid_name(longname));
    longname[255] = 'a'; longname[256] = '\0';
    CHECK(!fileops_valid_name(longname));
    CHECK(!fileops_valid_name(""));
    CHECK(!fileops_valid_name("."));
    CHECK(!fileops_valid_name(".."));
    CHECK(!fileops_valid_name("a/b"));
    CHECK(!fileops_valid_name("a\nb"));
    CHECK(!fileops_valid_name("a\tb"));
    CHECK(!fileops_valid_name("a\x7f"));
    CHECK(!fileops_valid_name(NULL));

    /* mkdir */
    CHECK(fileops_mkdir(root, "newdir") == FOP_OK);
    CHECK(exists(P("newdir", root)));
    CHECK(fileops_mkdir(root, "newdir") == FOP_EEXIST);
    CHECK(fileops_mkdir(root, "docs/sub") == FOP_OK);
    CHECK(fileops_mkdir(root, "/docs//sub2/") == FOP_OK);
    CHECK(fileops_mkdir(root, "nodir/x") == FOP_ENOENT);
    CHECK(fileops_mkdir(root, "../x") == FOP_EINVAL);
    CHECK(fileops_mkdir(root, "docs/../x") == FOP_EINVAL);
    CHECK(fileops_mkdir(root, "evil/x") == FOP_EINVAL);
    CHECK(!exists(P("x", base)));
    CHECK(fileops_mkdir(root, "") == FOP_EINVAL);
    CHECK(fileops_mkdir(root, "/") == FOP_EINVAL);
    CHECK(fileops_mkdir(root, "docs/.") == FOP_EINVAL);
    CHECK(fileops_mkdir(root, "bad\nname") == FOP_EINVAL);
    CHECK(fileops_mkdir(NULL, "x") == FOP_EINVAL);

    /* create */
    CHECK(fileops_create(root, "f.txt", &fd) == FOP_OK && fd >= 0);
    CHECK(write(fd, "hi", 2) == 2);
    close(fd);
    f = fopen(P("f.txt", root), "r");
    CHECK(f != NULL);
    if (f) { CHECK(fread(buf, 1, 7, f) == 2 && !strcmp(buf, "hi")); fclose(f); }
    {
        struct stat st;
        CHECK(stat(P("f.txt", root), &st) == 0 && (st.st_mode & 0777) == 0640);
    }
    CHECK(fileops_create(root, "f.txt", &fd) == FOP_EEXIST);
    CHECK(fileops_create(root, "docs/g.txt", &fd) == FOP_OK);
    close(fd);
    CHECK(fileops_create(root, "nodir/g.txt", &fd) == FOP_ENOENT);
    CHECK(fileops_create(root, "../g.txt", &fd) == FOP_EINVAL);
    CHECK(fileops_create(root, "evil/g.txt", &fd) == FOP_EINVAL);
    CHECK(!exists(P("g.txt", base)));
    CHECK(fileops_create(root, "dang", &fd) == FOP_EEXIST);   /* не идём по симлинку */
    CHECK(!exists(P("outside_new", base)));
    CHECK(fileops_create(root, "f.txt/inner", &fd) == FOP_ENOENT); /* родитель - файл */
    CHECK(fileops_create(root, "", &fd) == FOP_EINVAL);
    CHECK(fileops_create(root, "ok.txt", NULL) == FOP_EINVAL);

    /* remove */
    CHECK(fileops_remove(root, "f.txt") == FOP_OK && !exists(P("f.txt", root)));
    CHECK(fileops_remove(root, "f.txt") == FOP_ENOENT);
    CHECK(fileops_remove(root, "newdir") == FOP_OK && !exists(P("newdir", root)));
    CHECK(fileops_remove(root, "docs/full") == FOP_ENOTEMPTY);
    CHECK(exists(P("docs/full/x", root)));
    CHECK(fileops_remove(root, "") == FOP_EINVAL);
    CHECK(fileops_remove(root, "/") == FOP_EINVAL);
    CHECK(fileops_remove(root, "../outside") == FOP_EINVAL);
    CHECK(fileops_remove(root, "evil/outside") == FOP_EINVAL);
    CHECK(exists(P("outside", base)));
    CHECK(fileops_remove(root, "docs/..") == FOP_EINVAL);

    /* симлинк удаляется сам, цель остаётся */
    CHECK(fileops_remove(root, "inner") == FOP_OK);
    CHECK(!lexists(P("inner", root)));
    CHECK(exists(P("docs/full/x", root)));
    CHECK(fileops_remove(root, "evil") == FOP_OK);
    CHECK(!lexists(P("evil", root)));
    CHECK(exists(P("outside", base)));

    snprintf(cmd, sizeof cmd, "rm -rf %s", base);
    if (system(cmd) != 0) fprintf(stderr, "cleanup failed\n");
    DONE();
}
