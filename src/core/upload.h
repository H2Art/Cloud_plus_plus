#ifndef UPLOAD_H
#define UPLOAD_H

enum {
    UPLOAD_OK = 0,
    UPLOAD_EINVAL = -1,
    UPLOAD_EEXIST = -2,     /* файл уже есть (перезапись запрещена) */
    UPLOAD_ENOENT = -3,     /* нет каталога назначения */
    UPLOAD_EFULL = -4,      /* не хватает места */
    UPLOAD_EIO = -5,
    UPLOAD_ESHORT = -6,     /* получено меньше заявленного */
    UPLOAD_EOVERFLOW = -7   /* получено больше заявленного */
};

typedef struct upload upload;

/* Начинает загрузку файла root/rel размером expected байт.
 * Проверяет путь, что файла ещё нет и что есть место с запасом reserve.
 * Данные пишутся во временный файл (скрыт в listing). */
int upload_begin(const char *root, const char *rel, unsigned long long expected,
                 unsigned long long reserve, upload **out);

/* Дописывает порцию. После любой ошибки вызывающий обязан вызвать upload_abort. */
int upload_write(upload *u, const void *data, unsigned long len);

/* Публикует файл, если получено ровно expected байт. Освобождает u при
 * любом исходе; при ошибке временный файл удаляется. */
int upload_commit(upload *u);

/* Отменяет загрузку: удаляет временный файл, освобождает u. NULL допустим. */
void upload_abort(upload *u);

#endif
