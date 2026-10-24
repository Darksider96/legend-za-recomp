#include "nn/diag.h"
#include "za/logger.hpp"
#include "za/crash_handler.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace nn::diag {

namespace detail {

[[noreturn]] void AbortImpl(const nn::Result* result) {
    char err_buf[128];
    if (result) {
        std::snprintf(err_buf, sizeof(err_buf), "nn::Result error: 0x%08X", result->GetInnerValueForDebug());
    } else {
        std::snprintf(err_buf, sizeof(err_buf), "Unknown nn::Result error");
    }
    ZA_LOG_FATAL("DIAG", "%s", err_buf);
    za::CrashHandler::SetLastCheckpoint(err_buf);
    za::CrashHandler::WriteCrashDump(nullptr);
    std::abort();
}

[[noreturn]] void AbortImpl(const char* message) {
    ZA_LOG_FATAL("DIAG", "Abort: %s", message ? message : "(null)");
    za::CrashHandler::SetLastCheckpoint(message ? message : "Abort");
    za::CrashHandler::WriteCrashDump(nullptr);
    std::abort();
}

} // namespace detail

void Log(const char* format, ...) {
    char buf[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    // Remove newline no final para formatar limpo com o Logger
    size_t len = std::strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';

    ZA_LOG_INFO("NN_SDK", "%s", buf);
}

} // namespace nn::diag
