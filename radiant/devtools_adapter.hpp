#pragma once

struct UiContext;
struct RadiantDevtools;

struct RadiantDevtoolsHostOps {
    RadiantDevtools* (*open)(UiContext* ui, int fd, const char* source_path);
    bool (*tick)(RadiantDevtools* inspector);
    void (*destroy)(RadiantDevtools* inspector);
};
bool radiant_devtools_host_register(const RadiantDevtoolsHostOps* operations);
RadiantDevtools* radiant_devtools_open(UiContext* ui, int fd, const char* source_path);
bool radiant_devtools_tick(RadiantDevtools* inspector);
void radiant_devtools_destroy(RadiantDevtools* inspector);
int view_doc_with_devtools(const char* path, int fd, bool headless);
