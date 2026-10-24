#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

// Recovered storage layouts, not original Nintendo/Game Freak declarations.
// Addresses are target 64-bit addresses. Padding has no defined value.
// FlatBuffers FileLocation/PackageMetadata tables are NOT fixed-stride PODs.
namespace za::boot::archive {
struct TrpfsHeader {
    char magic[8];                       // "ONEPACK\0"
    std::uint64_t footer_offset;         // absolute file offset
};
struct TrpfsRange {
    std::uint64_t begin;                 // absolute file offset
    std::uint64_t end;                   // exclusive; size = end - begin
};
struct TrpfsHashIndexNodeStorage {
    std::uint64_t next_address;          // 0x00
    std::uint64_t cached_hash;           // 0x08
    std::uint64_t key_hash;              // 0x10
    std::uint32_t range_index;           // 0x18
    std::uint32_t padding_1c;            // 0x1C
};
struct TrpfdPackageNodeStorage {
    std::uint64_t next_address;          // 0x00
    std::uint64_t cached_hash;           // 0x08
    std::uint64_t package_name_hash;     // 0x10
    std::uint64_t byte_length;           // 0x18; TRPFD metadata.field0
    std::uint64_t file_count;            // 0x20; TRPFD metadata.field1
    std::uint64_t opaque_28;             // 0x28; initialized to zero
};
inline constexpr std::uint64_t path_hash(std::string_view relative_bytes) {
    std::uint64_t hash = 0xCBF29CE484222645ULL; // modified offset basis
    for (unsigned char byte : relative_bytes)
        hash = (hash ^ byte) * 0x100000001B3ULL;
    return hash;
}
static_assert(sizeof(TrpfsHeader) == 0x10);
static_assert(offsetof(TrpfsHeader, footer_offset) == 8);
static_assert(sizeof(TrpfsRange) == 0x10);
static_assert(sizeof(TrpfsHashIndexNodeStorage) == 0x20);
static_assert(offsetof(TrpfsHashIndexNodeStorage, range_index) == 0x18);
static_assert(sizeof(TrpfdPackageNodeStorage) == 0x30);
static_assert(offsetof(TrpfdPackageNodeStorage, byte_length) == 0x18);
static_assert(offsetof(TrpfdPackageNodeStorage, file_count) == 0x20);
static_assert(offsetof(TrpfdPackageNodeStorage, opaque_28) == 0x28);
static_assert(path_hash("arc/ai_influenceba_battle_area_0003_01_00_00.imap.trpak")
              == 0x7B906E2233F42E3BULL);
} // namespace za::boot::archive
