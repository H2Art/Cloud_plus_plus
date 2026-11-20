#ifndef FILEOPS_H
#define FILEOPS_H

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
 * не "." и "..", без '/' и управляющих символов (<0x20, 0x7f). */
int fileops_valid_name(const char *name);

/* Создаёт папку root/rel (родитель должен существовать). */
int fileops_mkdir(const char *root, const char *rel);

/* Создаёт новый файл (O_EXCL|O_NOFOLLOW, права 0640) и возвращает
 * открытый на запись дескриптор в *fd. Закрывает вызывающий. */
int fileops_create(const char *root, const char *rel, int *fd);

/* Удаляет файл или пустую папку. Симлинк удаляется сам, цель не трогается.
 * Корень удалить нельзя. */
int fileops_remove(const char *root, const char *rel);

#endif
