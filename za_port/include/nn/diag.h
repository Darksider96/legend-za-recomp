#pragma once

#include "nn/types.h"
#include <cstdio>
#include <cstdlib>

namespace nn::diag {

namespace detail {
    [[noreturn]] void AbortImpl(const nn::Result* result);
    [[noreturn]] void AbortImpl(const char* message);
} // namespace detail

void Log(const char* format, ...);

} // namespace nn::diag

#define NN_LOG(...) ::nn::diag::Log(__VA_ARGS__)
#define NN_ABORT(...) ::nn::diag::detail::AbortImpl(__VA_ARGS__)
#define NN_ASSERT(expr) do { if (!(expr)) { ::nn::diag::detail::AbortImpl("Assertion failed: " #expr); } } while(0)
