#ifndef UPS_H
#define UPS_H
#include <stddef.h>

/* Потоковая загрузка без привязки к Mongoose: begin -> chunk* -> end.
 * Функции begin/precheck возвращают 0 или HTTP-статус отказа:
 * 400 плохой запрос, 403 нет HX-Request, 404 нет каталога, 409 файл есть,
 * 413 больше лимита, 507 нет места, 500 внутренняя ошибка. */

typedef struct ups ups;

/* Проверка без побочных эффектов. p - уже декодированный путь,
 * size_str - строка с десятичным числом байт. */
int ups_precheck(const char *root, const char *p, const char *size_str, int hx,
                 unsigned long long max_size, unsigned long long reserve);

/* Начало загрузки: путь берётся из параметра p в query, размер из
 * заголовка Content-Length (content_length может быть NULL -> 400). */
int ups_begin(const char *root, const char *query, const char *content_length,
              int hx, unsigned long long max_size, unsigned long long reserve,
              ups **out);

/* Порция данных. 0 = ок. После первой ошибки загрузка считается проваленной. */
int ups_chunk(ups *s, const void *data, size_t len);

/* Конец потока (всегда вызывать ровно один раз): публикует файл, только если
 * не было ошибок и получено ровно заявленное число байт, иначе удаляет
 * временный файл. Освобождает s. Возвращает 0, если файл опубликован. NULL допустим. */
int ups_end(ups *s);

#endif
