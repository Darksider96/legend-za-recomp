#include "audio/wwise.hpp"
#include <iostream>
#include <vector>
#include <algorithm>

int main() {
    za::audio::WwisePackage pck;
    std::string err;
    if (!pck.Open("C:/Users/johna/OneDrive/Documentos/legend za/romfs/audio/wwise/bin/Switch/Stream.pck", err)) {
        std::cerr << "Failed to open Stream.pck: " << err << std::endl;
        return 1;
    }

    std::vector<za::audio::WwiseMedia> sorted = pck.Streams();
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        return a.size > b.size;
    });

    std::cout << "Total streams: " << sorted.size() << std::endl;
    int count = 0;
    for (const auto& m : sorted) {
        std::vector<uint8_t> hdr;
        if (pck.Read(m, hdr, err, 128)) {
            za::audio::WemInfo info;
            if (za::audio::ParseWemInfo(hdr, info)) {
                if (info.codec == 0x8311) { // PTADPCM
                    float duration_sec = 0.0f;
                    if (info.sample_rate > 0 && info.channels > 0) {
                        // cada 36 bytes de PTADPCM = 64 amostras
                        size_t total_samples = (info.data_size / 36) * 64;
                        duration_sec = static_cast<float>(total_samples) / (info.sample_rate * info.channels);
                    }
                    std::cout << "PTADPCM Media ID: " << m.id << " | Size: " << m.size << " bytes (" 
                              << (m.size / 1024) << " KB) | " << info.sample_rate << " Hz | " 
                              << info.channels << " ch | ~" << duration_sec << " s" << std::endl;
                    count++;
                    if (count >= 15) break;
                }
            }
        }
    }
    return 0;
}
