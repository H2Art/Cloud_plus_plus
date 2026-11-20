#ifndef QUOTA_H
#define QUOTA_H

typedef struct {
    unsigned long long total; /* размер ФС, байт */
    unsigned long long avail; /* доступно непривилегированному процессу, байт */
} quota_info;

/* Читает размер и свободное место ФС, на которой лежит path (statvfs).
 * 0 = ок, -1 = ошибка. Если storage смонтирован как отдельный loop-образ,
 * то лимит хранилища равен размеру образа. */
int quota_get(const char *path, quota_info *out);

/* 1, если файл на incoming байт поместится с остатком reserve, иначе 0.
 * Без переполнения при огромных значениях. */
int quota_check(const quota_info *q, unsigned long long incoming,
                unsigned long long reserve);

/* 1, если errno означает "нет места" (ENOSPC или EDQUOT). */
int quota_is_full_errno(int err);

#endif
