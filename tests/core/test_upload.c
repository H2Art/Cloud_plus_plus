#include "core/fileops.h"
#include "core/listing.h"
#include "core/upload.h"
#include "test.h"
#include <dirent.h>
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

int main(void)
{
    char tmpl[] = "/tmp/uptestXXXXXX", cmd[2 * PATH_MAX], buf[64];
    char *html = malloc(65536);
    upload *u = NULL;
    struct stat st;
    FILE *f;

    must(mkdtemp(tmpl) != NULL, "mkdtemp");
    must(realpath(tmpl, base) != NULL, "realpath");
    snprintf(root, sizeof root, "%s/root", base);
    must(mkdir(root, 0700) == 0, "mkdir root");
    must(mkdir(P("docs"), 0700) == 0, "mkdir docs");
    must(symlink(base, P("evil")) == 0, "symlink");

    /* обычная загрузка частями */
    CHECK(upload_begin(root, "docs/a.txt", 10, 0, &u) == UPLOAD_OK && u);
    CHECK(count_entries(P("docs")) == 1);                 /* временный файл */
    CHECK(access(P("docs/a.txt"), F_OK) != 0);            /* итогового ещё нет */
    CHECK(listing_render(root, "docs", html, 65536) == LISTING_OK);
    CHECK(strstr(html, ".part-") == NULL);                /* скрыт в списке */
    CHECK(strstr(html, "(empty)") != NULL);
    CHECK(upload_write(u, "hello", 5) == UPLOAD_OK);
    CHECK(upload_write(u, "world", 5) == UPLOAD_OK);
    CHECK(upload_commit(u) == UPLOAD_OK);
    CHECK(count_entries(P("docs")) == 1);                 /* tmp исчез, файл на месте */
    CHECK(read_file(P("docs/a.txt"), buf, sizeof buf) == 10 && !memcmp(buf, "helloworld", 10));
    CHECK(stat(P("docs/a.txt"), &st) == 0 && (st.st_mode & 0777) == 0640);

    /* пустой файл */
    CHECK(upload_begin(root, "empty.txt", 0, 0, &u) == UPLOAD_OK);
    CHECK(upload_commit(u) == UPLOAD_OK);
    CHECK(stat(P("empty.txt"), &st) == 0 && st.st_size == 0);

    /* уже существует: отказ до приёма данных, ничего не создано */
    CHECK(upload_begin(root, "docs/a.txt", 1, 0, &u) == UPLOAD_EEXIST && u == NULL);
    CHECK(count_entries(P("docs")) == 1);

    /* получено меньше заявленного */
    CHECK(upload_begin(root, "short.txt", 10, 0, &u) == UPLOAD_OK);
    CHECK(upload_write(u, "12345", 5) == UPLOAD_OK);
    CHECK(upload_commit(u) == UPLOAD_ESHORT);
    CHECK(access(P("short.txt"), F_OK) != 0);
    CHECK(count_entries(root) == 3);                      /* docs, evil, empty.txt */

    /* получено больше заявленного */
    CHECK(upload_begin(root, "big.txt", 3, 0, &u) == UPLOAD_OK);
    CHECK(upload_write(u, "ab", 2) == UPLOAD_OK);
    CHECK(upload_write(u, "cd", 2) == UPLOAD_EOVERFLOW);
    upload_abort(u);
    CHECK(count_entries(root) == 3);
    upload_abort(NULL);                                   /* безопасно */

    /* abort в середине */
    CHECK(upload_begin(root, "ab.txt", 5, 0, &u) == UPLOAD_OK);
    CHECK(upload_write(u, "abc", 3) == UPLOAD_OK);
    upload_abort(u);
    CHECK(access(P("ab.txt"), F_OK) != 0 && count_entries(root) == 3);

    /* гонка: файл появился между begin и commit, чужие данные не затираем */
    CHECK(upload_begin(root, "race.txt", 3, 0, &u) == UPLOAD_OK);
    CHECK(upload_write(u, "new", 3) == UPLOAD_OK);
    f = fopen(P("race.txt"), "w"); must(f != NULL, "fopen"); fputs("old", f); fclose(f);
    CHECK(upload_commit(u) == UPLOAD_EEXIST);
    CHECK(read_file(P("race.txt"), buf, sizeof buf) == 3 && !memcmp(buf, "old", 3));
    CHECK(count_entries(root) == 4);                      /* + race.txt, tmp удалён */

    /* плохие пути и имена */
    CHECK(upload_begin(root, "nodir/x.txt", 1, 0, &u) == UPLOAD_ENOENT);
    CHECK(upload_begin(root, "../x.txt", 1, 0, &u) == UPLOAD_EINVAL);
    CHECK(upload_begin(root, "evil/x.txt", 1, 0, &u) == UPLOAD_EINVAL);
    CHECK(access(P("../x.txt"), F_OK) != 0);
    CHECK(upload_begin(root, ".part-1-1", 1, 0, &u) == UPLOAD_EINVAL);
    CHECK(upload_begin(root, "", 1, 0, &u) == UPLOAD_EINVAL);
    CHECK(upload_begin(root, "bad\nname", 1, 0, &u) == UPLOAD_EINVAL);
    CHECK(upload_begin(NULL, "x", 1, 0, &u) == UPLOAD_EINVAL);
    CHECK(upload_begin(root, "x", 1, 0, NULL) == UPLOAD_EINVAL);

    /* нехватка места: резерв больше доступного */
    CHECK(upload_begin(root, "full.txt", 1, ~0ULL, &u) == UPLOAD_EFULL && u == NULL);
    CHECK(count_entries(root) == 4);

    /* precheck: ничего не создаёт */
    {
        int before = count_entries(root);
        CHECK(upload_precheck(root, "docs/pre.txt", 5, 0) == UPLOAD_OK);
        CHECK(upload_precheck(root, "docs/a.txt", 5, 0) == UPLOAD_EEXIST);
        CHECK(upload_precheck(root, "nodir/x", 5, 0) == UPLOAD_ENOENT);
        CHECK(upload_precheck(root, "../x", 5, 0) == UPLOAD_EINVAL);
        CHECK(upload_precheck(root, "x", 5, ~0ULL) == UPLOAD_EFULL);
        CHECK(upload_precheck(NULL, "x", 5, 0) == UPLOAD_EINVAL);
        CHECK(count_entries(root) == before && count_entries(P("docs")) == 1);
    }

    free(html);
    snprintf(cmd, sizeof cmd, "rm -rf %s", base);
    if (system(cmd) != 0) fprintf(stderr, "cleanup failed\n");
    DONE();
}
