/* Склейка Mongoose и http/api. Здесь нет логики приложения: только разбор
 * запроса в обычные C-строки и отправка ответа. */
#include "mg_server.h"
#include "http/api.h"
#include "http/limits.h"
#include "http/ups.h"
#include "mongoose.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_URI    2048
#define MAX_QUERY  12288
#define MAX_BODY   8192

#define SEC_HEADERS \
    "X-Content-Type-Options: nosniff\r\n" \
    "Cache-Control: no-store\r\n" \
    "Referrer-Policy: no-referrer\r\n" \
    "Content-Security-Policy: default-src 'self'; style-src 'self' 'unsafe-inline'; " \
    "frame-ancestors 'none'\r\n"

/* Копирует mg_str в буфер с завершающим нулём. 0, если не влезло или есть NUL. */
static int copy_str(const struct mg_str *s, char *out, size_t outsz)
{
    if (s->len >= outsz)
        return 0;
    if (s->len > 0 && memchr(s->buf, '\0', s->len) != NULL)
        return 0;
    if (s->len > 0)
        memcpy(out, s->buf, s->len);
    out[s->len] = '\0';
    return 1;
}

static void reply_text(struct mg_connection *c, int status, const char *msg)
{
    mg_http_reply(c, status,
                  "Content-Type: text/plain; charset=utf-8\r\n" SEC_HEADERS,
                  "%s", msg);
}

/* Состояние одной потоковой загрузки. Выделяется до mg_http_stream_body и
 * освобождается в завершающем вызове колбэка (он приходит ровно один раз). */
struct up_ctx {
    const char *root;
    ups *s;
    int started;
};

static int is_hx(struct mg_http_message *hm)
{
    const struct mg_str *h = mg_http_get_header(hm, "HX-Request");
    return h != NULL && h->len == 4 && memcmp(h->buf, "true", 4) == 0;
}

/* Колбэк mg_http_stream_body:
 *   data != NULL           - очередная порция;
 *   hm != NULL, не начат   - старт (hm действителен только сейчас);
 *   иначе                  - конец (в том числе обрыв или отказ до старта). */
static bool on_upload(struct mg_http_message *hm, struct mg_str *data, void **user_data)
{
    struct up_ctx *ctx = (struct up_ctx *)*user_data;

    if (ctx == NULL)
        return false;

    if (data != NULL) {
        return ctx->s != NULL && ups_chunk(ctx->s, data->buf, data->len) == 0;
    }
    if (hm != NULL && !ctx->started) {
        char query[MAX_QUERY], cl[32];
        const struct mg_str *clh = mg_http_get_header(hm, "Content-Length");
        int st;

        ctx->started = 1;
        if (!copy_str(&hm->query, query, sizeof query) || clh == NULL ||
            !copy_str(clh, cl, sizeof cl))
            return false;
        st = ups_begin(ctx->root, query, cl, is_hx(hm), LIMIT_MAX_UPLOAD,
                       LIMIT_RESERVE, &ctx->s);
        if (st != 0) {
            MG_INFO(("upload rejected: %d", st));
            return false;
        }
        return true;
    }

    /* конец потока */
    if (ctx->s != NULL && ups_end(ctx->s) != 0)
        MG_INFO(("upload discarded"));
    free(ctx);
    *user_data = NULL;
    return true;
}

static void on_event(struct mg_connection *c, int ev, void *ev_data)
{
    struct mg_http_message *hm;
    const struct mg_str *hxh;
    const char *root;
    char method[16], uri[MAX_URI], query[MAX_QUERY], body[MAX_BODY], hdr[512];
    api_resp r;
    int hx;

    if (ev == MG_EV_HTTP_HDRS) {
        /* PUT /up принимаем потоком, без буферизации тела в памяти */
        hm = (struct mg_http_message *)ev_data;
        if (mg_match(hm->uri, mg_str("/up"), NULL) &&
            hm->method.len == 3 && memcmp(hm->method.buf, "PUT", 3) == 0) {
            struct up_ctx *ctx = (struct up_ctx *)calloc(1, sizeof *ctx);
            if (ctx == NULL) {
                reply_text(c, 500, "out of memory");
                return;
            }
            ctx->root = (const char *)c->fn_data;
            mg_http_stream_body(c, hm, on_upload, ctx);
        }
        return;
    }
    if (ev != MG_EV_HTTP_MSG)
        return;
    hm = (struct mg_http_message *)ev_data;
    root = (const char *)c->fn_data;

    if (!copy_str(&hm->method, method, sizeof method) ||
        !copy_str(&hm->uri, uri, sizeof uri) ||
        !copy_str(&hm->query, query, sizeof query)) {
        reply_text(c, 414, "request too long");
        return;
    }
    if (!copy_str(&hm->body, body, sizeof body)) {
        reply_text(c, 413, "body too large");
        return;
    }

    hxh = mg_http_get_header(hm, "HX-Request");
    hx = hxh != NULL && hxh->len == 4 && memcmp(hxh->buf, "true", 4) == 0;

    api_dispatch(root, method, uri, query, body, hx, &r);

    snprintf(hdr, sizeof hdr, "Content-Type: %s\r\n" SEC_HEADERS,
             r.ctype ? r.ctype : "text/plain");
    mg_http_reply(c, r.status, hdr, "%.*s", (int)r.len, r.body ? r.body : "");
    api_resp_free(&r);
}

int server_run(const char *root, const char *listen_url, volatile sig_atomic_t *stop)
{
    struct mg_mgr mgr;

    mg_mgr_init(&mgr);
    mg_log_set(MG_LL_INFO);
    if (mg_http_listen(&mgr, listen_url, on_event, (void *)root) == NULL) {
        fprintf(stderr, "cloud: cannot listen on %s\n", listen_url);
        mg_mgr_free(&mgr);
        return -1;
    }
    MG_INFO(("serving %s on %s", root, listen_url));
    while (!*stop)
        mg_mgr_poll(&mgr, 200);
    mg_mgr_free(&mgr);
    return 0;
}
