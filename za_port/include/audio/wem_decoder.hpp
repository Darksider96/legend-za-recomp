#pragma once
// Decodificacao de .wem para PCM 16 bits, reamostragem linear e montagem de WAV
//
// Codecs suportados:
//   0x0001 / 0xFFFE PCM linear 16 bits (copiado do chunk "data")
//   0x8311 PTADPCM (Platinum ADPCM, padrao do Wwise 2019+): quadros de block_align / canais bytes por canal,
//     intercalados por quadro; cada quadro = s16 hist2, s16 hist1, u8 indice + nibbles (baixo primeiro),
//     2 + (tamanho - 5) * 2 amostras. Todos os 235 cries do Z-A sao PTADPCM mono com quadros de 0x24 bytes.
// Vorbis (0xFFFF), Opus e IMA ADPCM (0x0002, ausente no Z-A) nao sao decodificados.

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace za::audio {

struct PcmBuffer {
    uint32_t sample_rate{0};
    uint16_t channels{0};
    std::vector<int16_t> samples;   // intercaladas (L R L R ...)

    size_t FrameCount() const { return channels ? samples.size() / channels : 0; }
    float DurationSeconds() const { return sample_rate ? static_cast<float>(FrameCount()) / static_cast<float>(sample_rate) : 0.0f; }
};

// Decodifica quadros PTADPCM (frame_size bytes por canal, intercalados) em amostras intercaladas
bool DecodePtadpcm(std::span<const uint8_t> data, uint16_t channels, uint32_t frame_size, std::vector<int16_t>& out,
                   std::string& error);

bool DecodeWem(std::span<const uint8_t> wem, PcmBuffer& out, std::string& error);

// Interpolacao linear para sample_rate/channels (mono -> estereo duplica, estereo -> mono faz a media)
PcmBuffer ResampleLinear(const PcmBuffer& input, uint32_t sample_rate, uint16_t channels);

// WAV PCM 16 bits (RIFF/WAVE, fmt de 16 bytes + data); o payload de samples serve direto ao
// AudioPlayback::PlayPcm quando o buffer esta em 48 kHz estereo
std::vector<uint8_t> BuildWav(const PcmBuffer& pcm);

} // namespace za::audio
