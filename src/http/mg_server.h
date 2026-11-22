#ifndef MG_SERVER_H
#define MG_SERVER_H
#include <signal.h>

/* Запускает HTTP-сервер на listen_url и крутит цикл, пока *stop == 0.
 * opts->root: канонический путь хранилища (строка должна жить дольше сервера).
 * opts->static_dir: канонический путь к статике для /static/ или NULL (тогда
 * статику отдаёт nginx).
 * opts->serve_downloads: отдавать /dl/ самим (в проде это делает nginx).
 * Возвращает 0 при штатной остановке, -1 если не удалось начать слушать. */
struct server_opts {
    const char *root;
    const char *static_dir;   /* NULL: статику отдаёт nginx */
    int serve_downloads;      /* 1: GET /dl/<путь> отдаёт сам бэкенд (dev-режим) */
};

int server_run(const struct server_opts *opts, const char *listen_url,
               volatile sig_atomic_t *stop);

#endif
