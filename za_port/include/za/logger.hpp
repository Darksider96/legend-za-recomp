#pragma once
#include <string>
#include <string_view>
#include <cstdint>

namespace za {

enum class LogLevel : uint8_t {
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3,
    Fatal = 4,
    Perf = 5
};

class Logger {
public:
    static Logger& Instance();

    void Initialize(const std::string& log_filepath = "za_engine.log");
    void Shutdown();

    void Log(LogLevel level, const char* tag, const char* format, ...);
    void Flush();

    void SetMinLevel(LogLevel level) { min_level_ = level; }
    LogLevel GetMinLevel() const { return min_level_; }

private:
    Logger() = default;
    ~Logger();

    FILE* log_file_{nullptr};
    LogLevel min_level_{LogLevel::Info};
    bool initialized_{false};
};

} // namespace za

#define ZA_LOG_DEBUG(tag, ...) ::za::Logger::Instance().Log(::za::LogLevel::Debug, tag, __VA_ARGS__)
#define ZA_LOG_INFO(tag, ...)  ::za::Logger::Instance().Log(::za::LogLevel::Info, tag, __VA_ARGS__)
#define ZA_LOG_WARN(tag, ...)  ::za::Logger::Instance().Log(::za::LogLevel::Warn, tag, __VA_ARGS__)
#define ZA_LOG_ERROR(tag, ...) ::za::Logger::Instance().Log(::za::LogLevel::Error, tag, __VA_ARGS__)
#define ZA_LOG_FATAL(tag, ...) ::za::Logger::Instance().Log(::za::LogLevel::Fatal, tag, __VA_ARGS__)
#define ZA_LOG_PERF(tag, ...)  ::za::Logger::Instance().Log(::za::LogLevel::Perf, tag, __VA_ARGS__)
