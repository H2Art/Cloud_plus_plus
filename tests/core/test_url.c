#include "core/url.h"
#include "test.h"
#include <string.h>

int main(void)
{
    char b[64], c[64];

    /* encode */
    CHECK(url_encode("a/b.txt", b, sizeof b) == 0 && !strcmp(b, "a/b.txt"));
    CHECK(url_encode("A file.txt", b, sizeof b) == 0 && !strcmp(b, "A%20file.txt"));
    CHECK(url_encode("q\"<>&'.txt", b, sizeof b) == 0 &&
          !strcmp(b, "q%22%3C%3E%26%27.txt"));
    CHECK(url_encode("\xd0\xba", b, sizeof b) == 0 && !strcmp(b, "%D0%BA"));
    CHECK(url_encode("", b, sizeof b) == 0 && !strcmp(b, ""));
    CHECK(url_encode("%", b, sizeof b) == 0 && !strcmp(b, "%25"));
    CHECK(url_encode(" ", b, 4) == 0 && !strcmp(b, "%20"));
    CHECK(url_encode(" ", b, 3) == -1 && strlen(b) < 3);
    CHECK(url_encode("abc", b, 3) == -1);
    CHECK(url_encode("abc", b, 0) == -1);
    CHECK(url_encode(NULL, b, sizeof b) == -1);

    /* decode */
    CHECK(url_decode("a%20b", b, sizeof b) == 0 && !strcmp(b, "a b"));
    CHECK(url_decode("%D0%ba", b, sizeof b) == 0 && !strcmp(b, "\xd0\xba"));
    CHECK(url_decode("a+b", b, sizeof b) == 0 && !strcmp(b, "a+b"));
    CHECK(url_decode("%2e%2e", b, sizeof b) == 0 && !strcmp(b, ".."));
    CHECK(url_decode("%252e", b, sizeof b) == 0 && !strcmp(b, "%2e")); /* один раз */
    CHECK(url_decode("%zz", b, sizeof b) == -1);
    CHECK(url_decode("%4", b, sizeof b) == -1);
    CHECK(url_decode("%", b, sizeof b) == -1);
    CHECK(url_decode("a%00b", b, sizeof b) == -1);
    CHECK(url_decode("abc", b, 3) == -1);
    CHECK(url_decode("abc", b, 4) == 0);
    CHECK(url_decode("", b, 0) == -1);

    /* круговой проход */
    {
        const char *s = "dir one/\xd0\xba\xd0\xb8\xd1\x80 \"q\"&%.txt";
        CHECK(url_encode(s, b, sizeof b) == 0);
        CHECK(url_decode(b, c, sizeof c) == 0 && !strcmp(c, s));
    }
    /* form-декодирование */
    CHECK(url_decode_form("a+b%20c", b, sizeof b) == 0 && !strcmp(b, "a b c"));
    CHECK(url_decode_form("a%2Bb", b, sizeof b) == 0 && !strcmp(b, "a+b"));
    CHECK(url_decode_form("%zz", b, sizeof b) == -1);
    CHECK(url_decode_form("%00", b, sizeof b) == -1);
    DONE();
}
