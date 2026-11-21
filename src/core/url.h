#ifndef URL_H
#define URL_H
#include <stddef.h>

/* Кодирует всё, кроме A-Z a-z 0-9 - . _ ~ и '/'. 0 = ок, -1 = out мал. */
int url_encode(const char *in, char *out, size_t outsz);

/* Декодирует %XX один раз. '+' не трогает (это путь, не форма).
 * -1 при неверной последовательности %, при %00 или малом out. */
int url_decode(const char *in, char *out, size_t outsz);

/* Как url_decode, но '+' превращается в пробел (application/x-www-form-urlencoded). */
int url_decode_form(const char *in, char *out, size_t outsz);

#endif
