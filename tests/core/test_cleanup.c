#include "core/cleanup.h"
#include "test.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[PATH_MAX], root[PATH_MAX + 8];

static char *P(const char *rel, const char *where)
{
    static char b[8][2 * PATH_MAX];
    static int i;
    char *p = b[i++ & 7];
    snprintf(p, sizeof b[0], "%s/%s", where, rel);
    return p;
}
static void touch(const char *p)
{
    FILE *f = fopen(p, "w");
    if (!f) { perror("fopen"); exit(2); }
    fclose(f);
}
static void must(int ok, const char *what) { if (!ok) { perror(what); exit(2); } }
static int exists(const char *p) { struct stat st; return lstat(p, &st) == 0; }

int main(void)
{
    char tmpl[] = "/tmp/cleantestXXXXXX", cmd[2 * PATH_MAX];

    must(mkdtemp(tmpl) != NULL, "mkdtemp");
    must(realpath(tmpl, base) != NULL, "realpath");
    snprintf(root, sizeof root, "%s/root", base);
    must(mkdir(root, 0700) == 0, "mkdir");
    must(mkdir(P("docs", root), 0700) == 0, "docs");
    must(mkdir(P("docs/sub", root), 0700) == 0, "sub");
    must(mkdir(P(".part-dir", root), 0700) == 0, "part-dir");      /* каталог не трогаем */
    must(mkdir(P("outside", base), 0700) == 0, "outside");

    touch(P(".part-1-0", root));
    touch(P("docs/.part-2-1", root));
    touch(P("docs/sub/.part-3-2", root));
    touch(P("keep.txt", root));
    touch(P("docs/a.part-1", root));                               /* не префикс */
    touch(P("docs/.part", root));                                  /* не префикс ".part-" */
    touch(P(".part-dir/.part-9-9", root));                         /* внутри каталога: удаляется */
    touch(P(".part-out-1", base));                                 /* вне root: не трогать */
    touch(P("outside/.part-7-7", base));
    must(symlink(P("outside", base), P("evil", root)) == 0, "symlink dir");   /* не заходить */
    must(symlink("/nonexistent", P(".part-link", root)) == 0, "symlink tmp");

    CHECK(cleanup_tmp(root) == 5);   /* .part-1-0, .part-2-1, .part-3-2, .part-9-9, .part-link */

    CHECK(!exists(P(".part-1-0", root)));
    CHECK(!exists(P("docs/.part-2-1", root)));
    CHECK(!exists(P("docs/sub/.part-3-2", root)));
    CHECK(!exists(P(".part-dir/.part-9-9", root)));
    CHECK(!exists(P(".part-link", root)));
    CHECK(exists(P("keep.txt", root)));
    CHECK(exists(P("docs/a.part-1", root)));
    CHECK(exists(P("docs/.part", root)));
    CHECK(exists(P(".part-dir", root)));                           /* каталог остался */
    CHECK(exists(P("evil", root)));
    CHECK(exists(P("outside/.part-7-7", base)));                   /* через симлинк не ходили */
    CHECK(exists(P(".part-out-1", base)));

    CHECK(cleanup_tmp(root) == 0);                                 /* повтор ничего не находит */
    CHECK(cleanup_tmp("/nonexistent/zzz") == -1);
    CHECK(cleanup_tmp(NULL) == -1);

    snprintf(cmd, sizeof cmd, "rm -rf %s", base);
    if (system(cmd) != 0) fprintf(stderr, "cleanup failed\n");
    DONE();
}
