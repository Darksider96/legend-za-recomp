#pragma once

#include "nn/types.h"
#include <cstdint>
#include <cstddef>

namespace nn::os {

// Memory
struct MemoryInfo {
    uint64_t total_available_memory_size{0};
    uint64_t total_used_memory_size{0};
};

void QueryMemoryInfo(MemoryInfo* outMemoryInfo);
uint64_t GetVirtualAddressMemoryResourceUsage();

using MemoryAllocFunction = void* (*)(size_t size);
using MemoryFreeFunction = void (*)(void* ptr);

void SetMemoryAllocatorForThreadLocal(MemoryAllocFunction allocFunc, MemoryFreeFunction freeFunc);

// Arguments
int GetHostArgc();
char** GetHostArgv();
void SetHostArgs(int argc, char** argv);

// TLS (Thread-Local Storage)
using TlsSlot = uint32_t;
using TlsDestructor = void (*)(uintptr_t);

Result AllocateTlsSlot(TlsSlot* outSlot, TlsDestructor destructor);
void FreeTlsSlot(TlsSlot slot);
uintptr_t GetTlsValue(TlsSlot slot);
void SetTlsValue(TlsSlot slot, uintptr_t value);

// Mutex
struct MutexType {
    void* impl{nullptr};
    uint32_t thread_id{0};
    int lock_level{0};
    bool recursive{false};
};

void InitializeMutex(MutexType* mutex, bool recursive, int lockLevel);
void FinalizeMutex(MutexType* mutex);
void LockMutex(MutexType* mutex);
void UnlockMutex(MutexType* mutex);
bool TryLockMutex(MutexType* mutex);
bool IsMutexLockedByCurrentThread(const MutexType* mutex);

// Reader-Writer Lock
struct ReaderWriterLockType {
    void* impl{nullptr};
};

void InitializeReaderWriterLock(ReaderWriterLockType* rwlock);
void FinalizeReaderWriterLock(ReaderWriterLockType* rwlock);
void AcquireReadLock(ReaderWriterLockType* rwlock);
void ReleaseReadLock(ReaderWriterLockType* rwlock);
void AcquireWriteLock(ReaderWriterLockType* rwlock);
void ReleaseWriteLock(ReaderWriterLockType* rwlock);

// LightEvent
enum EventClearMode {
    EventClearMode_Manual = 0,
    EventClearMode_Auto   = 1
};

struct LightEventType {
    void* impl{nullptr};
    EventClearMode clear_mode{EventClearMode_Auto};
    bool signaled{false};
};

void InitializeLightEvent(LightEventType* event, bool initialSignaled, EventClearMode clearMode);
void FinalizeLightEvent(LightEventType* event);
void WaitLightEvent(LightEventType* event);
void SignalLightEvent(LightEventType* event);
void ClearLightEvent(LightEventType* event);

// ConditionVariable
struct ConditionVariableType {
    void* impl{nullptr};
};

void InitializeConditionVariable(ConditionVariableType* cond);
void FinalizeConditionVariable(ConditionVariableType* cond);
void SignalConditionVariable(ConditionVariableType* cond);
void BroadcastConditionVariable(ConditionVariableType* cond);
void WaitConditionVariable(ConditionVariableType* cond, MutexType* mutex);

// Timing and System Tick
struct Tick {
    int64_t val{0};

    constexpr Tick() = default;
    constexpr explicit Tick(int64_t v) : val(v) {}
    constexpr int64_t GetInt64Value() const noexcept { return val; }
};

Tick GetSystemTick();
TimeSpan ConvertToTimeSpan(Tick tick);

// Threads
struct ThreadType {
    uint64_t thread_id{0};
};

ThreadType* GetCurrentThread();
void SleepThread(TimeSpan time);

} // namespace nn::os
