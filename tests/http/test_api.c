#include "http/api.h"
#include "test.h"
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
static int has(const api_resp *r, const char *s) { return r->body && strstr(r->body, s) != NULL; }
static int is_dir(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }

static int call(api_resp *r, const char *m, const char *uri, const char *q,
                const char *body, int hx)
{
    api_dispatch(root, m, uri, q, body, hx, r);
    return r->status;
}

int main(void)
{
    char tmpl[] = "/tmp/apitestXXXXXX", cmd[2 * PATH_MAX];
    api_resp r;
    FILE *f;

    must(mkdtemp(tmpl) != NULL, "mkdtemp");
    must(realpath(tmpl, base) != NULL, "realpath");
    snprintf(root, sizeof root, "%s/root", base);
    must(mkdir(root, 0700) == 0, "mkdir");
    must(mkdir(P("docs"), 0700) == 0, "mkdir docs");
    must(mkdir(P("docs/full"), 0700) == 0, "mkdir full");
    must(mkdir(P("my dir"), 0700) == 0, "mkdir my dir");
    must(mkdir(P("q\"<b>"), 0700) == 0, "mkdir q");
    f = fopen(P("docs/full/x.txt"), "w"); must(f != NULL, "fopen"); fclose(f);
    f = fopen(P("a.txt"), "w");           must(f != NULL, "fopen"); fclose(f);
    must(symlink(base, P("evil")) == 0, "symlink");

    /* страница и фрагмент */
    CHECK(call(&r, "GET", "/", "", "", 0) == 200);
    CHECK(has(&r, "<!doctype html>") && has(&r, "/static/htmx.min.js"));
    CHECK(has(&r, "<table") && has(&r, "hx-post=\"/mkdir\"") && has(&r, "name=\"dir\" value=\"\""));
    CHECK(r.ctype && strstr(r.ctype, "text/html"));
    CHECK(r.len == strlen(r.body));
    api_resp_free(&r);

    CHECK(call(&r, "GET", "/list", "p=docs", "", 0) == 200);
    CHECK(!has(&r, "<!doctype html>") && has(&r, "docs/full/x.txt") == 0);
    CHECK(has(&r, ">full/</a>") && has(&r, "name=\"dir\" value=\"docs\""));
    api_resp_free(&r);

    CHECK(call(&r, "GET", "/list", "p=my%20dir", "", 0) == 200);
    CHECK(has(&r, "value=\"my dir\""));
    api_resp_free(&r);

    CHECK(call(&r, "GET", "/list", "p=/docs//full/", "", 0) == 200);
    CHECK(has(&r, "href=\"/dl/docs/full/x.txt\"") && has(&r, "value=\"docs/full\""));
    api_resp_free(&r);

    /* экранирование в форме */
    CHECK(call(&r, "GET", "/list", "p=q%22%3Cb%3E", "", 0) == 200);
    CHECK(has(&r, "value=\"q&quot;&lt;b&gt;\"") && !has(&r, "q\"<b>"));
    api_resp_free(&r);

    /* ошибки чтения */
    CHECK(call(&r, "GET", "/list", "p=..", "", 0) == 400);           api_resp_free(&r);
    CHECK(call(&r, "GET", "/list", "p=%2e%2e%2fx", "", 0) == 400);   api_resp_free(&r);
    CHECK(call(&r, "GET", "/list", "p=evil", "", 0) == 400);         api_resp_free(&r);
    CHECK(call(&r, "GET", "/list", "p=nope", "", 0) == 404);         api_resp_free(&r);
    CHECK(call(&r, "GET", "/list", "p=a.txt", "", 0) == 400);        api_resp_free(&r);
    CHECK(call(&r, "GET", "/list", "p=%zz", "", 0) == 400);          api_resp_free(&r);
    CHECK(call(&r, "GET", "/list", "p=a%00b", "", 0) == 400);        api_resp_free(&r);

    /* методы и маршруты */
    CHECK(call(&r, "POST", "/list", "", "", 1) == 405);   api_resp_free(&r);
    CHECK(call(&r, "GET", "/mkdir", "", "", 1) == 405);   api_resp_free(&r);
    CHECK(call(&r, "GET", "/rm", "p=a.txt", "", 1) == 405); api_resp_free(&r);
    CHECK(call(&r, "GET", "/nope", "", "", 1) == 404);    api_resp_free(&r);
    CHECK(call(&r, "GET", "/dl/a.txt", "", "", 1) == 404); api_resp_free(&r);   /* это nginx */
    api_dispatch(root, NULL, "/", "", "", 0, &r);
    CHECK(r.status == 400);                                api_resp_free(&r);

    /* mkdir */
    CHECK(call(&r, "POST", "/mkdir", "", "dir=&name=new+folder", 0) == 403);   /* без HX */
    api_resp_free(&r);
    CHECK(!is_dir(P("new folder")));
    CHECK(call(&r, "POST", "/mkdir", "", "dir=&name=new+folder", 1) == 200);
    CHECK(is_dir(P("new folder")) && has(&r, ">new folder/</a>"));
    CHECK(!has(&r, "<!doctype html>"));
    api_resp_free(&r);
    CHECK(call(&r, "POST", "/mkdir", "", "dir=docs&name=sub", 1) == 200);
    CHECK(is_dir(P("docs/sub")) && has(&r, "value=\"docs\""));
    api_resp_free(&r);
    CHECK(call(&r, "POST", "/mkdir", "dir=docs&name=viaquery", "", 1) == 200);
    CHECK(is_dir(P("docs/viaquery")));                          api_resp_free(&r);
    CHECK(call(&r, "POST", "/mkdir", "", "dir=&name=new+folder", 1) == 409);
    api_resp_free(&r);
    CHECK(call(&r, "POST", "/mkdir", "", "dir=nodir&name=x", 1) == 404);
    api_resp_free(&r);
    CHECK(call(&r, "POST", "/mkdir", "", "dir=&name=a%2Fb", 1) == 400);   /* '/' в имени */
    api_resp_free(&r);
    CHECK(call(&r, "POST", "/mkdir", "", "dir=&name=..", 1) == 400);      api_resp_free(&r);
    CHECK(call(&r, "POST", "/mkdir", "", "dir=&name=", 1) == 400);        api_resp_free(&r);
    CHECK(call(&r, "POST", "/mkdir", "", "dir=", 1) == 400);              api_resp_free(&r);
    CHECK(call(&r, "POST", "/mkdir", "", "dir=..&name=x", 1) == 400);     api_resp_free(&r);
    CHECK(call(&r, "POST", "/mkdir", "", "dir=evil&name=x", 1) == 400);   api_resp_free(&r);
    CHECK(!is_dir(P("../x")));
    CHECK(call(&r, "POST", "/mkdir", "", "dir=&name=.part-1", 1) == 400); api_resp_free(&r);

    /* удаление */
    CHECK(call(&r, "DELETE", "/rm", "p=a.txt", "", 0) == 403);   api_resp_free(&r);
    CHECK(access(P("a.txt"), F_OK) == 0);
    CHECK(call(&r, "DELETE", "/rm", "p=a.txt", "", 1) == 200);
    CHECK(access(P("a.txt"), F_OK) != 0 && !has(&r, ">a.txt<"));
    api_resp_free(&r);
    CHECK(call(&r, "DELETE", "/rm", "p=docs/sub", "", 1) == 200);
    CHECK(!is_dir(P("docs/sub")) && has(&r, "value=\"docs\""));
    api_resp_free(&r);
    CHECK(call(&r, "DELETE", "/rm", "p=docs/full", "", 1) == 409);   /* не пустая */
    api_resp_free(&r);
    CHECK(call(&r, "DELETE", "/rm", "p=a.txt", "", 1) == 404);       api_resp_free(&r);
    CHECK(call(&r, "DELETE", "/rm", "", "", 1) == 400);              api_resp_free(&r);
    CHECK(call(&r, "DELETE", "/rm", "p=", "", 1) == 400);            api_resp_free(&r);
    CHECK(call(&r, "DELETE", "/rm", "p=..%2Fbase", "", 1) == 400);   api_resp_free(&r);
    CHECK(call(&r, "DELETE", "/rm", "p=evil", "", 1) == 200);        /* удаляется сам симлинк */
    CHECK(access(base, F_OK) == 0);
    api_resp_free(&r);

    snprintf(cmd, sizeof cmd, "rm -rf %s", base);
    if (system(cmd) != 0) fprintf(stderr, "cleanup failed\n");
    DONE();
}
