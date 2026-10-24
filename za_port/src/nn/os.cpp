#include "nn/os.h"
#include "nn/diag.h"
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <vector>
#include <array>
#include <atomic>

namespace nn::os {

namespace {
    int g_argc = 0;
    char** g_argv = nullptr;
    MemoryAllocFunction g_tls_alloc = nullptr;
    MemoryFreeFunction g_tls_free = nullptr;

    constexpr size_t MaxTlsSlots = 64;
    struct TlsSlotEntry {
        bool in_use{false};
        TlsDestructor destructor{nullptr};
    };
    std::array<TlsSlotEntry, MaxTlsSlots> g_tls_slots{};
    std::mutex g_tls_mutex;

    thread_local std::array<uintptr_t, MaxTlsSlots> t_tls_values{};

    // Start time for tick generation (19.2 MHz clock emulation)
    const auto g_start_time = std::chrono::steady_clock::now();
}

void SetHostArgs(int argc, char** argv) {
    g_argc = argc;
    g_argv = argv;
}

int GetHostArgc() {
    return g_argc;
}

char** GetHostArgv() {
    return g_argv;
}

void QueryMemoryInfo(MemoryInfo* outMemoryInfo) {
    if (!outMemoryInfo) return;
    // Emulate standard Nintendo Switch 4GB RAM configuration
    constexpr uint64_t TotalRam = 4ULL * 1024 * 1024 * 1024;
    constexpr uint64_t UsedRam = 512ULL * 1024 * 1024;
    outMemoryInfo->total_available_memory_size = TotalRam;
    outMemoryInfo->total_used_memory_size = UsedRam;
    NN_LOG("[nn::os] QueryMemoryInfo -> Total: %llu MB, Used: %llu MB\n",
           TotalRam / (1024 * 1024), UsedRam / (1024 * 1024));
}

uint64_t GetVirtualAddressMemoryResourceUsage() {
    return 1024ULL * 1024 * 1024; // 1 GB simulated VA usage
}

void SetMemoryAllocatorForThreadLocal(MemoryAllocFunction allocFunc, MemoryFreeFunction freeFunc) {
    g_tls_alloc = allocFunc;
    g_tls_free = freeFunc;
    NN_LOG("[nn::os] SetMemoryAllocatorForThreadLocal registered alloc=%p, free=%p\n", (void*)allocFunc, (void*)freeFunc);
}

Result AllocateTlsSlot(TlsSlot* outSlot, TlsDestructor destructor) {
    if (!outSlot) return Result(1);
    std::lock_guard<std::mutex> lock(g_tls_mutex);
    for (size_t i = 0; i < MaxTlsSlots; ++i) {
        if (!g_tls_slots[i].in_use) {
            g_tls_slots[i].in_use = true;
            g_tls_slots[i].destructor = destructor;
            *outSlot = static_cast<TlsSlot>(i);
            NN_LOG("[nn::os] AllocateTlsSlot -> Slot %u\n", *outSlot);
            return ResultSuccess;
        }
    }
    return Result(2); // Out of TLS slots
}

void FreeTlsSlot(TlsSlot slot) {
    if (slot >= MaxTlsSlots) return;
    std::lock_guard<std::mutex> lock(g_tls_mutex);
    g_tls_slots[slot].in_use = false;
    g_tls_slots[slot].destructor = nullptr;
}

uintptr_t GetTlsValue(TlsSlot slot) {
    if (slot >= MaxTlsSlots) return 0;
    return t_tls_values[slot];
}

void SetTlsValue(TlsSlot slot, uintptr_t value) {
    if (slot >= MaxTlsSlots) return;
    t_tls_values[slot] = value;
}

// Mutex Implementation
struct MutexInternal {
    std::recursive_mutex r_mtx;
    std::mutex s_mtx;
};

void InitializeMutex(MutexType* mutex, bool recursive, int lockLevel) {
    if (!mutex) return;
    mutex->impl = new MutexInternal();
    mutex->recursive = recursive;
    mutex->lock_level = lockLevel;
    mutex->thread_id = 0;
}

void FinalizeMutex(MutexType* mutex) {
    if (!mutex || !mutex->impl) return;
    delete static_cast<MutexInternal*>(mutex->impl);
    mutex->impl = nullptr;
}

void LockMutex(MutexType* mutex) {
    if (!mutex || !mutex->impl) return;
    auto* internal = static_cast<MutexInternal*>(mutex->impl);
    if (mutex->recursive) {
        internal->r_mtx.lock();
    } else {
        internal->s_mtx.lock();
    }
}

void UnlockMutex(MutexType* mutex) {
    if (!mutex || !mutex->impl) return;
    auto* internal = static_cast<MutexInternal*>(mutex->impl);
    if (mutex->recursive) {
        internal->r_mtx.unlock();
    } else {
        internal->s_mtx.unlock();
    }
}

bool TryLockMutex(MutexType* mutex) {
    if (!mutex || !mutex->impl) return false;
    auto* internal = static_cast<MutexInternal*>(mutex->impl);
    return mutex->recursive ? internal->r_mtx.try_lock() : internal->s_mtx.try_lock();
}

bool IsMutexLockedByCurrentThread(const MutexType* mutex) {
    // Basic stub; in single-thread test or cooperative runner returns true when locked
    return mutex != nullptr && mutex->impl != nullptr;
}

// Reader-Writer Lock Implementation
struct RwLockInternal {
    std::shared_mutex mtx;
};

void InitializeReaderWriterLock(ReaderWriterLockType* rwlock) {
    if (!rwlock) return;
    rwlock->impl = new RwLockInternal();
}

void FinalizeReaderWriterLock(ReaderWriterLockType* rwlock) {
    if (!rwlock || !rwlock->impl) return;
    delete static_cast<RwLockInternal*>(rwlock->impl);
    rwlock->impl = nullptr;
}

void AcquireReadLock(ReaderWriterLockType* rwlock) {
    if (!rwlock || !rwlock->impl) return;
    static_cast<RwLockInternal*>(rwlock->impl)->mtx.lock_shared();
}

void ReleaseReadLock(ReaderWriterLockType* rwlock) {
    if (!rwlock || !rwlock->impl) return;
    static_cast<RwLockInternal*>(rwlock->impl)->mtx.unlock_shared();
}

void AcquireWriteLock(ReaderWriterLockType* rwlock) {
    if (!rwlock || !rwlock->impl) return;
    static_cast<RwLockInternal*>(rwlock->impl)->mtx.lock();
}

void ReleaseWriteLock(ReaderWriterLockType* rwlock) {
    if (!rwlock || !rwlock->impl) return;
    static_cast<RwLockInternal*>(rwlock->impl)->mtx.unlock();
}

// LightEvent Implementation
struct LightEventInternal {
    std::mutex mtx;
    std::condition_variable cv;
};

void InitializeLightEvent(LightEventType* event, bool initialSignaled, EventClearMode clearMode) {
    if (!event) return;
    event->impl = new LightEventInternal();
    event->clear_mode = clearMode;
    event->signaled = initialSignaled;
}

void FinalizeLightEvent(LightEventType* event) {
    if (!event || !event->impl) return;
    delete static_cast<LightEventInternal*>(event->impl);
    event->impl = nullptr;
}

void WaitLightEvent(LightEventType* event) {
    if (!event || !event->impl) return;
    auto* internal = static_cast<LightEventInternal*>(event->impl);
    std::unique_lock<std::mutex> lock(internal->mtx);
    internal->cv.wait(lock, [&] { return event->signaled; });
    if (event->clear_mode == EventClearMode_Auto) {
        event->signaled = false;
    }
}

void SignalLightEvent(LightEventType* event) {
    if (!event || !event->impl) return;
    auto* internal = static_cast<LightEventInternal*>(event->impl);
    {
        std::lock_guard<std::mutex> lock(internal->mtx);
        event->signaled = true;
    }
    internal->cv.notify_all();
}

void ClearLightEvent(LightEventType* event) {
    if (!event || !event->impl) return;
    auto* internal = static_cast<LightEventInternal*>(event->impl);
    std::lock_guard<std::mutex> lock(internal->mtx);
    event->signaled = false;
}

// ConditionVariable Implementation
struct CondVarInternal {
    std::condition_variable cv;
};

void InitializeConditionVariable(ConditionVariableType* cond) {
    if (!cond) return;
    cond->impl = new CondVarInternal();
}

void FinalizeConditionVariable(ConditionVariableType* cond) {
    if (!cond || !cond->impl) return;
    delete static_cast<CondVarInternal*>(cond->impl);
    cond->impl = nullptr;
}

void SignalConditionVariable(ConditionVariableType* cond) {
    if (!cond || !cond->impl) return;
    static_cast<CondVarInternal*>(cond->impl)->cv.notify_one();
}

void BroadcastConditionVariable(ConditionVariableType* cond) {
    if (!cond || !cond->impl) return;
    static_cast<CondVarInternal*>(cond->impl)->cv.notify_all();
}

void WaitConditionVariable(ConditionVariableType* cond, MutexType* mutex) {
    if (!cond || !cond->impl || !mutex || !mutex->impl) return;
    auto* c_internal = static_cast<CondVarInternal*>(cond->impl);
    auto* m_internal = static_cast<MutexInternal*>(mutex->impl);
    // Standard condition variable wait with underlying mutex
    std::unique_lock<std::mutex> lk(m_internal->s_mtx, std::adopt_lock);
    c_internal->cv.wait(lk);
    lk.release();
}

// Timing & System Tick
Tick GetSystemTick() {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - g_start_time).count();
    // Switch clock is 19.2 MHz (19,200,000 Hz) -> 1 tick = 1,000,000,000 / 19,200,000 ns = 52.083 ns
    const int64_t ticks = (elapsed_ns * 192) / 10000;
    return Tick(ticks);
}

TimeSpan ConvertToTimeSpan(Tick tick) {
    // Convert 19.2 MHz ticks to nanoseconds
    const int64_t ns = (tick.GetInt64Value() * 10000) / 192;
    return TimeSpan::FromNanoSeconds(ns);
}

// Thread Functions
ThreadType* GetCurrentThread() {
    static thread_local ThreadType t_current_thread{0};
    return &t_current_thread;
}

void SleepThread(TimeSpan time) {
    std::this_thread::sleep_for(std::chrono::nanoseconds(time.GetNanoSeconds()));
}

} // namespace nn::os
