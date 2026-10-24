#include "nn/time.hpp"
#include <iostream>
#include <chrono>

namespace nn::time {

static bool g_initialized = false;

nn::Result Initialize() {
    std::cout << "[nn::time] Initialize() -> High-Resolution Clock Ready" << std::endl;
    g_initialized = true;
    return nn::ResultSuccess;
}

bool IsInitialized() {
    return g_initialized;
}

} // namespace nn::time
