#include "html.h"
#include <string.h>

int html_escape(const char *in, char *out, size_t outsz)
{
    size_t o = 0;
    if (!in || !out || outsz == 0)
        return -1;
    for (; *in; in++) {
        const char *rep;
        switch (*in) {
        case '&':  rep = "&amp;";  break;
        case '<':  rep = "&lt;";   break;
        case '>':  rep = "&gt;";   break;
        case '"':  rep = "&quot;"; break;
        case '\'': rep = "&#39;";  break;
        default:   rep = NULL;
        }
        size_t l = rep ? strlen(rep) : 1;
        if (o + l >= outsz) {
            out[o] = '\0';
            return -1;
        }
        if (rep)
            memcpy(out + o, rep, l);
        else
            out[o] = *in;
        o += l;
    }
    out[o] = '\0';
    return 0;
}