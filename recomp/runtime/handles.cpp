#include "handles.hpp"

#include <map>
#include <thread>

namespace recomp::kernel {

namespace {

std::mutex g_mutex;
std::map<uint32_t, std::shared_ptr<Object>> g_objects;
uint32_t g_next = 0xD000;

// Uma variavel de condicao global para todas as esperas: simples e suficiente enquanto ha poucas threads
std::mutex g_wait_mutex;
std::condition_variable g_wait;

}  // namespace

void NotifyWaiters() {
    std::lock_guard lock(g_wait_mutex);
    g_wait.notify_all();
}

void Event::Signal() {
    {
        std::lock_guard lock(mutex);
        signaled = true;
    }
    NotifyWaiters();
}

void Event::Clear() {
    std::lock_guard lock(mutex);
    signaled = false;
}

bool Event::Signaled() {
    std::lock_guard lock(mutex);
    return signaled;
}

uint32_t Insert(std::shared_ptr<Object> object) {
    std::lock_guard lock(g_mutex);
    const uint32_t handle = g_next++;
    g_objects[handle] = std::move(object);
    return handle;
}

std::shared_ptr<Object> Get(uint32_t handle) {
    std::lock_guard lock(g_mutex);
    const auto it = g_objects.find(handle);
    return it == g_objects.end() ? nullptr : it->second;
}

bool Close(uint32_t handle) {
    std::lock_guard lock(g_mutex);
    return g_objects.erase(handle) > 0;
}

int WaitAny(const std::shared_ptr<Object>* objects, int count, int64_t timeout_ns) {
    const auto ready = [&]() -> int {
        for (int i = 0; i < count; ++i) {
            if (objects[i]->Signaled()) return i;
        }
        return -1;
    };
    std::unique_lock lock(g_wait_mutex);
    int index = ready();
    if (index >= 0 || timeout_ns == 0) return index;
    if (timeout_ns < 0) {
        g_wait.wait(lock, [&] { return (index = ready()) >= 0; });
    } else {
        g_wait.wait_for(lock, std::chrono::nanoseconds(timeout_ns), [&] { return (index = ready()) >= 0; });
    }
    return index;
}

} // namespace recomp::kernel
