#ifndef PATH_H
#define PATH_H
#include <stddef.h>

enum { PATH_OK = 0, PATH_EINVAL = -1, PATH_ETOOLONG = -2, PATH_ENOENT = -3 };

/* root: канонический абсолютный путь (результат realpath), не "/".
 * rel: путь от клиента, уже декодированный из URL.
 * Результат: канонический путь внутри root (файл может не существовать,
 * но его родитель должен существовать). */
int path_resolve(const char *root, const char *rel, char *out, size_t outsz);

#endif