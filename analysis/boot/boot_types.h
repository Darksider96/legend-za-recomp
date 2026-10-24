#ifndef LEGEND_ZA_ANALYSIS_BOOT_TYPES_H
#define LEGEND_ZA_ANALYSIS_BOOT_TYPES_H

#include <climits>
#include <cstddef>
#include <cstdint>
#include <type_traits>

// Reconstructed 64-bit guest ABI for this project's exefs/main + exefs/sdk.
// This is NOT a recovered copy of Nintendo's original source declarations.
// Member/enumerator names below are descriptive reconstruction names.
// Field offsets are observed in SDK instructions; padding, complete extents
// and C++ alignment follow the corresponding natural AArch64 layout.
// Evidence: sdk-types-evidence.txt, sdk-lock-details.txt, sdk-rom-details.txt,
// sdk-rom-cache.txt and boot-verified.txt in this directory.
// SDK SHA256: 0449487b457490afae879c08d6a90dd28295eaaa5c55677089a46b13991afbab
// SDK and main addresses in comments use SEPARATE module bases 0x7100000000.
//
// Use these definitions instead of the conflicting nn/os.h or nn/os.hpp
// structs in za_port; including both sets in one translation unit is invalid.
// If nn::Result is already defined, include its header first and define
// LEGEND_ZA_BOOT_TYPES_EXTERNAL_NN_RESULT before including this file.
// Keep host mutexes/condition variables in a side table keyed by guest address;
// their native representations do not fit these guest ABI structures.

static_assert(CHAR_BIT == 8, "The guest ABI requires 8-bit bytes");
static_assert(sizeof(void*) == 8, "The guest ABI requires 64-bit pointers");
static_assert(sizeof(std::size_t) == 8, "The guest ABI requires 64-bit size_t");
static_assert(sizeof(int) == 4, "The guest ABI requires 32-bit int");
static_assert(sizeof(bool) == 1, "The guest ABI requires 8-bit bool");

namespace nn {

#ifndef LEGEND_ZA_BOOT_TYPES_EXTERNAL_NN_RESULT
// Minimal, trivially returned 32-bit Result carrier, not an SDK class definition.
// SDK MountRom/QueryMountRomCacheSize return the status in w0; zero is success.
struct Result {
    std::uint32_t value;
};
#endif
static_assert(sizeof(Result) == 4 && alignof(Result) == 4,
              "nn::Result must use the 32-bit result ABI");
static_assert(std::is_trivially_copyable<Result>::value,
              "nn::Result must be trivially copyable");

namespace os {

struct ThreadType; // Only pointers are needed; its layout is not defined here.

// SDK InitializeLightEvent, 0x7100138C40: w2 == 1 selects auto-clear.
// WaitLightEvent consumes state 2 only in that branch, confirming its meaning.
// The manual-clear name/value 0 is the reconstructed non-auto counterpart.
enum EventClearMode : std::int32_t {
    EventClearMode_ManualClear = 0,
    EventClearMode_AutoClear = 1
};
static_assert(sizeof(EventClearMode) == 4, "EventClearMode is a 32-bit enum");

struct alignas(4) LightEventType {
    bool auto_clear;                    // 0x00: (clear_mode == 1)
    std::uint8_t state;                 // 0x01: initialized=1, finalized=0
    std::uint8_t reserved_02[2];        // 0x02: untouched by SDK initializer
    std::int32_t signal_state;          // 0x04: 0=clear, 1=waiting, 2=signaled
};
// main deliberately uses bytes +2/+3 for its surrounding custom spin lock.
// Do not clear those bytes during SignalLightEvent/WaitLightEvent.
static_assert(sizeof(LightEventType) == 0x08, "LightEventType size");
static_assert(alignof(LightEventType) == 0x04, "LightEventType alignment");
static_assert(offsetof(LightEventType, auto_clear) == 0x00, "auto_clear offset");
static_assert(offsetof(LightEventType, state) == 0x01, "state offset");
static_assert(offsetof(LightEventType, reserved_02) == 0x02, "reserved offset");
static_assert(offsetof(LightEventType, signal_state) == 0x04, "signal_state offset");

// SDK InitializeMutex: 0x710013C0A0; LockMutex: 0x710013C100.
// InternalCriticalSectionImplByHorizon constructor at 0x7100145EEC writes
// exactly one 32-bit word, embedded here at +0x18.
struct alignas(8) MutexType {
    std::uint8_t state;                 // 0x00: initialized=1, finalized=0
    bool recursive;                    // 0x01
    std::uint8_t reserved_02[2];        // 0x02
    std::int32_t lock_level;            // 0x04: InitializeMutex third argument
    std::int32_t lock_count;            // 0x08: incremented by LockMutex
    std::uint8_t reserved_0c[4];        // 0x0C
    ThreadType* owner_thread;           // 0x10: thread pointer, not thread ID
    std::uint32_t critical_section;     // 0x18: Horizon lock word
    std::uint8_t reserved_1c[4];        // 0x1C: tail padding
};
static_assert(sizeof(MutexType) == 0x20, "MutexType size");
static_assert(alignof(MutexType) == 0x08, "MutexType alignment");
static_assert(offsetof(MutexType, state) == 0x00, "state offset");
static_assert(offsetof(MutexType, recursive) == 0x01, "recursive offset");
static_assert(offsetof(MutexType, lock_level) == 0x04, "lock_level offset");
static_assert(offsetof(MutexType, lock_count) == 0x08, "lock_count offset");
static_assert(offsetof(MutexType, owner_thread) == 0x10, "owner_thread offset");
static_assert(offsetof(MutexType, critical_section) == 0x18, "lock word offset");

// SDK InitializeReaderWriterLock: 0x710013C280.
// AcquireReadLock: 0x7100146210; AcquireWriteLock: 0x7100146930.
// The first eight bytes are manipulated together by 64-bit LDAXR/STXR.
// Do not replace lock_word with implementation-defined C++ bitfields.
struct alignas(8) ReaderWriterLockType {
    std::uint64_t lock_word;            // 0x00: low32 critical section,
                                       //       high32 packed counters/flags
                                       // SDK initializer preserves bit 63
    std::uint8_t reserved_08[4];        // 0x08: meaning not recovered
    std::int32_t read_count_by_writer;  // 0x0C: reads acquired by write owner
    std::uint8_t state;                 // 0x10: initialized=1
    std::uint8_t reserved_11[7];        // 0x11
    ThreadType* owner_thread;           // 0x18: write owner
    std::int32_t write_lock_count;      // 0x20: recursive write count
    std::uint32_t read_wait_key;        // 0x24: internal condition-variable word
    std::uint32_t write_wait_key;       // 0x28: internal condition-variable word
    std::uint8_t reserved_2c[4];        // 0x2C: tail padding
};
static_assert(sizeof(ReaderWriterLockType) == 0x30, "ReaderWriterLockType size");
static_assert(alignof(ReaderWriterLockType) == 0x08, "ReaderWriterLockType alignment");
static_assert(offsetof(ReaderWriterLockType, lock_word) == 0x00, "lock_word offset");
static_assert(offsetof(ReaderWriterLockType, read_count_by_writer) == 0x0C,
              "read_count_by_writer offset");
static_assert(offsetof(ReaderWriterLockType, state) == 0x10, "state offset");
static_assert(offsetof(ReaderWriterLockType, owner_thread) == 0x18, "owner offset");
static_assert(offsetof(ReaderWriterLockType, write_lock_count) == 0x20,
              "write_lock_count offset");
static_assert(offsetof(ReaderWriterLockType, read_wait_key) == 0x24, "read key offset");
static_assert(offsetof(ReaderWriterLockType, write_wait_key) == 0x28, "write key offset");

static_assert(std::is_standard_layout<LightEventType>::value &&
              std::is_trivially_copyable<LightEventType>::value, "LightEventType POD ABI");
static_assert(std::is_standard_layout<MutexType>::value &&
              std::is_trivially_copyable<MutexType>::value, "MutexType POD ABI");
static_assert(std::is_standard_layout<ReaderWriterLockType>::value &&
              std::is_trivially_copyable<ReaderWriterLockType>::value,
              "ReaderWriterLockType POD ABI");

// C++ namespace linkage preserves the original Itanium symbol names when built
// with the guest toolchain. These are declarations, not stub implementations.
void InitializeLightEvent(LightEventType*, bool initially_signaled, EventClearMode);
void WaitLightEvent(LightEventType*);
void SignalLightEvent(LightEventType*);
void InitializeMutex(MutexType*, bool recursive, int lock_level);
void LockMutex(MutexType*);
void UnlockMutex(MutexType*);
void InitializeReaderWriterLock(ReaderWriterLockType*);

} // namespace os

namespace fs {

// _ZN2nn2fs22QueryMountRomCacheSizeEPm
// SDK 0x71000E1FF0; main call 0x710193A670.
Result QueryMountRomCacheSize(std::size_t* out_cache_size);

// _ZN2nn2fs8MountRomEPKcPvm
// SDK 0x71000E2360; main call 0x710193A6A4.
// AArch64: x0=mount_name, x1=cache_buffer, x2=cache_buffer_size, w0=Result.
// mount_name is a NUL-terminated string; this boot uses "data" (no colon).
// cache_buffer is writable, non-null for this overload, and must remain valid
// while mounted: RomFS builds storages referencing the caller's buffer.
// Obtain cache_buffer_size from a SUCCESSFUL QueryMountRomCacheSize call.
// main passes that exact queried size and requests allocator alignment 8.
// Never substitute a guessed fixed 64 KiB or the minimum below for the query.
Result MountRom(const char* mount_name, void* cache_buffer,
                std::size_t cache_buffer_size);

// These describe observed behavior of this SDK/callsite, not original SDK names.
namespace boot_abi {
constexpr std::size_t MountRomBufferAlignmentObserved = 8;
constexpr std::size_t MinimumQueriedMountRomCacheSize = 0x20;

// Native RomFS path, SDK 0x71000F5E50 and 0x71000E2FA0:
//   raw = header.u64_at_10 + header.u64_at_20
//       + header.u64_at_30 + header.u64_at_40;
//   queried_size = max(0x20, (raw + 7) & ~std::uint64_t(7));
// The 0x50-byte header comes from IStorage::Read, not exefs/main.
// Thus the title-specific byte count is not a constant recoverable from main.
// The alternate filesystem path also clamps the query result to at least 0x20.
// This file provides no replacement implementation of that query.
} // namespace boot_abi

} // namespace fs
} // namespace nn

#endif // LEGEND_ZA_ANALYSIS_BOOT_TYPES_H
