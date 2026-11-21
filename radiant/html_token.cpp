#include "network_integration.h"
#include "lib/str.h"
#include <string.h>

bool radiant_html_has_token(const char* tokens, const char* token) {
    if (!tokens || !token || !*token) return false;
    size_t length = strlen(token);
    for (const char* p = tokens; *p;) {
        while (str_is_html_space(*p)) p++;
        const char* start = p;
        while (*p && !str_is_html_space(*p)) p++;
        if (str_ieq(start, (size_t)(p - start), token, length)) return true;
    }
    return false;
}

