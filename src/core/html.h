#ifndef HTML_H
#define HTML_H
#include <stddef.h>

/* Экранирует & < > " ' . Возвращает 0 при успехе, -1 если out мал.
 * out всегда завершён нулём (если outsz > 0). */
int html_escape(const char *in, char *out, size_t outsz);

#endif