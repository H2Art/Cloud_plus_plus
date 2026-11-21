#include "fileops.h"
#include "path.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int fileops_is_tmp_name(const char *name)
{
    return name && strncmp(name, FILEOPS_TMP_PREFIX, strlen(FILEOPS_TMP_PREFIX)) == 0;
}

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
    if (fileops_is_tmp_name(name))
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

int fileops_split(const char *rel, char *dir, size_t dsz, char *name, size_t nsz)
{
    char tmp[PATH_MAX];
    const char *d, *b;
    char *slash;
    size_t len;

    if (!rel || !dir || !name)
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
        d = tmp;
        b = slash + 1;
    } else {
        d = "";
        b = tmp;
    }
    if (!fileops_valid_name(b) || strlen(b) >= nsz || strlen(d) >= dsz)
        return FOP_EINVAL;
    memcpy(dir, d, strlen(d) + 1);
    memcpy(name, b, strlen(b) + 1);
    return FOP_OK;
}

/* Канонизирует каталог (проверка корня и симлинков) и открывает его. */
static int open_dir(const char *root, const char *dirrel, int *pfd)
{
    char parent[PATH_MAX];
    int r;

    if (!root || !dirrel || !pfd)
        return FOP_EINVAL;
    r = path_resolve(root, dirrel, parent, sizeof parent);
    if (r == PATH_ENOENT)
        return FOP_ENOENT;
    if (r != PATH_OK)
        return FOP_EINVAL;
    *pfd = open(parent, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (*pfd < 0)
        return map_errno(errno);
    return FOP_OK;
}

/* Делит rel на каталог и имя, открывает каталог. Последний компонент НЕ
 * разворачивается, поэтому симлинк в конце пути не прослеживается. */
static int open_parent(const char *root, const char *rel, int *pfd,
                       char *name, size_t nsz)
{
    char dir[PATH_MAX];
    int rc = fileops_split(rel, dir, sizeof dir, name, nsz);
    if (rc != FOP_OK)
        return rc;
    return open_dir(root, dir, pfd);
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

int fileops_exists(const char *root, const char *rel)
{
    char name[NAME_MAX + 1];
    struct stat st;
    int pfd, rc;

    rc = open_parent(root, rel, &pfd, name, sizeof name);
    if (rc != FOP_OK)
        return rc;
    if (fstatat(pfd, name, &st, AT_SYMLINK_NOFOLLOW) == 0)
        rc = 1;
    else
        rc = errno == ENOENT ? 0 : map_errno(errno);
    close(pfd);
    return rc;
}

int fileops_create_tmp(const char *root, const char *dirrel, int *fd,
                       char *name, size_t nsz)
{
    static unsigned counter;
    char n[NAME_MAX + 1];
    int pfd, rc = FOP_EEXIST, f;

    if (!fd || !name || nsz < sizeof n)
        return FOP_EINVAL;
    rc = open_dir(root, dirrel, &pfd);
    if (rc != FOP_OK)
        return rc;
    rc = FOP_EEXIST;
    for (int i = 0; i < 16; i++) {
        snprintf(n, sizeof n, "%s%ld-%u", FILEOPS_TMP_PREFIX, (long)getpid(), counter++);
        f = openat(pfd, n, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0640);
        if (f >= 0) {
            *fd = f;
            memcpy(name, n, strlen(n) + 1);
            rc = FOP_OK;
            break;
        }
        if (errno != EEXIST) {
            rc = map_errno(errno);
            break;
        }
    }
    close(pfd);
    return rc;
}

int fileops_commit_tmp(const char *root, const char *dirrel,
                       const char *tmpname, const char *final_name)
{
    int pfd, rc;

    if (!fileops_is_tmp_name(tmpname) || strchr(tmpname, '/') ||
        !fileops_valid_name(final_name))
        return FOP_EINVAL;
    rc = open_dir(root, dirrel, &pfd);
    if (rc != FOP_OK)
        return rc;
    if (linkat(pfd, tmpname, pfd, final_name, 0) != 0)
        rc = errno == EPERM ? FOP_EIO : map_errno(errno);
    else if (unlinkat(pfd, tmpname, 0) != 0)
        rc = FOP_EIO;
    else
        rc = FOP_OK;
    close(pfd);
    return rc;
}

int fileops_discard_tmp(const char *root, const char *dirrel, const char *tmpname)
{
    int pfd, rc;

    if (!fileops_is_tmp_name(tmpname) || strchr(tmpname, '/'))
        return FOP_EINVAL;
    rc = open_dir(root, dirrel, &pfd);
    if (rc != FOP_OK)
        return rc;
    rc = unlinkat(pfd, tmpname, 0) == 0 ? FOP_OK : map_errno(errno);
    close(pfd);
    return rc;
}
