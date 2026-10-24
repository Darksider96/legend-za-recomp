#include "za/logger.hpp"
#include <cstdio>
#include <cstdarg>
#include <chrono>
#include <ctime>
#include <mutex>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace za {

static std::mutex g_log_mutex;

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    Shutdown();
}

void Logger::Initialize(const std::string& log_filepath) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (initialized_) return;

    log_file_ = std::fopen(log_filepath.c_str(), "w");
    initialized_ = true;

    if (log_file_) {
        std::fprintf(log_file_, "===============================================================================\n");
        std::fprintf(log_file_, "  POKEMON LEGENDS: Z-A - LOG DO MOTOR E SUBSISTEMAS (Trinity Engine x86_64)\n");
        std::fprintf(log_file_, "===============================================================================\n\n");
        std::fflush(log_file_);
    }
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (log_file_) {
        std::fprintf(log_file_, "\n[LOG] Finalizacao normal do arquivo de log do motor.\n");
        std::fclose(log_file_);
        log_file_ = nullptr;
    }
    initialized_ = false;
}

void Logger::Flush() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (log_file_) std::fflush(log_file_);
    std::fflush(stdout);
    std::fflush(stderr);
}

void Logger::Log(LogLevel level, const char* tag, const char* format, ...) {
    if (level < min_level_) return;

    // Timestamp de alta resolução
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &now_c);
#else
    localtime_r(&now_c, &tm_buf);
#endif

    char time_str[32];
    std::snprintf(time_str, sizeof(time_str), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                  tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, (int)ms.count());

    const char* lvl_str = "INFO";
    switch (level) {
        case LogLevel::Debug: lvl_str = "DEBUG"; break;
        case LogLevel::Info:  lvl_str = "INFO";  break;
        case LogLevel::Warn:  lvl_str = "WARN";  break;
        case LogLevel::Error: lvl_str = "ERROR"; break;
        case LogLevel::Fatal: lvl_str = "FATAL"; break;
        case LogLevel::Perf:  lvl_str = "PERF";  break;
    }

    char msg_buf[2048];
    va_list args;
    va_start(args, format);
    std::vsnprintf(msg_buf, sizeof(msg_buf), format, args);
    va_end(args);

    std::lock_guard<std::mutex> lock(g_log_mutex);

    // Formatação de saída: [TIMESTAMP] [LEVEL] [TAG] Mensagem
    char full_line[2304];
    std::snprintf(full_line, sizeof(full_line), "[%s] [%-5s] [%-12s] %s\n",
                  time_str, lvl_str, tag ? tag : "ENGINE", msg_buf);

    // 1. Saída de console
    if (level >= LogLevel::Error) {
        std::fputs(full_line, stderr);
    } else {
        std::fputs(full_line, stdout);
    }

    // 2. Gravação em disco
    if (log_file_) {
        std::fputs(full_line, log_file_);
        if (level >= LogLevel::Error) {
            std::fflush(log_file_);
        }
    }
}

} // namespace za
