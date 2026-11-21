#include "http/mg_server.h"
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop;

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static int usage(void)
{
    fprintf(stderr, "usage: cloud --root DIR [--listen http://127.0.0.1:PORT]\n");
    return 2;
}

int main(int argc, char **argv)
{
    const char *root_arg = NULL, *listen_url = "http://127.0.0.1:8080";
    char root[PATH_MAX];
    struct sigaction sa;
    struct stat st;
    int rc;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--root") && i + 1 < argc)
            root_arg = argv[++i];
        else if (!strcmp(argv[i], "--listen") && i + 1 < argc)
            listen_url = argv[++i];
        else
            return usage();
    }
    if (!root_arg)
        return usage();

    /* У бэкенда нет своей авторизации: слушать можно только localhost,
     * наружу его выставляет nginx. */
    if (strncmp(listen_url, "http://127.0.0.1:", 17) != 0) {
        fprintf(stderr, "cloud: --listen must start with http://127.0.0.1: "
                        "(no built-in auth, expose only through nginx)\n");
        return 2;
    }
    if (!realpath(root_arg, root)) {
        perror("cloud: root");
        return 1;
    }
    if (stat(root, &st) != 0 || !S_ISDIR(st.st_mode) || strcmp(root, "/") == 0) {
        fprintf(stderr, "cloud: root must be an existing directory other than /\n");
        return 1;
    }

    umask(027);   /* файлы 0640, папки 0750: читает группа (nginx) */

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    rc = server_run(root, listen_url, &g_stop);
    return rc == 0 ? 0 : 1;
}
