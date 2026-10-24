#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

namespace za {

struct StageTiming {
    const char* name;
    uint64_t total_us{0};
    uint64_t max_us{0};
    uint64_t current_us{0};
};

class Profiler {
public:
    static Profiler& Instance();

    void Initialize();
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    void BeginStage(const char* name);
    void EndStage(const char* name);

    float GetCurrentFps() const;
    float GetAverageFps() const;
    float Get1PercentLowFps() const;
    float Get01PercentLowFps() const;
    float GetAverageFrametimeMs() const;

    void DumpReport(const std::string& filepath = "za_performance.log");

private:
    Profiler() = default;

    std::chrono::high_resolution_clock::time_point frame_start_;
    std::chrono::high_resolution_clock::time_point stage_start_;
    const char* current_stage_name_{nullptr};

    std::vector<float> frame_times_ms_;
    std::vector<StageTiming> stages_;

    uint64_t total_frames_{0};
    uint64_t stutter_count_25ms_{0};
    uint64_t stutter_count_33ms_{0};
    double total_time_ms_{0.0};
};

struct ScopedStageProfile {
    const char* name;
    explicit ScopedStageProfile(const char* n) : name(n) {
        Profiler::Instance().BeginStage(name);
    }
    ~ScopedStageProfile() {
        Profiler::Instance().EndStage(name);
    }
};

} // namespace za

#define ZA_PROFILE_STAGE(name) ::za::ScopedStageProfile _prof_stage_##__LINE__(name)
