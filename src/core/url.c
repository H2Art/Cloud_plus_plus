#include "url.h"

static int hexv(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int is_safe(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
           c == '~' || c == '/';
}

int url_encode(const char *in, char *out, size_t outsz)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    if (!in || !out || outsz == 0)
        return -1;
    for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
        if (is_safe(*p)) {
            if (o + 1 >= outsz) goto fail;
            out[o++] = (char)*p;
        } else {
            if (o + 3 >= outsz) goto fail;
            out[o++] = '%';
            out[o++] = hex[*p >> 4];
            out[o++] = hex[*p & 15];
        }
    }
    out[o] = '\0';
    return 0;
fail:
    out[o] = '\0';
    return -1;
}

static int decode(const char *in, char *out, size_t outsz, int form)
{
    size_t o = 0;
    if (!in || !out || outsz == 0)
        return -1;
    for (; *in; in++) {
        unsigned char c = (unsigned char)*in;
        if (form && c == '+') {
            c = ' ';
        } else if (c == '%') {
            int h = hexv(in[1]);
            int l = h < 0 ? -1 : hexv(in[2]);
            if (h < 0 || l < 0) goto fail;
            c = (unsigned char)(h * 16 + l);
            if (c == 0) goto fail;
            in += 2;
        }
        if (o + 1 >= outsz) goto fail;
        out[o++] = (char)c;
    }
    out[o] = '\0';
    return 0;
fail:
    out[o] = '\0';
    return -1;
}

int url_decode(const char *in, char *out, size_t outsz)
{
    return decode(in, out, outsz, 0);
}

int url_decode_form(const char *in, char *out, size_t outsz)
{
    return decode(in, out, outsz, 1);
}
