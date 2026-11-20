#include "core/quota.h"
#include "test.h"
#include <errno.h>
#include <limits.h>

int main(void)
{
    quota_info q = { 1000, 500 }, z = { 1000, 0 }, real;

    /* quota_check */
    CHECK(quota_check(&q, 0, 0) == 1);
    CHECK(quota_check(&q, 500, 0) == 1);            /* впритык */
    CHECK(quota_check(&q, 501, 0) == 0);            /* на байт больше */
    CHECK(quota_check(&q, 400, 100) == 1);          /* с резервом впритык */
    CHECK(quota_check(&q, 401, 100) == 0);
    CHECK(quota_check(&q, 0, 500) == 1);
    CHECK(quota_check(&q, 0, 501) == 0);            /* резерв больше свободного */
    CHECK(quota_check(&z, 0, 0) == 1);
    CHECK(quota_check(&z, 1, 0) == 0);
    CHECK(quota_check(&q, ULLONG_MAX, 0) == 0);     /* без переполнения */
    CHECK(quota_check(&q, 1, ULLONG_MAX) == 0);
    CHECK(quota_check(NULL, 1, 0) == 0);

    /* quota_get на реальной ФС */
    CHECK(quota_get("/tmp", &real) == 0);
    CHECK(real.total > 0);
    CHECK(real.avail <= real.total);
    CHECK(quota_get("/nonexistent/zzz", &real) == -1);
    CHECK(quota_get(NULL, &real) == -1);
    CHECK(quota_get("/tmp", NULL) == -1);

    /* errno */
    CHECK(quota_is_full_errno(ENOSPC) == 1);
    CHECK(quota_is_full_errno(EDQUOT) == 1);
    CHECK(quota_is_full_errno(EACCES) == 0);
    CHECK(quota_is_full_errno(0) == 0);
    DONE();
}
