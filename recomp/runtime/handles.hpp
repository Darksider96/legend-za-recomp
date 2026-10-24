#pragma once
// Objetos do kernel e tabela de handles do processo (sessoes de IPC, eventos, threads)

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>

namespace recomp::kernel {

struct Object {
    virtual ~Object() = default;
    // Objetos sincronizaveis (eventos, threads): sinalizado = WaitSynchronization retorna
    virtual bool Signaled() { return false; }
};

// Evento do kernel (ReadableEvent + WritableEvent juntos): Signal acorda quem espera em WaitSynchronization
struct Event final : Object {
    void Signal();
    void Clear();
    bool Signaled() override;

    std::mutex mutex;
    bool signaled{false};
};

uint32_t Insert(std::shared_ptr<Object> object);
std::shared_ptr<Object> Get(uint32_t handle);
bool Close(uint32_t handle);

template <typename T>
std::shared_ptr<T> GetAs(uint32_t handle) {
    return std::dynamic_pointer_cast<T>(Get(handle));
}

// Espera ate algum dos objetos ficar sinalizado; devolve o indice ou -1 no tempo limite (timeout_ns < 0: sem limite)
int WaitAny(const std::shared_ptr<Object>* objects, int count, int64_t timeout_ns);
// Acorda as esperas de WaitAny para reavaliar (chamado quando um objeto muda de estado)
void NotifyWaiters();

} // namespace recomp::kernel
