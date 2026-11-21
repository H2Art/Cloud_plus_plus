#ifndef API_H
#define API_H
#include <stddef.h>

/* Слой маршрутов без привязки к Mongoose: принимает разобранный запрос,
 * возвращает готовый ответ. Все строки на входе завершены нулём. */

typedef struct {
    int status;
    const char *ctype;   /* статическая строка */
    char *body;          /* malloc, может быть NULL (пустое тело) */
    size_t len;
} api_resp;

void api_resp_free(api_resp *r);

/* Маршруты:
 *   GET    /       полная страница (p=путь)
 *   GET    /list   фрагмент для HTMX (p=путь)
 *   POST   /mkdir  dir=каталог, name=имя          (нужен hx=1)
 *   DELETE /rm     p=путь                         (нужен hx=1)
 * Параметры берутся из query, для POST также из тела (form-urlencoded).
 * hx != 0, если пришёл заголовок "HX-Request: true"; без него изменяющие
 * запросы отклоняются (403): простая защита от CSRF.
 * Ответ заполняется всегда; r нужно освободить через api_resp_free. */
void api_dispatch(const char *root, const char *method, const char *uri,
                  const char *query, const char *body, int hx, api_resp *r);

#endif
