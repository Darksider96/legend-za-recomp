#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace za::loader {

struct NsoSectionHeader {
    uint32_t file_offset;
    uint32_t memory_offset;
    uint32_t decompressed_size;
    uint32_t align_or_reserved;
};

struct NsoHeader {
    uint32_t magic; // "NSO0"
    uint32_t version;
    uint32_t reserved;
    uint32_t flags;
    NsoSectionHeader text;
    NsoSectionHeader rodata;
    NsoSectionHeader data;
    uint8_t module_id[0x20];
    uint32_t text_compressed_size;
    uint32_t rodata_compressed_size;
    uint32_t data_compressed_size;
};

class NsoLoader {
public:
    NsoLoader();
    ~NsoLoader();

    bool Load(const std::string& filepath, uintptr_t preferred_base = 0x7100000000ULL);
    uintptr_t GetBaseAddress() const { return base_address_; }
    size_t GetTotalSize() const { return total_size_; }
    void* GetMemoryPointer() const { return allocated_memory_; }

private:
    uintptr_t base_address_{0};
    size_t total_size_{0};
    void* allocated_memory_{nullptr};

    static int DecompressLZ4(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_len);
};

} // namespace za::loader
