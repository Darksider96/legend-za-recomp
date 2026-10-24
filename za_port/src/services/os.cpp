#include "nn/os.hpp"
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <shared_mutex>
#include <memory>

namespace nn::os {

static int g_argc = 1;
static char* g_default_argv[] = { (char*)"za_port.exe", nullptr };

int32_t GetHostArgc() {
    std::cout << "[nn::os] GetHostArgc() -> " << g_argc << std::endl;
    return g_argc;
}

char** GetHostArgv() {
    std::cout << "[nn::os] GetHostArgv()" << std::endl;
    return g_default_argv;
}

// Side Table para sincronização do Host (mantém a struct do Guest em 8 bytes)
struct HostLightEvent {
    std::mutex mtx;
    std::condition_variable cv;
};

static std::mutex g_event_table_mutex;
static std::unordered_map<LightEventType*, std::unique_ptr<HostLightEvent>> g_event_table;

static HostLightEvent& GetOrCreateHostEvent(LightEventType* event) {
    std::lock_guard<std::mutex> lock(g_event_table_mutex);
    auto it = g_event_table.find(event);
    if (it == g_event_table.end()) {
        auto entry = std::make_unique<HostLightEvent>();
        auto& ref = *entry;
        g_event_table[event] = std::move(entry);
        return ref;
    }
    return *(it->second);
}

void InitializeLightEvent(LightEventType* event, bool initially_signaled, EventClearMode clear_mode) {
    std::cout << "[nn::os] InitializeLightEvent(signaled=" << initially_signaled 
              << ", auto_clear=" << (clear_mode == EventClearMode_AutoClear) << ")" << std::endl;
    if (event) {
        event->auto_clear = (clear_mode == EventClearMode_AutoClear);
        event->state = 1; // Initialized
        event->signal_state = initially_signaled ? 2 : 0;
        GetOrCreateHostEvent(event);
    }
}

void WaitLightEvent(LightEventType* event) {
    if (!event) return;
    auto& host_ev = GetOrCreateHostEvent(event);
    std::unique_lock<std::mutex> lock(host_ev.mtx);
    host_ev.cv.wait(lock, [&]() { return event->signal_state == 2; });

    if (event->auto_clear) {
        event->signal_state = 0;
    }
}

void SignalLightEvent(LightEventType* event) {
    if (!event) return;
    auto& host_ev = GetOrCreateHostEvent(event);
    {
        std::lock_guard<std::mutex> lock(host_ev.mtx);
        event->signal_state = 2;
    }
    host_ev.cv.notify_all();
}

void ClearLightEvent(LightEventType* event) {
    if (!event) return;
    auto& host_ev = GetOrCreateHostEvent(event);
    std::lock_guard<std::mutex> lock(host_ev.mtx);
    event->signal_state = 0;
}

// Side Table para Mutex
static std::mutex g_mutex_table_mutex;
static std::unordered_map<MutexType*, std::unique_ptr<std::recursive_mutex>> g_mutex_table;

static std::recursive_mutex& GetOrCreateHostMutex(MutexType* mutex) {
    std::lock_guard<std::mutex> lock(g_mutex_table_mutex);
    auto it = g_mutex_table.find(mutex);
    if (it == g_mutex_table.end()) {
        auto entry = std::make_unique<std::recursive_mutex>();
        auto& ref = *entry;
        g_mutex_table[mutex] = std::move(entry);
        return ref;
    }
    return *(it->second);
}

void InitializeMutex(MutexType* mutex, bool is_recursive, int32_t lock_level) {
    std::cout << "[nn::os] InitializeMutex(recursive=" << is_recursive << ", level=" << lock_level << ")" << std::endl;
    if (mutex) {
        mutex->state = 1;
        mutex->recursive = is_recursive;
        mutex->lock_level = lock_level;
        mutex->lock_count = 0;
        mutex->owner_thread = nullptr;
        mutex->critical_section = 0;
        GetOrCreateHostMutex(mutex);
    }
}

void LockMutex(MutexType* mutex) {
    if (mutex) {
        GetOrCreateHostMutex(mutex).lock();
        mutex->lock_count++;
    }
}

void UnlockMutex(MutexType* mutex) {
    if (mutex) {
        mutex->lock_count--;
        GetOrCreateHostMutex(mutex).unlock();
    }
}

// Side Table para ReaderWriterLock
static std::mutex g_rwlock_table_mutex;
static std::unordered_map<ReaderWriterLockType*, std::unique_ptr<std::shared_mutex>> g_rwlock_table;

static std::shared_mutex& GetOrCreateHostRwLock(ReaderWriterLockType* rwlock) {
    std::lock_guard<std::mutex> lock(g_rwlock_table_mutex);
    auto it = g_rwlock_table.find(rwlock);
    if (it == g_rwlock_table.end()) {
        auto entry = std::make_unique<std::shared_mutex>();
        auto& ref = *entry;
        g_rwlock_table[rwlock] = std::move(entry);
        return ref;
    }
    return *(it->second);
}

void InitializeReaderWriterLock(ReaderWriterLockType* rwlock) {
    std::cout << "[nn::os] InitializeReaderWriterLock()" << std::endl;
    if (rwlock) {
        rwlock->state = 1;
        rwlock->lock_word = 0;
        rwlock->read_count_by_writer = 0;
        rwlock->write_lock_count = 0;
        rwlock->owner_thread = nullptr;
        GetOrCreateHostRwLock(rwlock);
    }
}

void AcquireReadLock(ReaderWriterLockType* rwlock) {
    if (rwlock) {
        GetOrCreateHostRwLock(rwlock).lock_shared();
    }
}

void ReleaseReadLock(ReaderWriterLockType* rwlock) {
    if (rwlock) {
        GetOrCreateHostRwLock(rwlock).unlock_shared();
    }
}

} // namespace nn::os
