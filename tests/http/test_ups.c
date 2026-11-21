#include "http/ups.h"
#include "test.h"
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[PATH_MAX], root[PATH_MAX + 8];
#define MAXSZ 100ULL

static char *P(const char *rel)
{
    static char b[4][2 * PATH_MAX];
    static int i;
    char *p = b[i++ & 3];
    snprintf(p, sizeof b[0], "%s/%s", root, rel);
    return p;
}
static int count_entries(const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    int n = 0;
    if (!d) return -1;
    while ((e = readdir(d)))
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
            n++;
    closedir(d);
    return n;
}
static size_t read_file(const char *path, char *buf, size_t n)
{
    FILE *f = fopen(path, "r");
    size_t r;
    if (!f) return (size_t)-1;
    r = fread(buf, 1, n, f);
    fclose(f);
    return r;
}
static void must(int ok, const char *what) { if (!ok) { perror(what); exit(2); } }
static int begin(const char *q, const char *cl, int hx, ups **u)
{
    return ups_begin(root, q, cl, hx, MAXSZ, 0, u);
}

int main(void)
{
    char tmpl[] = "/tmp/upstestXXXXXX", cmd[2 * PATH_MAX], buf[32];
    ups *u = NULL;
    struct stat st;
    FILE *f;

    must(mkdtemp(tmpl) != NULL, "mkdtemp");
    must(realpath(tmpl, base) != NULL, "realpath");
    snprintf(root, sizeof root, "%s/root", base);
    must(mkdir(root, 0700) == 0, "mkdir");
    must(mkdir(P("docs"), 0700) == 0, "mkdir docs");
    must(symlink(base, P("evil")) == 0, "symlink");
    f = fopen(P("docs/old.txt"), "w"); must(f != NULL, "fopen"); fclose(f);

    /* полный цикл */
    CHECK(begin("p=docs%2Fa.txt", "10", 1, &u) == 0 && u);
    CHECK(count_entries(P("docs")) == 2);                     /* old.txt + tmp */
    CHECK(ups_chunk(u, "hello", 5) == 0);
    CHECK(ups_chunk(u, "world", 5) == 0);
    CHECK(ups_end(u) == 0);
    CHECK(read_file(P("docs/a.txt"), buf, sizeof buf) == 10 && !memcmp(buf, "helloworld", 10));
    CHECK(count_entries(P("docs")) == 2);                     /* tmp исчез */

    /* пустой файл */
    CHECK(begin("p=empty.bin", "0", 1, &u) == 0);
    CHECK(ups_end(u) == 0);
    CHECK(stat(P("empty.bin"), &st) == 0 && st.st_size == 0);

    /* обрыв соединения: end без полного тела */
    CHECK(begin("p=drop.bin", "10", 1, &u) == 0);
    CHECK(ups_chunk(u, "12345", 5) == 0);
    CHECK(ups_end(u) != 0);
    CHECK(access(P("drop.bin"), F_OK) != 0);
    CHECK(count_entries(root) == 3);                          /* docs, evil, empty.bin */

    /* лишние данные: провал, затем end убирает всё */
    CHECK(begin("p=over.bin", "3", 1, &u) == 0);
    CHECK(ups_chunk(u, "abcd", 4) != 0);
    CHECK(ups_chunk(u, "a", 1) != 0);                         /* после ошибки всё отклоняется */
    CHECK(ups_end(u) != 0);
    CHECK(access(P("over.bin"), F_OK) != 0 && count_entries(root) == 3);

    /* отказы на старте */
    CHECK(begin("p=x.bin", "1", 0, &u) == 403 && u == NULL);
    CHECK(begin("", "1", 1, &u) == 400);
    CHECK(begin("p=", "1", 1, &u) == 400);
    CHECK(begin("p=x.bin", NULL, 1, &u) == 400);
    CHECK(begin("p=x.bin", "", 1, &u) == 400);
    CHECK(begin("p=x.bin", "abc", 1, &u) == 400);
    CHECK(begin("p=x.bin", "-1", 1, &u) == 400);
    CHECK(begin("p=x.bin", "1x", 1, &u) == 400);
    CHECK(begin("p=x.bin", "99999999999999999999999", 1, &u) == 400);
    CHECK(begin("p=x.bin", "101", 1, &u) == 413);             /* больше MAXSZ */
    CHECK(begin("p=x.bin", "100", 1, &u) == 0);
    ups_end(u);                                                /* 0 из 100 байт: откат */
    CHECK(begin("p=docs%2Fold.txt", "1", 1, &u) == 409);
    CHECK(begin("p=nodir%2Fx", "1", 1, &u) == 404);
    CHECK(begin("p=..%2Fx", "1", 1, &u) == 400);
    CHECK(begin("p=evil%2Fx", "1", 1, &u) == 400);
    CHECK(begin("p=%zz", "1", 1, &u) == 400);
    CHECK(begin("p=.part-1", "1", 1, &u) == 400);
    CHECK(ups_begin(root, "p=x.bin", "1", 1, MAXSZ, ~0ULL, &u) == 507);
    CHECK(ups_begin(root, "p=x.bin", "1", 1, MAXSZ, 0, NULL) == 500);
    CHECK(access(P("x.bin"), F_OK) != 0);
    CHECK(count_entries(root) == 3);                          /* ничего лишнего не осталось */

    /* precheck */
    CHECK(ups_precheck(root, "docs/p.bin", "10", 1, MAXSZ, 0) == 0);
    CHECK(ups_precheck(root, "docs/p.bin", "10", 0, MAXSZ, 0) == 403);
    CHECK(ups_precheck(root, "docs/old.txt", "10", 1, MAXSZ, 0) == 409);
    CHECK(ups_precheck(root, "docs/p.bin", "1000", 1, MAXSZ, 0) == 413);
    CHECK(ups_precheck(root, "", "10", 1, MAXSZ, 0) == 400);
    CHECK(ups_precheck(NULL, "x", "10", 1, MAXSZ, 0) != 0);
    CHECK(count_entries(P("docs")) == 2);

    /* безопасные вызовы на NULL */
    CHECK(ups_end(NULL) != 0);
    CHECK(ups_chunk(NULL, "a", 1) != 0);

    snprintf(cmd, sizeof cmd, "rm -rf %s", base);
    if (system(cmd) != 0) fprintf(stderr, "cleanup failed\n");
    DONE();
}
