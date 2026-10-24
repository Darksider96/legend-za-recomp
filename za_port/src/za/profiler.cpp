#include "za/profiler.hpp"
#include "za/logger.hpp"
#include <algorithm>
#include <numeric>
#include <cstdio>
#include <cmath>

namespace za {

Profiler& Profiler::Instance() {
    static Profiler instance;
    return instance;
}

void Profiler::Initialize() {
    frame_times_ms_.clear();
    frame_times_ms_.reserve(50000);
    stages_.clear();
    total_frames_ = 0;
    stutter_count_25ms_ = 0;
    stutter_count_33ms_ = 0;
    total_time_ms_ = 0.0;

    ZA_LOG_INFO("PROFILER", "Sistema de Telemetria e Profiling de Performance inicializado.");
}

void Profiler::Shutdown() {
    DumpReport("za_performance.log");
}

void Profiler::BeginFrame() {
    frame_start_ = std::chrono::high_resolution_clock::now();
}

void Profiler::EndFrame() {
    auto frame_end = std::chrono::high_resolution_clock::now();
    float dt_ms = std::chrono::duration<float, std::milli>(frame_end - frame_start_).count();

    total_frames_++;
    total_time_ms_ += dt_ms;
    frame_times_ms_.push_back(dt_ms);

    if (dt_ms > 25.0f) stutter_count_25ms_++;
    if (dt_ms > 33.3f) stutter_count_33ms_++;
}

void Profiler::BeginStage(const char* name) {
    current_stage_name_ = name;
    stage_start_ = std::chrono::high_resolution_clock::now();
}

void Profiler::EndStage(const char* name) {
    auto stage_end = std::chrono::high_resolution_clock::now();
    uint64_t dur_us = std::chrono::duration_cast<std::chrono::microseconds>(stage_end - stage_start_).count();

    for (auto& s : stages_) {
        if (s.name == name || std::string(s.name) == name) {
            s.current_us = dur_us;
            s.total_us += dur_us;
            if (dur_us > s.max_us) s.max_us = dur_us;
            return;
        }
    }
    stages_.push_back(StageTiming{name, dur_us, dur_us, dur_us});
}

float Profiler::GetCurrentFps() const {
    if (frame_times_ms_.empty()) return 60.0f;
    float last_dt = frame_times_ms_.back();
    return (last_dt > 0.001f) ? (1000.0f / last_dt) : 60.0f;
}

float Profiler::GetAverageFps() const {
    if (total_time_ms_ <= 0.0 || total_frames_ == 0) return 60.0f;
    return static_cast<float>(total_frames_ * 1000.0 / total_time_ms_);
}

float Profiler::GetAverageFrametimeMs() const {
    if (total_frames_ == 0) return 16.66f;
    return static_cast<float>(total_time_ms_ / total_frames_);
}

float Profiler::Get1PercentLowFps() const {
    if (frame_times_ms_.size() < 100) return GetAverageFps();
    std::vector<float> sorted = frame_times_ms_;
    std::sort(sorted.begin(), sorted.end());

    // 1% piores tempos (99º percentil)
    size_t idx = static_cast<size_t>(sorted.size() * 0.99f);
    if (idx >= sorted.size()) idx = sorted.size() - 1;
    float dt = sorted[idx];
    return (dt > 0.001f) ? (1000.0f / dt) : 30.0f;
}

float Profiler::Get01PercentLowFps() const {
    if (frame_times_ms_.size() < 1000) return Get1PercentLowFps();
    std::vector<float> sorted = frame_times_ms_;
    std::sort(sorted.begin(), sorted.end());

    // 0.1% piores tempos (99.9º percentil)
    size_t idx = static_cast<size_t>(sorted.size() * 0.999f);
    if (idx >= sorted.size()) idx = sorted.size() - 1;
    float dt = sorted[idx];
    return (dt > 0.001f) ? (1000.0f / dt) : 20.0f;
}

void Profiler::DumpReport(const std::string& filepath) {
    FILE* f = std::fopen(filepath.c_str(), "w");
    if (!f) return;

    std::fprintf(f, "===============================================================================\n");
    std::fprintf(f, "  RELATORIO DE PERFORMANCE E TELEMETRIA — POKEMON LEGENDS: Z-A (x86_64)       \n");
    std::fprintf(f, "===============================================================================\n\n");

    std::fprintf(f, "Total de Quadros Executados : %llu\n", (unsigned long long)total_frames_);
    std::fprintf(f, "Tempo Total de Sessao       : %.2f segundos\n", total_time_ms_ / 1000.0);
    std::fprintf(f, "Taxa de Quadros Media (FPS) : %.2f FPS\n", GetAverageFps());
    std::fprintf(f, "Frametime Medio             : %.3f ms (Alvo 60 FPS: 16.666 ms)\n", GetAverageFrametimeMs());
    std::fprintf(f, "1%% Low FPS                  : %.2f FPS\n", Get1PercentLowFps());
    std::fprintf(f, "0.1%% Low FPS                : %.2f FPS\n", Get01PercentLowFps());
    std::fprintf(f, "Quadros Acima de 25 ms      : %llu (Stutters leves)\n", (unsigned long long)stutter_count_25ms_);
    std::fprintf(f, "Quadros Acima de 33 ms      : %llu (Quedas abaixo de 30 FPS)\n\n", (unsigned long long)stutter_count_33ms_);

    std::fprintf(f, "[DECOMPOSICAO DE TEMPO POR SUBSISTEMA / ESTAGIO]\n");
    std::fprintf(f, "%-28s | %-12s | %-12s | %-10s\n", "Nome do Estagio", "Tempo Medio", "Pico Maximo", "%% do Total");
    std::fprintf(f, "-------------------------------------------------------------------------------\n");

    for (const auto& s : stages_) {
        double avg_ms = (total_frames_ > 0) ? (static_cast<double>(s.total_us) / total_frames_) / 1000.0 : 0.0;
        double max_ms = s.max_us / 1000.0;
        double pct = (total_time_ms_ > 0.0) ? (avg_ms * total_frames_ / total_time_ms_) * 100.0 : 0.0;

        std::fprintf(f, "%-28s | %8.3f ms  | %8.3f ms  | %6.1f%%\n",
                      s.name, avg_ms, max_ms, pct);
    }

    std::fprintf(f, "\n============================ FIM DO RELATORIO ============================\n");
    std::fclose(f);

    ZA_LOG_INFO("PROFILER", "Relatorio de performance gravado em: %s (FPS Medio: %.1f)", filepath.c_str(), GetAverageFps());
}

} // namespace za
