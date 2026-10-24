#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>

// Reconstructed storage ABI, not a Nintendo SDK source declaration.
// Evidence: phase4-hid-polling.txt and phase4-hid-sdk-detail.txt.
namespace za::boot::hid {
struct AnalogStickState {
    std::int32_t x;
    std::int32_t y;
};

struct alignas(8) NpadFullKeyStateStorage {
    std::int64_t sampling_number;       // 0x00; SDK checks consecutive samples.
    std::uint64_t buttons;             // 0x08
    AnalogStickState analog_stick_l;    // 0x10
    AnalogStickState analog_stick_r;    // 0x18
    std::uint64_t opaque_20;            // 0x20; copied by SDK, unread by main's poller.
};
using NpadHandheldStateStorage = NpadFullKeyStateStorage;

static_assert(sizeof(AnalogStickState) == 0x08);
static_assert(std::is_standard_layout_v<NpadFullKeyStateStorage>);
static_assert(sizeof(NpadFullKeyStateStorage) == 0x28);
static_assert(offsetof(NpadFullKeyStateStorage, sampling_number) == 0x00);
static_assert(offsetof(NpadFullKeyStateStorage, buttons) == 0x08);
static_assert(offsetof(NpadFullKeyStateStorage, analog_stick_l) == 0x10);
static_assert(offsetof(NpadFullKeyStateStorage, analog_stick_r) == 0x18);
static_assert(offsetof(NpadFullKeyStateStorage, opaque_20) == 0x20);
// No attributes/reserved split is asserted for the opaque tail: these static
// consumers establish eight bytes of storage, not its original source type.
} // namespace za::boot::hid
