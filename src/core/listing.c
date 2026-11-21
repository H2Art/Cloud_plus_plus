#include "listing.h"
#include "fileops.h"
#include "html.h"
#include "path.h"
#include "url.h"
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

typedef struct {
    char *name;
    int is_dir;
    unsigned long long size;
    time_t mtime;
} entry;

typedef struct {
    char *buf;
    size_t cap, len;
    int err;
} sb;

static void sb_add(sb *b, const char *s)
{
    size_t n = strlen(s);
    if (b->err || b->len + n >= b->cap) {
        b->err = 1;
        return;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

void format_size(unsigned long long n, char *out, size_t outsz)
{
    static const char *u[] = { "B", "KB", "MB", "GB", "TB" };
    double v;
    int i = 0;
    if (n < 1024) {
        snprintf(out, outsz, "%llu B", n);
        return;
    }
    v = (double)n;
    while (v >= 1024 && i < 4) {
        v /= 1024;
        i++;
    }
    snprintf(out, outsz, "%.1f %s", v, u[i]);
}

static int cmp_entry(const void *a, const void *b)
{
    const entry *x = a, *y = b;
    if (x->is_dir != y->is_dir)
        return y->is_dir - x->is_dir;
    return strcmp(x->name, y->name);
}

static void add_nav(sb *b, const char *enc)
{
    sb_add(b, "href=\"/?p=");
    sb_add(b, enc);
    sb_add(b, "\" hx-get=\"/list?p=");
    sb_add(b, enc);
    sb_add(b, "\" hx-target=\"#list\" hx-push-url=\"/?p=");
    sb_add(b, enc);
    sb_add(b, "\"");
}

int listing_render(const char *root, const char *rel, char *out, size_t outsz)
{
    char dir[PATH_MAX], full[PATH_MAX], child[PATH_MAX], enc[PATH_MAX * 3 + 1];
    char esc[NAME_MAX * 6 + 8], szbuf[32], tbuf[32];
    const char *crel;
    struct stat st;
    DIR *d;
    struct dirent *de;
    entry *es = NULL;
    size_t n = 0, cap = 0;
    sb b;
    int r, rc = LISTING_OK;

    if (!out || outsz == 0)
        return LISTING_ETRUNC;
    out[0] = '\0';

    r = path_resolve(root, rel, dir, sizeof dir);
    if (r == PATH_ENOENT)
        return LISTING_ENOENT;
    if (r != PATH_OK)
        return LISTING_EPATH;
    if (stat(dir, &st) != 0)
        return LISTING_ENOENT;
    if (!S_ISDIR(st.st_mode))
        return LISTING_ENOTDIR;

    crel = dir + strlen(root);
    while (*crel == '/')
        crel++;

    d = opendir(dir);
    if (!d)
        return LISTING_EIO;
    while ((de = readdir(d)) != NULL) {
        int len;
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;
        if (fileops_is_tmp_name(de->d_name))
            continue;
        len = snprintf(full, sizeof full, "%s/%s", dir, de->d_name);
        if (len < 0 || (size_t)len >= sizeof full)
            continue;
        if (lstat(full, &st) != 0 || S_ISLNK(st.st_mode))
            continue;
        if (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode))
            continue;
        if (n == cap) {
            size_t nc = cap ? cap * 2 : 32;
            entry *t = realloc(es, nc * sizeof *es);
            if (!t) { rc = LISTING_ENOMEM; break; }
            es = t;
            cap = nc;
        }
        es[n].name = strdup(de->d_name);
        if (!es[n].name) { rc = LISTING_ENOMEM; break; }
        es[n].is_dir = S_ISDIR(st.st_mode);
        es[n].size = (unsigned long long)st.st_size;
        es[n].mtime = st.st_mtime;
        n++;
    }
    closedir(d);
    if (rc != LISTING_OK)
        goto done;

    if (n > 1)
        qsort(es, n, sizeof *es, cmp_entry);

    b.buf = out; b.cap = outsz; b.len = 0; b.err = 0;
    sb_add(&b, "<table class=\"files\">\n");

    if (*crel) {
        /* родитель: crel без последнего компонента */
        char par[PATH_MAX];
        char *slash;
        snprintf(par, sizeof par, "%s", crel);
        slash = strrchr(par, '/');
        if (slash) *slash = '\0'; else par[0] = '\0';
        if (url_encode(par, enc, sizeof enc) != 0) { rc = LISTING_EPATH; goto done; }
        sb_add(&b, "<tr><td colspan=\"4\"><a ");
        add_nav(&b, enc);
        sb_add(&b, ">..</a></td></tr>\n");
    }

    for (size_t i = 0; i < n; i++) {
        entry *e = &es[i];
        struct tm tmv;
        int len = *crel ? snprintf(child, sizeof child, "%s/%s", crel, e->name)
                        : snprintf(child, sizeof child, "%s", e->name);
        if (len < 0 || (size_t)len >= sizeof child)
            continue;
        if (url_encode(child, enc, sizeof enc) != 0)
            continue;
        if (html_escape(e->name, esc, sizeof esc) != 0)
            continue;

        sb_add(&b, "<tr><td>");
        if (e->is_dir) {
            sb_add(&b, "<a ");
            add_nav(&b, enc);
            sb_add(&b, ">");
            sb_add(&b, esc);
            sb_add(&b, "/</a>");
            snprintf(szbuf, sizeof szbuf, "-");
        } else {
            sb_add(&b, "<a href=\"/dl/");
            sb_add(&b, enc);
            sb_add(&b, "\">");
            sb_add(&b, esc);
            sb_add(&b, "</a>");
            format_size(e->size, szbuf, sizeof szbuf);
        }
        sb_add(&b, "</td><td>");
        sb_add(&b, szbuf);
        sb_add(&b, "</td><td>");
        if (gmtime_r(&e->mtime, &tmv))
            strftime(tbuf, sizeof tbuf, "%Y-%m-%d %H:%M", &tmv);
        else
            snprintf(tbuf, sizeof tbuf, "-");
        sb_add(&b, tbuf);
        sb_add(&b, "</td><td><button class=\"del\" title=\"\xD0\xA3\xD0\xB4\xD0\xB0\xD0\xBB\xD0\xB8\xD1\x82\xD1\x8C\" aria-label=\"\xD0\xA3\xD0\xB4\xD0\xB0\xD0\xBB\xD0\xB8\xD1\x82\xD1\x8C\" hx-delete=\"/rm?p=");
        sb_add(&b, enc);
        sb_add(&b, "\" hx-target=\"#list\" hx-confirm=\"\xD0\xA3\xD0\xB4\xD0\xB0\xD0\xBB\xD0\xB8\xD1\x82\xD1\x8C?\">\xE2\x9C\x95</button></td></tr>\n");
    }
    if (n == 0)
        sb_add(&b, "<tr><td colspan=\"4\">(empty)</td></tr>\n");
    sb_add(&b, "</table>\n");
    if (b.err) {
        out[0] = '\0';
        rc = LISTING_ETRUNC;
    }

done:
    for (size_t i = 0; i < n; i++)
        free(es[i].name);
    free(es);
    return rc;
}
