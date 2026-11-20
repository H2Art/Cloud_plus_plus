#ifndef LISTING_H
#define LISTING_H
#include <stddef.h>

enum {
    LISTING_OK = 0,
    LISTING_EPATH = -1,   /* путь вне корня или некорректный */
    LISTING_ENOENT = -2,
    LISTING_ENOTDIR = -3,
    LISTING_EIO = -4,
    LISTING_ETRUNC = -5,  /* out мал */
    LISTING_ENOMEM = -6
};

/* "1.5 KB", "512 B". */
void format_size(unsigned long long n, char *out, size_t outsz);

/* Рендерит HTML-фрагмент (таблицу) каталога root/rel в out.
 * root: канонический путь. rel: декодированный путь от клиента.
 * Папки сверху, дальше побайтовая сортировка по имени.
 * Симлинки и не-обычные файлы пропускаются. Время в UTC. */
int listing_render(const char *root, const char *rel, char *out, size_t outsz);

#endif