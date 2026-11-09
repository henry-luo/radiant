#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

// Resolve owner-held fixtures for standalone and Lambda integration runners.
inline bool radiant_test_fixture_path(const char* name, char* out, size_t size) {
    struct stat info;
    if (stat("test/fixtures", &info) == 0 && S_ISDIR(info.st_mode))
        return snprintf(out, size, "test/fixtures/%s", name) < (int)size;
    const char* owner = getenv("RADIANT_ROOT");
    return snprintf(out, size, "%s/test/fixtures/%s", owner ? owner : "../radiant", name) < (int)size;
}
