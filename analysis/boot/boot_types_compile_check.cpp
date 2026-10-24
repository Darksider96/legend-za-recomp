#include "boot_types.h"

// Compile-only ABI probe. Undefined symbols in the object must retain the
// linkage names imported by exefs/main. No game or SDK code is executed.
namespace boot_types_compile_check {
auto initialize_light_event = &nn::os::InitializeLightEvent;
auto wait_light_event = &nn::os::WaitLightEvent;
auto signal_light_event = &nn::os::SignalLightEvent;
auto initialize_mutex = &nn::os::InitializeMutex;
auto lock_mutex = &nn::os::LockMutex;
auto unlock_mutex = &nn::os::UnlockMutex;
auto initialize_reader_writer_lock = &nn::os::InitializeReaderWriterLock;
auto query_mount_rom_cache_size = &nn::fs::QueryMountRomCacheSize;
auto mount_rom = &nn::fs::MountRom;
} // namespace boot_types_compile_check
