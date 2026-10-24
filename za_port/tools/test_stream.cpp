#include "audio/wwise.hpp"
#include "audio/wem_decoder.hpp"
#include <iostream>

int main() {
    za::audio::WwisePackage pck;
    std::string err;
    if (!pck.Open("C:/Users/johna/OneDrive/Documentos/legend za/romfs/audio/wwise/bin/Switch/Stream.pck", err)) {
        std::cerr << "Open error: " << err << std::endl;
        return 1;
    }

    uint32_t target_id = 294474531; // 13 MB PTADPCM, 48kHz, 2 ch, 243 s
    const auto* media = pck.Find(target_id);
    if (!media) {
        std::cerr << "Media not found!" << std::endl;
        return 1;
    }

    std::cout << "Reading first 100 KB of media " << target_id << "..." << std::endl;
    std::vector<uint8_t> wem_data;
    if (!pck.Read(*media, wem_data, err, 100 * 1024)) {
        std::cerr << "Read error: " << err << std::endl;
        return 1;
    }

    za::audio::WemInfo info;
    if (za::audio::ParseWemInfo(wem_data, info)) {
        std::cout << "Codec: 0x" << std::hex << info.codec << std::dec << ", " 
                  << info.channels << " ch, " << info.sample_rate << " Hz, data_size=" 
                  << info.data_size << ", block_align=" << info.block_align << std::endl;
    }

    std::vector<int16_t> pcm;
    size_t payload_len = wem_data.size() - info.data_offset;
    size_t block = 0x24 * info.channels;
    payload_len = (payload_len / block) * block;
    if (za::audio::DecodePtadpcm(std::span<const uint8_t>(wem_data.data() + info.data_offset, payload_len), 
                                 info.channels, 0x24, pcm, err)) {
        std::cout << "Successfully decoded " << pcm.size() << " samples (" << (pcm.size() / (info.channels * info.sample_rate * 1.0f)) << " s)!" << std::endl;
        int peak = 0;
        for (int16_t s : pcm) peak = std::max(peak, std::abs(int{s}));
        std::cout << "Peak amplitude: " << peak << std::endl;
    } else {
        std::cerr << "Decode error: " << err << std::endl;
    }

    return 0;
}
