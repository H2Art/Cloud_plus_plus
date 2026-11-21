#include "ups.h"
#include "core/upload.h"
#include "http/api.h"
#include <limits.h>
#include <stdlib.h>

struct ups {
    upload *u;
    int failed;
};

static int status_of_upload(int rc)
{
    switch (rc) {
    case UPLOAD_OK:        return 0;
    case UPLOAD_EINVAL:    return 400;
    case UPLOAD_EEXIST:    return 409;
    case UPLOAD_ENOENT:    return 404;
    case UPLOAD_EFULL:     return 507;
    case UPLOAD_ESHORT:
    case UPLOAD_EOVERFLOW: return 400;
    default:               return 500;
    }
}

static int parse_size(const char *s, unsigned long long *out)
{
    unsigned long long v = 0;
    if (!s || !*s)
        return -1;
    for (; *s; s++) {
        unsigned d;
        if (*s < '0' || *s > '9')
            return -1;
        d = (unsigned)(*s - '0');
        if (v > (ULLONG_MAX - d) / 10)
            return -1;
        v = v * 10 + d;
    }
    *out = v;
    return 0;
}

static int check(const char *root, const char *p, const char *size_str, int hx,
                 unsigned long long max_size, unsigned long long reserve,
                 unsigned long long *size)
{
    if (!hx)
        return 403;
    if (!p || !*p)
        return 400;
    if (parse_size(size_str, size) != 0)
        return 400;
    if (*size > max_size)
        return 413;
    return status_of_upload(upload_precheck(root, p, *size, reserve));
}

int ups_precheck(const char *root, const char *p, const char *size_str, int hx,
                 unsigned long long max_size, unsigned long long reserve)
{
    unsigned long long size;
    return check(root, p, size_str, hx, max_size, reserve, &size);
}

int ups_begin(const char *root, const char *query, const char *content_length,
              int hx, unsigned long long max_size, unsigned long long reserve,
              ups **out)
{
    char p[PATH_MAX];
    unsigned long long size;
    upload *u = NULL;
    ups *s;
    int st;

    if (!out)
        return 500;
    *out = NULL;
    if (!hx)
        return 403;
    if (api_get_param(query, NULL, "p", p, sizeof p) <= 0)
        return 400;
    st = check(root, p, content_length, hx, max_size, reserve, &size);
    if (st != 0)
        return st;
    st = status_of_upload(upload_begin(root, p, size, reserve, &u));
    if (st != 0)
        return st;
    s = calloc(1, sizeof *s);
    if (!s) {
        upload_abort(u);
        return 500;
    }
    s->u = u;
    *out = s;
    return 0;
}

int ups_chunk(ups *s, const void *data, size_t len)
{
    if (!s || s->failed)
        return -1;
    if (upload_write(s->u, data, (unsigned long)len) != UPLOAD_OK) {
        s->failed = 1;
        return -1;
    }
    return 0;
}

int ups_end(ups *s)
{
    int rc = -1;
    if (!s)
        return -1;
    if (s->failed)
        upload_abort(s->u);
    else
        rc = upload_commit(s->u);   /* сам освобождает и проверяет размер */
    free(s);
    return rc == UPLOAD_OK ? 0 : -1;
}
