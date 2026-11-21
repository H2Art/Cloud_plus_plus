#ifndef FILEOPS_H
#define FILEOPS_H
#include <stddef.h>

enum {
    FOP_OK = 0,
    FOP_EINVAL = -1,    /* плохое имя или путь вне корня */
    FOP_EEXIST = -2,
    FOP_ENOENT = -3,    /* нет самой записи или родительского каталога */
    FOP_ENOTEMPTY = -4,
    FOP_EFULL = -5,     /* ENOSPC / EDQUOT */
    FOP_EIO = -6
};

/* 1, если name допустимо как имя файла или папки: 1..255 байт,
 * не "." и "..", не начинается с FILEOPS_TMP_PREFIX, без '/' и управляющих символов (<0x20, 0x7f). */
int fileops_valid_name(const char *name);

/* Создаёт папку root/rel (родитель должен существовать). */
int fileops_mkdir(const char *root, const char *rel);

/* Создаёт новый файл (O_EXCL|O_NOFOLLOW, права 0640) и возвращает
 * открытый на запись дескриптор в *fd. Закрывает вызывающий. */
int fileops_create(const char *root, const char *rel, int *fd);

/* Удаляет файл или пустую папку. Симлинк удаляется сам, цель не трогается.
 * Корень удалить нельзя. */
int fileops_remove(const char *root, const char *rel);

/* Префикс временных файлов загрузки. Такие имена нельзя создать, удалить
 * или проверить через обычный API, а listing их скрывает. */
#define FILEOPS_TMP_PREFIX ".part-"
int fileops_is_tmp_name(const char *name);

/* Делит rel на каталог (dir, может быть "") и последнее имя (name),
 * проверяет имя через fileops_valid_name. Трейлинг '/' отбрасывается. */
int fileops_split(const char *rel, char *dir, size_t dsz, char *name, size_t nsz);

/* 1 - запись есть (симлинк в конце не разворачивается), 0 - нет,
 * отрицательное - ошибка (FOP_*). */
int fileops_exists(const char *root, const char *rel);

/* Создаёт временный файл в каталоге dirrel, возвращает fd и имя
 * (name размером не меньше NAME_MAX + 1). */
int fileops_create_tmp(const char *root, const char *dirrel, int *fd,
                       char *name, size_t nsz);

/* Публикует временный файл под именем final_name без перезаписи
 * (link + unlink). При FOP_EEXIST временный файл остаётся: вызвать discard. */
int fileops_commit_tmp(const char *root, const char *dirrel,
                       const char *tmpname, const char *final_name);

/* Удаляет временный файл (только с префиксом FILEOPS_TMP_PREFIX). */
int fileops_discard_tmp(const char *root, const char *dirrel, const char *tmpname);

#endif
