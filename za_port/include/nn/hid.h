#pragma once

#include "nn/types.h"
#include <cstdint>
#include <cstddef>

namespace nn::hid {

using NpadIdType = uint32_t;

namespace NpadId {
    inline constexpr NpadIdType No1      = 0x0;
    inline constexpr NpadIdType No2      = 0x1;
    inline constexpr NpadIdType No3      = 0x2;
    inline constexpr NpadIdType No4      = 0x3;
    inline constexpr NpadIdType Handheld = 0x20;
}

using NpadStyleSet = uint32_t;

namespace NpadStyle {
    inline constexpr NpadStyleSet FullKey     = 1 << 0; // Pro controller
    inline constexpr NpadStyleSet Handheld    = 1 << 1; // Attached Joy-Cons
    inline constexpr NpadStyleSet JoyDual     = 1 << 2; // Detached Joy-Con pair
    inline constexpr NpadStyleSet JoyLeft     = 1 << 3; // Single Left Joy-Con
    inline constexpr NpadStyleSet JoyRight    = 1 << 4; // Single Right Joy-Con
}

// 0x7 mask used by Pokémon Legends: Z-A is (FullKey | Handheld | JoyDual)
inline constexpr NpadStyleSet DefaultStyleMask = 0x7;

struct NpadState {
    int64_t sampling_number{0};
    uint64_t buttons{0};
    int32_t analog_stick_l_x{0};
    int32_t analog_stick_l_y{0};
    int32_t analog_stick_r_x{0};
    int32_t analog_stick_r_y{0};
    uint32_t attributes{0};
};

void InitializeNpad();
void SetSupportedNpadIdType(const NpadIdType* idArray, size_t count);
void SetSupportedNpadStyleSet(NpadStyleSet style);
NpadStyleSet GetNpadStyleSet(const NpadIdType& id);

void ShowControllerStrapGuide();
void InitializeTouchScreen(int width, int height);

void GetNpadState(NpadState* outState, const NpadIdType& id);

} // namespace nn::hid
