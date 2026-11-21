#include "api.h"
#include "core/fileops.h"
#include "core/html.h"
#include "core/listing.h"
#include "core/path.h"
#include "core/url.h"
#include "http/limits.h"
#include "http/ups.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIST_MAX (1u << 20)   /* максимум HTML одного каталога */

static const char *PAGE_HEAD =
    "<!doctype html>\n<html lang=\"ru\"><head><meta charset=\"utf-8\">\n"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
    "<title>Cloud</title>\n<script src=\"/static/htmx.min.js\"></script>\n"
    "</head><body>\n<h1>Cloud</h1>\n<div id=\"list\">\n";
static const char *PAGE_TAIL = "</div>\n</body></html>\n";

void api_resp_free(api_resp *r)
{
    if (!r)
        return;
    free(r->body);
    r->body = NULL;
    r->len = 0;
}

static void set_body(api_resp *r, int status, const char *ctype, char *body)
{
    r->status = status;
    r->ctype = ctype;
    r->body = body;
    r->len = body ? strlen(body) : 0;
}

static void text(api_resp *r, int status, const char *msg)
{
    set_body(r, status, "text/plain; charset=utf-8", strdup(msg));
}

static int status_of_fop(int rc)
{
    switch (rc) {
    case FOP_EINVAL:    return 400;
    case FOP_EEXIST:    return 409;
    case FOP_ENOENT:    return 404;
    case FOP_ENOTEMPTY: return 409;
    case FOP_EFULL:     return 507;
    default:            return 500;
    }
}

/* 1 найден, 0 нет, -1 ошибка декодирования */
static int param_in(const char *s, const char *key, int form, char *out, size_t outsz)
{
    size_t kl = strlen(key);
    if (!s)
        return 0;
    while (*s) {
        const char *amp = strchr(s, '&');
        size_t plen = amp ? (size_t)(amp - s) : strlen(s);
        const char *eq = memchr(s, '=', plen);
        size_t nl = eq ? (size_t)(eq - s) : plen;
        if (nl == kl && memcmp(s, key, kl) == 0) {
            char tmp[PATH_MAX * 3];
            const char *v = eq ? eq + 1 : "";
            size_t vl = eq ? plen - nl - 1 : 0;
            if (vl >= sizeof tmp)
                return -1;
            memcpy(tmp, v, vl);
            tmp[vl] = '\0';
            return (form ? url_decode_form(tmp, out, outsz)
                         : url_decode(tmp, out, outsz)) == 0 ? 1 : -1;
        }
        if (!amp)
            break;
        s = amp + 1;
    }
    return 0;
}

int api_get_param(const char *query, const char *body, const char *key,
                     char *out, size_t outsz)
{
    int r = param_in(query, key, 0, out, outsz);
    if (r != 0)
        return r;
    return param_in(body, key, 1, out, outsz);
}

static void respond_dir(api_resp *r, const char *root, const char *dir, int full_page)
{
    char res[PATH_MAX], esc[PATH_MAX * 6 + 8];
    const char *crel;
    char *buf, *out;
    size_t n, need;
    int rc;

    rc = path_resolve(root, dir, res, sizeof res);
    if (rc == PATH_ENOENT) { text(r, 404, "not found"); return; }
    if (rc != PATH_OK)     { text(r, 400, "bad path"); return; }
    crel = res + strlen(root);
    while (*crel == '/')
        crel++;
    if (html_escape(crel, esc, sizeof esc) != 0) { text(r, 400, "bad path"); return; }

    buf = malloc(LIST_MAX);
    if (!buf) { text(r, 500, "out of memory"); return; }
    rc = listing_render(root, dir, buf, LIST_MAX);
    if (rc != LISTING_OK) {
        free(buf);
        switch (rc) {
        case LISTING_EPATH:   text(r, 400, "bad path"); break;
        case LISTING_ENOENT:  text(r, 404, "not found"); break;
        case LISTING_ENOTDIR: text(r, 400, "not a directory"); break;
        case LISTING_ETRUNC:  text(r, 500, "directory too large"); break;
        default:              text(r, 500, "internal error");
        }
        return;
    }
    n = strlen(buf);
    rc = snprintf(buf + n, LIST_MAX - n,
        "<form hx-post=\"/mkdir\" hx-target=\"#list\">"
        "<input type=\"hidden\" name=\"dir\" value=\"%s\">"
        "<input name=\"name\" placeholder=\"New folder\" required>"
        "<button type=\"submit\">Create folder</button></form>\n", esc);
    if (rc < 0 || (size_t)rc >= LIST_MAX - n) {
        free(buf);
        text(r, 500, "directory too large");
        return;
    }
    if (!full_page) {
        set_body(r, 200, "text/html; charset=utf-8", buf);
        return;
    }
    need = strlen(PAGE_HEAD) + strlen(buf) + strlen(PAGE_TAIL) + 1;
    out = malloc(need);
    if (!out) { free(buf); text(r, 500, "out of memory"); return; }
    snprintf(out, need, "%s%s%s", PAGE_HEAD, buf, PAGE_TAIL);
    free(buf);
    set_body(r, 200, "text/html; charset=utf-8", out);
}

void api_dispatch(const char *root, const char *method, const char *uri,
                  const char *query, const char *body, int hx, api_resp *r)
{
    char p[PATH_MAX], dir[PATH_MAX], name[NAME_MAX + 1], rel[PATH_MAX * 2];
    int g;

    memset(r, 0, sizeof *r);
    if (!root || !method || !uri) { text(r, 400, "bad request"); return; }

    if (!strcmp(uri, "/") || !strcmp(uri, "/list")) {
        if (strcmp(method, "GET")) { text(r, 405, "method not allowed"); return; }
        g = api_get_param(query, body, "p", p, sizeof p);
        if (g < 0) { text(r, 400, "bad parameter"); return; }
        if (g == 0) p[0] = '\0';
        respond_dir(r, root, p, !strcmp(uri, "/"));
        return;
    }

    if (!strcmp(uri, "/mkdir")) {
        int rc, n;
        if (strcmp(method, "POST")) { text(r, 405, "method not allowed"); return; }
        if (!hx) { text(r, 403, "forbidden"); return; }
        g = api_get_param(query, body, "dir", dir, sizeof dir);
        if (g < 0) { text(r, 400, "bad parameter"); return; }
        if (g == 0) dir[0] = '\0';
        g = api_get_param(query, body, "name", name, sizeof name);
        if (g <= 0 || !fileops_valid_name(name)) { text(r, 400, "bad name"); return; }
        n = snprintf(rel, sizeof rel, "%s%s%s", dir, *dir ? "/" : "", name);
        if (n < 0 || (size_t)n >= PATH_MAX) { text(r, 400, "path too long"); return; }
        rc = fileops_mkdir(root, rel);
        if (rc != FOP_OK) { text(r, status_of_fop(rc), "mkdir failed"); return; }
        respond_dir(r, root, dir, 0);
        return;
    }

    if (!strcmp(uri, "/rm")) {
        int rc;
        if (strcmp(method, "DELETE")) { text(r, 405, "method not allowed"); return; }
        if (!hx) { text(r, 403, "forbidden"); return; }
        g = api_get_param(query, body, "p", p, sizeof p);
        if (g <= 0) { text(r, 400, "bad parameter"); return; }
        rc = fileops_split(p, dir, sizeof dir, name, sizeof name);
        if (rc != FOP_OK) { text(r, 400, "bad path"); return; }
        rc = fileops_remove(root, p);
        if (rc != FOP_OK) { text(r, status_of_fop(rc), "remove failed"); return; }
        respond_dir(r, root, dir, 0);
        return;
    }

    if (!strcmp(uri, "/upcheck")) {
        char sz[32];
        int st;
        if (strcmp(method, "POST")) { text(r, 405, "method not allowed"); return; }
        g = api_get_param(query, body, "p", p, sizeof p);
        if (g <= 0) { text(r, 400, "bad parameter"); return; }
        g = api_get_param(query, body, "size", sz, sizeof sz);
        if (g <= 0) { text(r, 400, "bad parameter"); return; }
        st = ups_precheck(root, p, sz, hx, LIMIT_MAX_UPLOAD, LIMIT_RESERVE);
        if (st == 0)
            text(r, 200, "ok");
        else
            text(r, st, "upload not possible");
        return;
    }

    text(r, 404, "not found");
}
