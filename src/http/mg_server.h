#ifndef MG_SERVER_H
#define MG_SERVER_H
#include <signal.h>

/* Запускает HTTP-сервер на listen_url и крутит цикл, пока *stop == 0.
 * root: канонический путь хранилища (строка должна жить дольше сервера).
 * Возвращает 0 при штатной остановке, -1 если не удалось начать слушать. */
int server_run(const char *root, const char *listen_url, volatile sig_atomic_t *stop);

#endif
