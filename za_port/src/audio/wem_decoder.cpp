#include "audio/wem_decoder.hpp"

#include "audio/wwise.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace za::audio {

namespace {

// Tabela [indice][nibble] = {passo, proximo indice} do PTADPCM, conforme vgmstream (src/coding/ptadpcm_decoder.c,
// licenca ISC), que a obteve por engenharia reversa dos executaveis do Wwise. A linha 12 (zeros) atende
// cabecalhos com indice > 11; os passos nunca levam a um indice acima de 11.
constexpr int16_t kPtadpcmTable[13][16][2] = {
    {{-14, 2}, {-10, 2}, {-7, 1}, {-5, 1}, {-3, 0}, {-2, 0}, {-1, 0}, {0, 0},
     {0, 0}, {1, 0}, {2, 0}, {3, 0}, {5, 1}, {7, 1}, {10, 2}, {14, 2}},
    {{-28, 3}, {-20, 3}, {-14, 2}, {-10, 2}, {-7, 1}, {-5, 1}, {-3, 1}, {-1, 0},
     {1, 0}, {3, 1}, {5, 1}, {7, 1}, {10, 2}, {14, 2}, {20, 3}, {28, 3}},
    {{-56, 4}, {-40, 4}, {-28, 3}, {-20, 3}, {-14, 2}, {-10, 2}, {-6, 2}, {-2, 1},
     {2, 1}, {6, 2}, {10, 2}, {14, 2}, {20, 3}, {28, 3}, {40, 4}, {56, 4}},
    {{-112, 5}, {-80, 5}, {-56, 4}, {-40, 4}, {-28, 3}, {-20, 3}, {-12, 3}, {-4, 2},
     {4, 2}, {12, 3}, {20, 3}, {28, 3}, {40, 4}, {56, 4}, {80, 5}, {112, 5}},
    {{-224, 6}, {-160, 6}, {-112, 5}, {-80, 5}, {-56, 4}, {-40, 4}, {-24, 4}, {-8, 3},
     {8, 3}, {24, 4}, {40, 4}, {56, 4}, {80, 5}, {112, 5}, {160, 6}, {224, 6}},
    {{-448, 7}, {-320, 7}, {-224, 6}, {-160, 6}, {-112, 5}, {-80, 5}, {-48, 5}, {-16, 4},
     {16, 4}, {48, 5}, {80, 5}, {112, 5}, {160, 6}, {224, 6}, {320, 7}, {448, 7}},
    {{-896, 8}, {-640, 8}, {-448, 7}, {-320, 7}, {-224, 6}, {-160, 6}, {-96, 6}, {-32, 5},
     {32, 5}, {96, 6}, {160, 6}, {224, 6}, {320, 7}, {448, 7}, {640, 8}, {896, 8}},
    {{-1792, 9}, {-1280, 9}, {-896, 8}, {-640, 8}, {-448, 7}, {-320, 7}, {-192, 7}, {-64, 6},
     {64, 6}, {192, 7}, {320, 7}, {448, 7}, {640, 8}, {896, 8}, {1280, 9}, {1792, 9}},
    {{-3584, 10}, {-2560, 10}, {-1792, 9}, {-1280, 9}, {-896, 8}, {-640, 8}, {-384, 8}, {-128, 7},
     {128, 7}, {384, 8}, {640, 8}, {896, 8}, {1280, 9}, {1792, 9}, {2560, 10}, {3584, 10}},
    {{-7168, 11}, {-5120, 11}, {-3584, 10}, {-2560, 10}, {-1792, 9}, {-1280, 9}, {-768, 9}, {-256, 8},
     {256, 8}, {768, 9}, {1280, 9}, {1792, 9}, {2560, 10}, {3584, 10}, {5120, 11}, {7168, 11}},
    {{-14336, 11}, {-10240, 11}, {-7168, 11}, {-5120, 11}, {-3584, 10}, {-2560, 10}, {-1536, 10}, {-512, 9},
     {512, 9}, {1536, 10}, {2560, 10}, {3584, 10}, {5120, 11}, {7168, 11}, {10240, 11}, {14336, 11}},
    {{-28672, 11}, {-20480, 11}, {-14336, 11}, {-10240, 11}, {-7168, 11}, {-5120, 11}, {-3072, 11}, {-1024, 10},
     {1024, 10}, {3072, 11}, {5120, 11}, {7168, 11}, {10240, 11}, {14336, 11}, {20480, 11}, {28672, 11}},
    {},
};

int16_t Clamp16(int32_t value) {
    return static_cast<int16_t>(std::clamp<int32_t>(value, -32768, 32767));
}

void Put16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value));
    out.push_back(static_cast<uint8_t>(value >> 8));
}

void Put32(std::vector<uint8_t>& out, uint32_t value) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(value >> (8 * i)));
}

} // namespace

bool DecodePtadpcm(std::span<const uint8_t> data, uint16_t channels, uint32_t frame_size, std::vector<int16_t>& out,
                   std::string& error) {
    out.clear();
    if (channels == 0 || frame_size < 6) {
        error = "PTADPCM com canais/quadro invalidos";
        return false;
    }
    const size_t samples_per_frame = 2 + (size_t{frame_size} - 5) * 2;
    const size_t block = size_t{frame_size} * channels;
    const size_t blocks = data.size() / block;
    out.resize(blocks * samples_per_frame * channels);

    for (size_t b = 0; b < blocks; ++b) {
        for (uint16_t c = 0; c < channels; ++c) {
            const uint8_t* frame = data.data() + b * block + size_t{c} * frame_size;
            int16_t* dst = out.data() + b * samples_per_frame * channels + c;
            int32_t hist2 = static_cast<int16_t>(frame[0] | (frame[1] << 8));
            int32_t hist1 = static_cast<int16_t>(frame[2] | (frame[3] << 8));
            int index = std::min<int>(frame[4], 12);
            dst[0] = static_cast<int16_t>(hist2);
            dst[channels] = static_cast<int16_t>(hist1);
            for (size_t i = 0; i < samples_per_frame - 2; ++i) {
                const uint8_t byte = frame[5 + i / 2];
                const int nibble = (i & 1) ? (byte >> 4) : (byte & 0x0F);
                const int32_t step = kPtadpcmTable[index][nibble][0];
                index = kPtadpcmTable[index][nibble][1];
                const int16_t sample = Clamp16(step + 2 * hist1 - hist2);
                dst[(i + 2) * channels] = sample;
                hist2 = hist1;
                hist1 = sample;
            }
        }
    }
    return true;
}

bool DecodeWem(std::span<const uint8_t> wem, PcmBuffer& out, std::string& error) {
    out = {};
    WemInfo info;
    if (!ParseWemInfo(wem, info) || info.data_offset == 0) {
        error = "WEM sem cabecalho RIFF/WAVE ou chunk data";
        return false;
    }
    if (info.channels == 0 || info.sample_rate == 0) {
        error = "WEM com canais/taxa invalidos";
        return false;
    }
    const size_t available = wem.size() - std::min<size_t>(wem.size(), info.data_offset);
    const std::span<const uint8_t> payload = wem.subspan(info.data_offset, std::min<size_t>(info.data_size, available));
    out.sample_rate = info.sample_rate;
    out.channels = info.channels;

    if (IsPcmCodec(info.codec)) {
        if (info.bits_per_sample != 16) {
            error = "PCM de " + std::to_string(info.bits_per_sample) + " bits nao suportado";
            return false;
        }
        out.samples.resize(payload.size() / 2);
        if (!out.samples.empty()) std::memcpy(out.samples.data(), payload.data(), out.samples.size() * 2);
        return true;
    }
    if (info.codec == 0x8311) {
        if (info.bits_per_sample != 4 || info.block_align % info.channels != 0) {
            error = "PTADPCM com fmt inesperado (bits " + std::to_string(info.bits_per_sample) + ", block_align " +
                    std::to_string(info.block_align) + ")";
            return false;
        }
        return DecodePtadpcm(payload, info.channels, info.block_align / info.channels, out.samples, error);
    }
    error = "codec " + std::string(WemCodecName(info.codec)) + " nao suportado pelo decodificador";
    return false;
}

PcmBuffer ResampleLinear(const PcmBuffer& input, uint32_t sample_rate, uint16_t channels) {
    PcmBuffer out;
    out.sample_rate = sample_rate;
    out.channels = channels;
    const size_t in_frames = input.FrameCount();
    if (in_frames == 0 || input.sample_rate == 0 || sample_rate == 0 || channels == 0) return out;

    // Amostra do canal c do quadro f, com o mapeamento de canais pedido
    const auto source = [&](size_t frame, uint16_t c) -> float {
        const int16_t* row = input.samples.data() + frame * input.channels;
        if (channels == input.channels) return row[c];
        if (input.channels == 1) return row[0];
        if (channels == 1) {
            float sum = 0;
            for (uint16_t k = 0; k < input.channels; ++k) sum += row[k];
            return sum / static_cast<float>(input.channels);
        }
        return row[std::min<uint16_t>(c, static_cast<uint16_t>(input.channels - 1))];
    };

    const size_t out_frames = static_cast<size_t>((uint64_t{in_frames} * sample_rate + input.sample_rate - 1) / input.sample_rate);
    out.samples.resize(out_frames * channels);
    const double ratio = static_cast<double>(input.sample_rate) / sample_rate;
    for (size_t f = 0; f < out_frames; ++f) {
        const double position = static_cast<double>(f) * ratio;
        const size_t i0 = std::min(static_cast<size_t>(position), in_frames - 1);
        const size_t i1 = std::min(i0 + 1, in_frames - 1);
        const float t = static_cast<float>(position - static_cast<double>(i0));
        for (uint16_t c = 0; c < channels; ++c) {
            const float value = source(i0, c) + (source(i1, c) - source(i0, c)) * t;
            out.samples[f * channels + c] = Clamp16(static_cast<int32_t>(std::lround(value)));
        }
    }
    return out;
}

std::vector<uint8_t> BuildWav(const PcmBuffer& pcm) {
    const uint32_t data_size = static_cast<uint32_t>(pcm.samples.size() * 2);
    std::vector<uint8_t> wav;
    wav.reserve(44 + data_size);
    wav.insert(wav.end(), {'R', 'I', 'F', 'F'});
    Put32(wav, 36 + data_size);
    wav.insert(wav.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    Put32(wav, 16);
    Put16(wav, 1);
    Put16(wav, pcm.channels);
    Put32(wav, pcm.sample_rate);
    Put32(wav, pcm.sample_rate * pcm.channels * 2);
    Put16(wav, static_cast<uint16_t>(pcm.channels * 2));
    Put16(wav, 16);
    wav.insert(wav.end(), {'d', 'a', 't', 'a'});
    Put32(wav, data_size);
    const size_t header = wav.size();
    wav.resize(header + data_size);
    if (data_size) std::memcpy(wav.data() + header, pcm.samples.data(), data_size);
    return wav;
}

} // namespace za::audio
