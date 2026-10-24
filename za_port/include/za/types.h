#pragma once

#include <cstdint>
#include <cstddef>

namespace za {

// Base analysis load address in Ghidra
inline constexpr uintptr_t AnalysisBaseAddress = 0x7100000000ULL;

// Key RVAs mapped during reverse engineering
namespace rva {
    inline constexpr uintptr_t nninitStartup         = 0x019E8720;
    inline constexpr uintptr_t nnMain                = 0x019E8950;
    inline constexpr uintptr_t AppFramework_Construct= 0x00B10BD0;
    inline constexpr uintptr_t AppFramework_RunLoop  = 0x019E31B0;
    inline constexpr uintptr_t AppFramework_Init     = 0x005AEB60; // vtable +0x20
    inline constexpr uintptr_t AppFramework_Tick     = 0x019E32EC; // vtable +0x18
    inline constexpr uintptr_t AppFramework_Finalize = 0x0104624C; // vtable +0x28
    inline constexpr uintptr_t AppFramework_PreLoop  = 0x019E3180; // vtable +0x90

    inline constexpr uintptr_t SubsystemDispatcher   = 0x019DD110;
    inline constexpr uintptr_t RomMountCallback      = 0x019DD280; // Subsystem slot #7
    inline constexpr uintptr_t RomMountHelper        = 0x0193A498;
}

// Subsystem count in registration table (0x7103A38E18 - 0x7103A39038)
inline constexpr size_t SubsystemTableCount = 34;

} // namespace za
