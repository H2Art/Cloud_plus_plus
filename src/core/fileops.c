#include "fileops.h"
#include "path.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int fileops_valid_name(const char *name)
{
    size_t n;
    if (!name)
        return 0;
    n = strlen(name);
    if (n == 0 || n > NAME_MAX)
        return 0;
    if (!strcmp(name, ".") || !strcmp(name, ".."))
        return 0;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++)
        if (*p < 0x20 || *p == 0x7f || *p == '/')
            return 0;
    return 1;
}

static int map_errno(int e)
{
    if (e == EEXIST) return FOP_EEXIST;
    if (e == ENOENT || e == ENOTDIR) return FOP_ENOENT;
    if (e == ENOSPC || e == EDQUOT) return FOP_EFULL;
    if (e == ENOTEMPTY) return FOP_ENOTEMPTY;
    return FOP_EIO;
}

/* Делит rel на родителя и имя, канонизирует родителя (проверка корня и
 * симлинков), открывает его. Последний компонент НЕ разворачивается,
 * поэтому симлинк в конце пути не прослеживается. */
static int open_parent(const char *root, const char *rel, int *pfd,
                       char *name, size_t nsz)
{
    char tmp[PATH_MAX], parent[PATH_MAX];
    const char *dirpart, *base;
    char *slash;
    size_t len;
    int r;

    if (!root || !rel || !pfd)
        return FOP_EINVAL;
    len = strlen(rel);
    if (len >= sizeof tmp)
        return FOP_EINVAL;
    memcpy(tmp, rel, len + 1);
    while (len > 0 && tmp[len - 1] == '/')
        tmp[--len] = '\0';

    slash = strrchr(tmp, '/');
    if (slash) {
        *slash = '\0';
        dirpart = tmp;
        base = slash + 1;
    } else {
        dirpart = "";
        base = tmp;
    }
    if (!fileops_valid_name(base) || strlen(base) >= nsz)
        return FOP_EINVAL;
    memcpy(name, base, strlen(base) + 1);

    r = path_resolve(root, dirpart, parent, sizeof parent);
    if (r == PATH_ENOENT)
        return FOP_ENOENT;
    if (r != PATH_OK)
        return FOP_EINVAL;

    *pfd = open(parent, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (*pfd < 0)
        return map_errno(errno);
    return FOP_OK;
}

int fileops_mkdir(const char *root, const char *rel)
{
    char name[NAME_MAX + 1];
    int pfd, rc;

    rc = open_parent(root, rel, &pfd, name, sizeof name);
    if (rc != FOP_OK)
        return rc;
    rc = mkdirat(pfd, name, 0750) == 0 ? FOP_OK : map_errno(errno);
    close(pfd);
    return rc;
}

int fileops_create(const char *root, const char *rel, int *fd)
{
    char name[NAME_MAX + 1];
    int pfd, rc, f;

    if (!fd)
        return FOP_EINVAL;
    rc = open_parent(root, rel, &pfd, name, sizeof name);
    if (rc != FOP_OK)
        return rc;
    f = openat(pfd, name,
               O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0640);
    rc = f >= 0 ? FOP_OK : map_errno(errno);
    close(pfd);
    if (rc == FOP_OK)
        *fd = f;
    return rc;
}

int fileops_remove(const char *root, const char *rel)
{
    char name[NAME_MAX + 1];
    struct stat st;
    int pfd, rc, r;

    rc = open_parent(root, rel, &pfd, name, sizeof name);
    if (rc != FOP_OK)
        return rc;
    if (fstatat(pfd, name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
        rc = map_errno(errno);
    } else {
        r = unlinkat(pfd, name, S_ISDIR(st.st_mode) ? AT_REMOVEDIR : 0);
        if (r == 0)
            rc = FOP_OK;
        else if (errno == EEXIST)       /* на некоторых ФС вместо ENOTEMPTY */
            rc = FOP_ENOTEMPTY;
        else
            rc = map_errno(errno);
    }
    close(pfd);
    return rc;
}
