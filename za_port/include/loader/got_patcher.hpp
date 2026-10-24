#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

namespace za::loader {

struct GotEntry {
    uintptr_t plt_address;
    uintptr_t got_address;
    const char* symbol;
    const char* readable_name;
};

class GotPatcher {
public:
    static bool PatchAll(uintptr_t base_address);
    static void RegisterCustomHook(uintptr_t got_address, void* host_func);
    static void LogGuestCall(const char* symbol);
};

} // namespace za::loader
