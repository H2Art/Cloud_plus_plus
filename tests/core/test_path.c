#include "core/path.h"
#include "test.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char base[PATH_MAX], root[PATH_MAX], other[PATH_MAX];

static void sym(const char *target, const char *link)
{
    if (symlink(target, link) != 0) { perror("symlink"); exit(2); }
}

static void touch(const char *p)
{
    FILE *f = fopen(p, "w");
    if (f) fclose(f);
}

int main(void)
{
    char tmpl[] = "/tmp/pathtestXXXXXX", p[PATH_MAX], out[PATH_MAX], cmd[PATH_MAX + 16];

    if (!mkdtemp(tmpl)) { perror("mkdtemp"); return 2; }
    if (!realpath(tmpl, base)) { perror("realpath"); return 2; }
    snprintf(root, sizeof root, "%s/root", base);
    snprintf(other, sizeof other, "%s/root2", base); /* общий префикс с root */
    mkdir(root, 0700);
    mkdir(other, 0700);

    snprintf(p, sizeof p, "%s/docs", root);       mkdir(p, 0700);
    snprintf(p, sizeof p, "%s/docs/a.txt", root); touch(p);
    snprintf(p, sizeof p, "%s/secret", other);    touch(p);
    snprintf(p, sizeof p, "%s/out_secret", base); touch(p);

    /* симлинки: наружу, на соседа с общим префиксом, внутрь, битый */
    snprintf(p, sizeof p, "%s/evil", root);     sym(base, p);
    snprintf(p, sizeof p, "%s/evil2", root);    sym(other, p);
    snprintf(p, sizeof p, "%s/inner", root);
    { char t[PATH_MAX]; snprintf(t, sizeof t, "%s/docs", root); sym(t, p); }
    snprintf(p, sizeof p, "%s/dangling", root); sym("/nonexistent/zzz", p);

    /* нормальные случаи */
    CHECK(path_resolve(root, "docs/a.txt", out, sizeof out) == PATH_OK);
    snprintf(p, sizeof p, "%s/docs/a.txt", root);
    CHECK(strcmp(out, p) == 0);
    CHECK(path_resolve(root, "/docs/a.txt", out, sizeof out) == PATH_OK);
    CHECK(path_resolve(root, "docs//./a.txt", out, sizeof out) == PATH_OK);
    CHECK(path_resolve(root, "", out, sizeof out) == PATH_OK && !strcmp(out, root));
    CHECK(path_resolve(root, "/", out, sizeof out) == PATH_OK && !strcmp(out, root));
    CHECK(path_resolve(root, "inner/a.txt", out, sizeof out) == PATH_OK); /* симлинк внутрь */

    /* новые файлы */
    CHECK(path_resolve(root, "docs/new.txt", out, sizeof out) == PATH_OK);
    snprintf(p, sizeof p, "%s/docs/new.txt", root);
    CHECK(strcmp(out, p) == 0);
    CHECK(path_resolve(root, "nodir/new.txt", out, sizeof out) == PATH_ENOENT);
    CHECK(path_resolve(root, "docs/a.txt/x", out, sizeof out) == PATH_ENOENT);

    /* обход */
    CHECK(path_resolve(root, "..", out, sizeof out) == PATH_EINVAL);
    CHECK(path_resolve(root, "../out_secret", out, sizeof out) == PATH_EINVAL);
    CHECK(path_resolve(root, "docs/../../out_secret", out, sizeof out) == PATH_EINVAL);
    CHECK(path_resolve(root, "docs/..", out, sizeof out) == PATH_EINVAL);
    CHECK(path_resolve(root, "../root2/secret", out, sizeof out) == PATH_EINVAL);
    CHECK(path_resolve(root, "docs/.../x", out, sizeof out) != PATH_EINVAL); /* "..." обычное имя */

    /* симлинки наружу */
    CHECK(path_resolve(root, "evil", out, sizeof out) == PATH_EINVAL);
    CHECK(path_resolve(root, "evil/out_secret", out, sizeof out) == PATH_EINVAL);
    CHECK(path_resolve(root, "evil/new", out, sizeof out) == PATH_EINVAL);
    CHECK(path_resolve(root, "evil2/secret", out, sizeof out) == PATH_EINVAL); /* префикс root2 */
    CHECK(path_resolve(root, "dangling", out, sizeof out) == PATH_EINVAL);

    /* некорректные аргументы */
    CHECK(path_resolve(NULL, "a", out, sizeof out) == PATH_EINVAL);
    CHECK(path_resolve(root, NULL, out, sizeof out) == PATH_EINVAL);
    CHECK(path_resolve("relative", "a", out, sizeof out) == PATH_EINVAL);
    CHECK(path_resolve("/", "a", out, sizeof out) == PATH_EINVAL);
    CHECK(path_resolve(root, "docs/a.txt", out, 5) == PATH_ETOOLONG);

    /* слишком длинный путь */
    {
        char *big = malloc(PATH_MAX + 10);
        memset(big, 'a', PATH_MAX + 5);
        big[PATH_MAX + 5] = '\0';
        CHECK(path_resolve(root, big, out, sizeof out) == PATH_ETOOLONG);
        free(big);
    }

    snprintf(cmd, sizeof cmd, "rm -rf %s", base);
    if (system(cmd) != 0) fprintf(stderr, "cleanup failed\n");
    DONE();
}