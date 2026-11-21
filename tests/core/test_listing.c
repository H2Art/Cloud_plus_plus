#include "core/listing.h"
#include "test.h"
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void epoch(const char *p)
{
    struct timespec ts[2] = { { 0, 0 }, { 0, 0 } };
    if (utimensat(AT_FDCWD, p, ts, 0) != 0) { perror("utimensat"); exit(2); }
}

static void wfile(const char *dir, const char *name, const char *data)
{
    char p[2 * PATH_MAX];
    FILE *f;
    snprintf(p, sizeof p, "%s/%s", dir, name);
    f = fopen(p, "w");
    if (!f) { perror("fopen"); exit(2); }
    fputs(data, f);
    fclose(f);
    epoch(p);
}

static void wdir(const char *dir, const char *name)
{
    char p[2 * PATH_MAX];
    snprintf(p, sizeof p, "%s/%s", dir, name);
    if (mkdir(p, 0700) != 0) { perror("mkdir"); exit(2); }
    epoch(p);
}

int main(void)
{
    char tmpl[] = "/tmp/listtestXXXXXX", base[PATH_MAX], root[PATH_MAX + 8];
    char p[2 * PATH_MAX], cmd[2 * PATH_MAX], sz[32];
    char *out = malloc(65536);
    const char *a, *b2;
    int r;

    if (!mkdtemp(tmpl)) { perror("mkdtemp"); return 2; }
    if (!realpath(tmpl, base)) { perror("realpath"); return 2; }
    snprintf(root, sizeof root, "%s/root", base);
    if (mkdir(root, 0700) != 0) { perror("mkdir"); return 2; }

    wdir(root, "docs");
    wdir(root, "empty");
    wfile(root, "b.txt", "hello");
    wfile(root, "A file.txt", "");
    wfile(root, "<x>.txt", "x");
    wfile(root, "q\"uote.txt", "x");
    wfile(root, "\xd0\xba\xd0\xb8\xd1\x80.txt", "x");
    snprintf(p, sizeof p, "%s/docs", root);
    wfile(p, "inner.txt", "1234567890");
    wdir(p, "sub");
    snprintf(p, sizeof p, "%s/evil", root);
    if (symlink(base, p) != 0) { perror("symlink"); return 2; }

    /* корень */
    r = listing_render(root, "", out, 65536);
    CHECK(r == LISTING_OK);
    CHECK(strstr(out, "<table") && strstr(out, "</table>"));
    CHECK(strstr(out, ">docs/</a>") != NULL);
    CHECK(strstr(out, "href=\"/?p=docs\"") != NULL);
    CHECK(strstr(out, "hx-get=\"/list?p=docs\"") != NULL);
    CHECK(strstr(out, "href=\"/dl/b.txt\"") != NULL);
    CHECK(strstr(out, ">5 B<") != NULL);
    CHECK(strstr(out, ">0 B<") != NULL);
    CHECK(strstr(out, "1970-01-01 00:00") != NULL);
    CHECK(strstr(out, "hx-delete=\"/rm?p=b.txt\"") != NULL);
    CHECK(strstr(out, "class=\"del\"") != NULL && strstr(out, "title=\"Удалить\"") != NULL);
    CHECK(strstr(out, "hx-confirm=\"Удалить?\">\xE2\x9C\x95</button>") != NULL);
    CHECK(strstr(out, ">..</a>") == NULL);          /* в корне нет "вверх" */
    CHECK(strstr(out, "evil") == NULL);             /* симлинк скрыт */

    /* сортировка: папки сверху, затем побайтово */
    a = strstr(out, ">docs/</a>");
    b2 = strstr(out, ">b.txt</a>");
    CHECK(a && b2 && a < b2);
    a = strstr(out, ">empty/</a>");
    CHECK(a && b2 && a < b2);
    a = strstr(out, ">A file.txt</a>");
    CHECK(a && b2 && a < b2);                       /* 'A' < 'b' */

    /* экранирование и кодирование */
    CHECK(strstr(out, "&lt;x&gt;.txt") != NULL);
    CHECK(strstr(out, "<x>.txt") == NULL);
    CHECK(strstr(out, "q&quot;uote.txt") != NULL);
    CHECK(strstr(out, "q%22uote.txt") != NULL);
    CHECK(strstr(out, "A%20file.txt") != NULL);
    CHECK(strstr(out, "%D0%BA%D0%B8%D1%80.txt") != NULL);

    /* подкаталог */
    r = listing_render(root, "docs", out, 65536);
    CHECK(r == LISTING_OK);
    CHECK(strstr(out, "href=\"/?p=\"") != NULL);    /* родитель = корень */
    CHECK(strstr(out, ">..</a>") != NULL);
    CHECK(strstr(out, "href=\"/dl/docs/inner.txt\"") != NULL);
    CHECK(strstr(out, ">10 B<") != NULL);
    CHECK(strstr(out, "href=\"/?p=docs/sub\"") != NULL);

    /* вложенный: родитель = docs */
    r = listing_render(root, "docs/sub", out, 65536);
    CHECK(r == LISTING_OK);
    CHECK(strstr(out, "hx-get=\"/list?p=docs\"") != NULL);
    CHECK(strstr(out, "(empty)") != NULL);

    /* нормализация пути на входе */
    r = listing_render(root, "/docs//./", out, 65536);
    CHECK(r == LISTING_OK && strstr(out, "href=\"/dl/docs/inner.txt\""));

    /* ошибки */
    CHECK(listing_render(root, "..", out, 65536) == LISTING_EPATH);
    CHECK(listing_render(root, "docs/../..", out, 65536) == LISTING_EPATH);
    CHECK(listing_render(root, "evil", out, 65536) == LISTING_EPATH);
    CHECK(listing_render(root, "nope", out, 65536) == LISTING_ENOENT);
    CHECK(listing_render(root, "b.txt", out, 65536) == LISTING_ENOTDIR);
    CHECK(listing_render(root, "", out, 50) == LISTING_ETRUNC && out[0] == '\0');
    CHECK(listing_render(root, "", NULL, 10) == LISTING_ETRUNC);

    /* format_size */
    format_size(0, sz, sizeof sz);          CHECK(!strcmp(sz, "0 B"));
    format_size(1023, sz, sizeof sz);       CHECK(!strcmp(sz, "1023 B"));
    format_size(1024, sz, sizeof sz);       CHECK(!strcmp(sz, "1.0 KB"));
    format_size(1536, sz, sizeof sz);       CHECK(!strcmp(sz, "1.5 KB"));
    format_size(5ULL << 20, sz, sizeof sz); CHECK(!strcmp(sz, "5.0 MB"));
    format_size(2ULL << 30, sz, sizeof sz); CHECK(!strcmp(sz, "2.0 GB"));

    free(out);
    snprintf(cmd, sizeof cmd, "rm -rf %s", base);
    if (system(cmd) != 0) fprintf(stderr, "cleanup failed\n");
    DONE();
}
