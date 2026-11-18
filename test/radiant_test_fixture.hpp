#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

// Resolve owner-held fixtures for standalone and Lambda integration runners.
inline bool radiant_test_fixture_path(const char* name, char* out, size_t size) {
    struct stat info;
    const char* owner = getenv("RADIANT_ROOT");
    // support standalone, configured, and nested or sibling integration checkouts.
    const char* roots[] = {owner, ".", "radiant", "../radiant"};
    for (const char* root : roots) {
        if (!root || !*root) continue;
        int written = snprintf(out, size, "%s/test/fixtures/%s", root, name);
        if (written >= 0 && (size_t)written < size &&
            stat(out, &info) == 0 && !S_ISDIR(info.st_mode)) return true;
    }
    return false;
}
