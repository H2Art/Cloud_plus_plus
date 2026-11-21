#include "upload.h"
#include "fileops.h"
#include "quota.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct upload {
    char root[PATH_MAX];
    char dir[PATH_MAX];
    char tmp[NAME_MAX + 1];
    char name[NAME_MAX + 1];
    int fd;
    unsigned long long expected, written;
};

static int map_fop(int rc)
{
    switch (rc) {
    case FOP_EINVAL: return UPLOAD_EINVAL;
    case FOP_EEXIST: return UPLOAD_EEXIST;
    case FOP_ENOENT: return UPLOAD_ENOENT;
    case FOP_EFULL:  return UPLOAD_EFULL;
    default:         return UPLOAD_EIO;
    }
}

int upload_precheck(const char *root, const char *rel, unsigned long long expected,
                    unsigned long long reserve)
{
    char dir[PATH_MAX], name[NAME_MAX + 1];
    quota_info q;
    int rc;

    if (!root || !rel)
        return UPLOAD_EINVAL;
    if (fileops_split(rel, dir, sizeof dir, name, sizeof name) != FOP_OK)
        return UPLOAD_EINVAL;
    rc = fileops_exists(root, rel);
    if (rc == 1)
        return UPLOAD_EEXIST;
    if (rc < 0)
        return map_fop(rc);
    if (quota_get(root, &q) != 0)
        return UPLOAD_EIO;
    if (!quota_check(&q, expected, reserve))
        return UPLOAD_EFULL;
    return UPLOAD_OK;
}

int upload_begin(const char *root, const char *rel, unsigned long long expected,
                 unsigned long long reserve, upload **out)
{
    upload *u;
    int rc;

    if (!out)
        return UPLOAD_EINVAL;
    *out = NULL;
    if (!root || !rel || strlen(root) >= PATH_MAX)
        return UPLOAD_EINVAL;

    rc = upload_precheck(root, rel, expected, reserve);
    if (rc != UPLOAD_OK)
        return rc;

    u = calloc(1, sizeof *u);
    if (!u)
        return UPLOAD_EIO;
    u->fd = -1;
    u->expected = expected;
    memcpy(u->root, root, strlen(root) + 1);

    if (fileops_split(rel, u->dir, sizeof u->dir, u->name, sizeof u->name) != FOP_OK) {
        free(u);
        return UPLOAD_EINVAL;
    }
    rc = fileops_create_tmp(root, u->dir, &u->fd, u->tmp, sizeof u->tmp);
    if (rc != FOP_OK) {
        free(u);
        return map_fop(rc);
    }
    *out = u;
    return UPLOAD_OK;
}

int upload_write(upload *u, const void *data, unsigned long len)
{
    const char *p = data;

    if (!u || u->fd < 0 || (len && !data))
        return UPLOAD_EINVAL;
    if (len > u->expected - u->written)
        return UPLOAD_EOVERFLOW;
    while (len > 0) {
        ssize_t n = write(u->fd, p, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return quota_is_full_errno(errno) ? UPLOAD_EFULL : UPLOAD_EIO;
        }
        p += n;
        len -= (unsigned long)n;
        u->written += (unsigned long long)n;
    }
    return UPLOAD_OK;
}

void upload_abort(upload *u)
{
    if (!u)
        return;
    if (u->fd >= 0)
        close(u->fd);
    fileops_discard_tmp(u->root, u->dir, u->tmp);
    free(u);
}

int upload_commit(upload *u)
{
    int rc;

    if (!u || u->fd < 0)
        return UPLOAD_EINVAL;
    if (u->written != u->expected) {
        upload_abort(u);
        return UPLOAD_ESHORT;
    }
    if (fsync(u->fd) != 0) {
        rc = quota_is_full_errno(errno) ? UPLOAD_EFULL : UPLOAD_EIO;
        upload_abort(u);
        return rc;
    }
    rc = close(u->fd);
    u->fd = -1;
    if (rc != 0) {
        rc = quota_is_full_errno(errno) ? UPLOAD_EFULL : UPLOAD_EIO;
        upload_abort(u);
        return rc;
    }
    rc = fileops_commit_tmp(u->root, u->dir, u->tmp, u->name);
    if (rc != FOP_OK) {
        int mapped = map_fop(rc);
        upload_abort(u);
        return mapped;
    }
    free(u);
    return UPLOAD_OK;
}
