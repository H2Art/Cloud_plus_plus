#include "core/html.h"
#include "test.h"
#include <string.h>

int main(void)
{
    char b[64];

    CHECK(html_escape("plain.txt", b, sizeof b) == 0 && !strcmp(b, "plain.txt"));
    CHECK(html_escape("", b, sizeof b) == 0 && !strcmp(b, ""));
    CHECK(html_escape("<script>", b, sizeof b) == 0 && !strcmp(b, "&lt;script&gt;"));
    CHECK(html_escape("a&b", b, sizeof b) == 0 && !strcmp(b, "a&amp;b"));
    CHECK(html_escape("\"x\" 'y'", b, sizeof b) == 0 &&
          !strcmp(b, "&quot;x&quot; &#39;y&#39;"));
    CHECK(html_escape("\xd0\xb4\xd0\xbe\xd0\xba", b, sizeof b) == 0 &&
          !strcmp(b, "\xd0\xb4\xd0\xbe\xd0\xba")); /* UTF-8 не трогаем */

    /* усечение: "&lt;" = 4 символа + \0 = 5 байт */
    CHECK(html_escape("<", b, 5) == 0 && !strcmp(b, "&lt;"));
    CHECK(html_escape("<", b, 4) == -1 && b[0] == '\0');
    CHECK(html_escape("abc", b, 3) == -1 && strlen(b) < 3);
    CHECK(html_escape("abc", b, 0) == -1);
    CHECK(html_escape(NULL, b, sizeof b) == -1);
    DONE();
}