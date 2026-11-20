#include "quota.h"
#include <errno.h>
#include <sys/statvfs.h>

int quota_get(const char *path, quota_info *out)
{
    struct statvfs sv;
    unsigned long long bs;
    if (!path || !out)
        return -1;
    if (statvfs(path, &sv) != 0)
        return -1;
    bs = sv.f_frsize ? sv.f_frsize : sv.f_bsize;
    out->total = (unsigned long long)sv.f_blocks * bs;
    out->avail = (unsigned long long)sv.f_bavail * bs;
    return 0;
}

int quota_check(const quota_info *q, unsigned long long incoming,
                unsigned long long reserve)
{
    if (!q)
        return 0;
    if (reserve > q->avail)
        return 0;
    return incoming <= q->avail - reserve;
}

int quota_is_full_errno(int err)
{
    return err == ENOSPC || err == EDQUOT;
}
