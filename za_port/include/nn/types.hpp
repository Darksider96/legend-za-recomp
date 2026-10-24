#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

namespace nn {

struct Result {
    uint32_t value{0};

    constexpr Result() = default;
    constexpr Result(uint32_t val) : value(val) {}
    constexpr Result(int val) : value(static_cast<uint32_t>(val)) {}

    constexpr operator uint32_t() const noexcept { return value; }

    [[nodiscard]] constexpr bool IsSuccess() const noexcept {
        return value == 0;
    }

    [[nodiscard]] constexpr bool IsFailure() const noexcept {
        return value != 0;
    }

    [[nodiscard]] constexpr uint32_t GetInnerValueForDebug() const noexcept {
        return value;
    }

    constexpr bool operator==(const Result& other) const noexcept {
        return value == other.value;
    }

    constexpr bool operator!=(const Result& other) const noexcept {
        return value != other.value;
    }
};

inline constexpr Result ResultSuccess{0};

using ApplicationId = uint64_t;

namespace account {
    struct Uid {
        uint64_t data[2]{0, 0};

        constexpr bool operator==(const Uid& other) const noexcept {
            return data[0] == other.data[0] && data[1] == other.data[1];
        }
    };
} // namespace account

struct TimeSpan {
    int64_t nano_seconds{0};

    constexpr TimeSpan() = default;
    constexpr explicit TimeSpan(int64_t ns) : nano_seconds(ns) {}

    static constexpr TimeSpan FromNanoSeconds(int64_t ns) { return TimeSpan(ns); }
    static constexpr TimeSpan FromMicroSeconds(int64_t us) { return TimeSpan(us * 1000); }
    static constexpr TimeSpan FromMilliSeconds(int64_t ms) { return TimeSpan(ms * 1000 * 1000); }
    static constexpr TimeSpan FromSeconds(int64_t s) { return TimeSpan(s * 1000 * 1000 * 1000); }

    [[nodiscard]] constexpr int64_t GetNanoSeconds() const noexcept { return nano_seconds; }
    [[nodiscard]] constexpr int64_t GetMilliSeconds() const noexcept { return nano_seconds / (1000 * 1000); }
    [[nodiscard]] constexpr int64_t GetSeconds() const noexcept { return nano_seconds / (1000 * 1000 * 1000); }
};

} // namespace nn
