#ifndef TEST_H
#define TEST_H
#include <stdio.h>
static int t_run, t_fail;
#define CHECK(c) do { t_run++; if (!(c)) { t_fail++; \
    fprintf(stderr, "%s:%d: FAIL: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define DONE() do { printf("%s: %d checks, %d failed\n", __FILE__, t_run, t_fail); \
    return t_fail ? 1 : 0; } while (0)
#endif