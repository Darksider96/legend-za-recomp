#pragma once
#include <cstddef>
#include <cstdint>

namespace za::re::pokemon {
// Inline FlatBuffers field 0 of personal_array.bin. These are two u16 values;
// the executable builds a separate map key species*10000 + form.
struct SpeciesFormWire {
    std::uint16_t species_id;
    std::uint16_t form_id;
};
static_assert(sizeof(SpeciesFormWire) == 4);
static_assert(offsetof(SpeciesFormWire, species_id) == 0);
static_assert(offsetof(SpeciesFormWire, form_id) == 2);
constexpr std::uint64_t personal_lookup_key(SpeciesFormWire id) noexcept {
    return std::uint64_t{id.species_id} * 10000 + id.form_id;
}
// Inline field 20 (vtable offset 0x2C). Each getter CF9428..CF95A4 reads
// one unsigned byte. Original source member names are not recovered.
struct PersonalStatBytesWire {
    std::uint8_t field_00; // interpreted as HP in the sampled records
    std::uint8_t field_01; // Attack
    std::uint8_t field_02; // Defense
    std::uint8_t field_03; // Special Attack
    std::uint8_t field_04; // Special Defense
    std::uint8_t field_05; // Speed
};
static_assert(sizeof(PersonalStatBytesWire) == 6);
static_assert(alignof(PersonalStatBytesWire) == 1);
} // namespace za::re::pokemon
