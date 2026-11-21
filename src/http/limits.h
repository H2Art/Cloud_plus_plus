#ifndef LIMITS_H
#define LIMITS_H

/* Максимальный размер одного файла. Должен совпадать с client_max_body_size
 * в nginx (500m = 500 * 1024 * 1024 байт). */
#define LIMIT_MAX_UPLOAD (500ULL << 20)

/* Свободное место, которое всегда остаётся на диске хранилища. */
#define LIMIT_RESERVE (1ULL << 20)

#endif
