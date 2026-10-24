#include "audio/ambient_stream.hpp"
#include <iostream>
#include <cmath>

namespace za::audio {

bool AmbientAudioStream::Open(const WwisePackage& pck, uint32_t media_id, std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    valid_ = false;
    media_id_ = media_id;

    const auto* media = pck.Find(media_id);
    if (!media) {
        error = "Midia " + std::to_string(media_id) + " nao encontrada no pacote Stream.pck";
        return false;
    }

    std::vector<uint8_t> wem_bytes;
    if (!pck.Read(*media, wem_bytes, error)) {
        return false;
    }

    return OpenFromMemory(std::move(wem_bytes), error);
}

bool AmbientAudioStream::OpenFromMemory(std::vector<uint8_t> wem_bytes, std::string& error) {
    if (!ParseWemInfo(wem_bytes, info_) || info_.data_offset == 0) {
        error = "Cabecalho RIFF/WAVE WEM invalido ou ausente";
        return false;
    }

    if (info_.codec != 0x8311 && info_.codec != 0x0001) {
        error = "Codec 0x" + std::to_string(info_.codec) + " (" + std::string(WemCodecName(info_.codec)) + 
                ") nao suportado para streaming em tempo real (requer PTADPCM ou PCM)";
        return false;
    }

    if (info_.channels == 0 || info_.sample_rate == 0) {
        error = "Canais ou taxa de amostragem invalidos";
        return false;
    }

    size_t payload_size = wem_bytes.size() - info_.data_offset;
    if (info_.data_size > 0 && info_.data_size < payload_size) {
        payload_size = info_.data_size;
    }

    raw_adpcm_data_.assign(wem_bytes.begin() + info_.data_offset,
                           wem_bytes.begin() + info_.data_offset + payload_size);
    data_cursor_ = 0;

    if (info_.codec == 0x8311) {
        // PTADPCM: cada bloco de (36 * canais) bytes contem 64 quadros
        size_t block_bytes = 36 * info_.channels;
        size_t total_frames = (raw_adpcm_data_.size() / block_bytes) * 64;
        duration_seconds_ = static_cast<float>(total_frames) / info_.sample_rate;
    } else {
        // PCM linear
        size_t bytes_per_frame = (info_.bits_per_sample / 8) * info_.channels;
        duration_seconds_ = static_cast<float>(raw_adpcm_data_.size() / bytes_per_frame) / info_.sample_rate;
    }

    valid_ = true;
    return true;
}

void AmbientAudioStream::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    data_cursor_ = 0;
}

bool AmbientAudioStream::GetNextBuffer(std::vector<int16_t>& out_pcm, size_t frame_count, bool loop) {
    std::lock_guard<std::mutex> lock(mutex_);
    out_pcm.clear();
    if (!valid_ || raw_adpcm_data_.empty() || frame_count == 0) return false;

    if (info_.codec == 0x8311) {
        // PTADPCM: cada bloco de (36 * canais) bytes gera 64 quadros
        const size_t block_bytes = 36 * info_.channels;
        const size_t frames_per_block = 64;

        size_t blocks_needed = (frame_count + frames_per_block - 1) / frames_per_block;
        std::vector<int16_t> decoded_pcm;

        while (blocks_needed > 0) {
            if (data_cursor_ + block_bytes > raw_adpcm_data_.size()) {
                if (!loop) break;
                data_cursor_ = 0; // Wrap around para o início
            }

            size_t bytes_remaining = raw_adpcm_data_.size() - data_cursor_;
            size_t available_blocks = bytes_remaining / block_bytes;
            size_t blocks_to_decode = std::min(blocks_needed, available_blocks);

            if (blocks_to_decode == 0) {
                if (!loop) break;
                data_cursor_ = 0;
                continue;
            }

            size_t decode_bytes = blocks_to_decode * block_bytes;
            std::vector<int16_t> block_pcm;
            std::string dec_err;
            if (DecodePtadpcm(std::span<const uint8_t>(raw_adpcm_data_.data() + data_cursor_, decode_bytes),
                              info_.channels, 36, block_pcm, dec_err)) {
                decoded_pcm.insert(decoded_pcm.end(), block_pcm.begin(), block_pcm.end());
                data_cursor_ += decode_bytes;
                blocks_needed -= blocks_to_decode;
            } else {
                break;
            }
        }

        if (decoded_pcm.empty()) return false;

        // Truncar para a quantidade exata de quadros requisitados
        size_t total_samples_req = frame_count * info_.channels;
        if (decoded_pcm.size() > total_samples_req) {
            decoded_pcm.resize(total_samples_req);
        }

        // Reamostragem se necessário (garante 48.000 Hz estéreo)
        PcmBuffer buf{info_.sample_rate, info_.channels, std::move(decoded_pcm)};
        if (info_.sample_rate != 48000 || info_.channels != 2) {
            PcmBuffer resampled = ResampleLinear(buf, 48000, 2);
            out_pcm = std::move(resampled.samples);
        } else {
            out_pcm = std::move(buf.samples);
        }

        return true;
    } else {
        // PCM 16-bit linear
        size_t bytes_per_frame = 2 * info_.channels;

        std::vector<int16_t> raw_samples;
        while (raw_samples.size() < frame_count * info_.channels) {
            if (data_cursor_ + bytes_per_frame > raw_adpcm_data_.size()) {
                if (!loop) break;
                data_cursor_ = 0;
            }

            size_t available = (raw_adpcm_data_.size() - data_cursor_) / 2;
            size_t needed = frame_count * info_.channels - raw_samples.size();
            size_t to_copy = std::min(available, needed);

            const int16_t* src = reinterpret_cast<const int16_t*>(raw_adpcm_data_.data() + data_cursor_);
            raw_samples.insert(raw_samples.end(), src, src + to_copy);
            data_cursor_ += to_copy * 2;
        }

        PcmBuffer buf{info_.sample_rate, info_.channels, std::move(raw_samples)};
        if (info_.sample_rate != 48000 || info_.channels != 2) {
            PcmBuffer resampled = ResampleLinear(buf, 48000, 2);
            out_pcm = std::move(resampled.samples);
        } else {
            out_pcm = std::move(buf.samples);
        }

        return true;
    }
}

} // namespace za::audio
