#pragma once
#include "audio/wwise.hpp"
#include "audio/wem_decoder.hpp"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>
#include <cstdint>

namespace za::audio {

class AmbientAudioStream {
public:
    AmbientAudioStream() = default;
    ~AmbientAudioStream() = default;

    // Abre uma mídia PTADPCM/PCM de streaming a partir de um WwisePackage (.pck)
    bool Open(const WwisePackage& pck, uint32_t media_id, std::string& error);

    // Abre diretamente a partir de bytes WEM em memória
    bool OpenFromMemory(std::vector<uint8_t> wem_bytes, std::string& error);

    // Obtém o próximo bloco de áudio descompactado (amostras 16-bit interleaved estéreo a 48 kHz)
    // Se loop == true, reinicia do início da faixa automaticamente
    bool GetNextBuffer(std::vector<int16_t>& out_pcm, size_t frame_count, bool loop = true);

    void Reset();
    void SetVolume(float volume) { volume_ = std::clamp(volume, 0.0f, 1.0f); }
    float GetVolume() const { return volume_; }

    bool IsValid() const { return valid_; }
    uint32_t GetMediaId() const { return media_id_; }
    uint32_t GetSampleRate() const { return info_.sample_rate; }
    uint16_t GetChannels() const { return info_.channels; }
    float GetDurationSeconds() const { return duration_seconds_; }

private:
    uint32_t media_id_{0};
    WemInfo info_{};
    std::vector<uint8_t> raw_adpcm_data_;
    size_t data_cursor_{0};
    float volume_{0.45f};
    float duration_seconds_{0.0f};
    bool valid_{false};
    mutable std::mutex mutex_;
};

} // namespace za::audio
