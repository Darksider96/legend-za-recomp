#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <atomic>
#include <memory>

namespace za::audio {

class AmbientAudioStream;

class AudioPlayback {
public:
    static AudioPlayback& Instance();

    bool Initialize(uint32_t sample_rate = 48000, uint16_t channels = 2);
    void Shutdown();

    // Reproduz um buffer PCM linear (16-bit stereo interleaved)
    void PlayPcm(const int16_t* pcm_data, size_t sample_count, float volume = 1.0f);

    // Define um stream contínuo de ambiência ou BGM em loop
    void SetAmbientStream(std::shared_ptr<AmbientAudioStream> stream);
    std::shared_ptr<AmbientAudioStream> GetAmbientStream() const;
    void StopAmbientStream();

    // Reproduz um som procedural agradável de feedback sonoro (Chime do Pokémon Z-A)
    void PlayPokeChime(float freq = 523.25f /* C5 */, float duration_seconds = 0.35f, float volume = 0.5f);

    // Reproduz tom de confirmação de mira / trava de câmera
    void PlayLockonBeep(float freq = 880.0f /* A5 */, float duration_seconds = 0.12f, float volume = 0.4f);

    // Reproduz efeito sonoro de ataque e impacto de combate (whoosh + impacto + ressonância)
    void PlayAttackImpact(float power = 1.0f, float volume = 0.55f);

    void SetMasterVolume(float volume); // 0.0f a 1.0f
    float GetMasterVolume() const;

    bool IsActive() const { return active_; }

private:
    AudioPlayback();
    ~AudioPlayback();

    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::atomic<bool> active_{false};
    std::atomic<float> master_volume_{0.7f};
};

} // namespace za::audio
