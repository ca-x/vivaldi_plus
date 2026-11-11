#pragma once
#include <string>
struct LifecycleOptions {
    bool enabled = false;
    std::wstring startup;
    std::wstring exit;
};
void InitializeLifecycle(const LifecycleOptions& options);
