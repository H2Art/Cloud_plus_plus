/* Склейка Mongoose и http/api. Здесь нет логики приложения: только разбор
 * запроса в обычные C-строки и отправка ответа. */
#include "mg_server.h"
#include "http/api.h"
#include "mongoose.h"
#include <stdio.h>
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

static void on_event(struct mg_connection *c, int ev, void *ev_data)
{
    struct mg_http_message *hm;
    const struct mg_str *hxh;
    const char *root;
    char method[16], uri[MAX_URI], query[MAX_QUERY], body[MAX_BODY], hdr[512];
    api_resp r;
    int hx;

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
