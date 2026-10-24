// Threads do jogo e sincronizacao do kernel (arbitragem de mutex e variavel de condicao do Horizon)
//
// Cada thread do jogo e uma thread do host com Context e regiao de TLS (TPIDRRO_EL0) proprios. ExitThread desenrola
// a pilha do host com uma excecao. A arbitragem segue o Mesosphere: o mutex do usuario guarda o handle do dono e o
// bit 0x40000000 de "ha esperando"; o kernel so e chamado na disputa.

#include "threads.hpp"

#include "recomp/runtime.hpp"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <list>
#include <mutex>

namespace recomp::kernel {

namespace {

constexpr uint32_t kWaitMask = 0x40000000;
constexpr uint32_t kPseudoCurrentThread = 0xFFFF8000;
constexpr uint32_t kResultSuccess = 0, kResultInvalidHandle = 0xE401, kResultTimedOut = 0xEA01, kResultOutOfMemory = 0xD001;

struct ThreadExit {};

std::mutex g_arbiter;               // protege as filas de espera e os campos de espera das threads
std::list<Thread*> g_mutex_waiters;  // esperando um mutex (wait_mutex)
std::list<Thread*> g_key_waiters;    // esperando uma variavel de condicao (wait_key)
std::atomic<uint64_t> g_next_id{0x60};

thread_local Thread* t_current = nullptr;

uint32_t Load32(uint64_t address) { return std::atomic_ref<uint32_t>(*reinterpret_cast<uint32_t*>(address)).load(); }
void Store32(uint64_t address, uint32_t value) { std::atomic_ref<uint32_t>(*reinterpret_cast<uint32_t*>(address)).store(value); }

std::shared_ptr<Thread> Resolve(uint32_t handle) {
    if (handle == kPseudoCurrentThread && t_current) return std::static_pointer_cast<Thread>(t_current->shared_from_this());
    return GetAs<Thread>(handle);
}

// Entrega o mutex em address ao proximo da fila (maior prioridade = menor numero; FIFO entre iguais)
void ReleaseMutexLocked(uint64_t address) {
    auto best = g_mutex_waiters.end();
    size_t others = 0;
    for (auto it = g_mutex_waiters.begin(); it != g_mutex_waiters.end(); ++it) {
        if ((*it)->wait_mutex != address) continue;
        if (best == g_mutex_waiters.end() || (*it)->priority < (*best)->priority) best = it;
        ++others;
    }
    if (best == g_mutex_waiters.end()) {
        Store32(address, 0);
        return;
    }
    Thread* next = *best;
    g_mutex_waiters.erase(best);
    Store32(address, next->handle | (others > 1 ? kWaitMask : 0));
    next->wait_mutex = 0;
    next->woken = true;
    next->wake_result = kResultSuccess;
    next->cv.notify_all();
}

void* AllocateTls() { return VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); }

}  // namespace

Thread* CurrentThread() { return t_current; }

uint32_t CreateMainThread(Context& c) {
    auto thread = std::make_shared<Thread>();
    thread->id = g_next_id++;
    thread->priority = 44;
    thread->tls = c.tpidrro;
    thread->started = true;
    thread->handle = Insert(thread);
    t_current = thread.get();
    static std::shared_ptr<Thread> keep = thread;  // a thread principal vive o processo todo
    return thread->handle;
}

// svcCreateThread(X1 = entrada, X2 = argumento, X3 = topo da pilha, W4 = prioridade, W5 = nucleo) -> W1 = handle
void SvcCreateThread(Context& c) {
    auto thread = std::make_shared<Thread>();
    thread->entry = c.x[1];
    thread->argument = c.x[2];
    thread->stack_top = c.x[3];
    thread->priority = static_cast<int32_t>(c.x[4]);
    thread->core = static_cast<int32_t>(c.x[5]);
    thread->id = g_next_id++;
    thread->tls = reinterpret_cast<uint64_t>(AllocateTls());
    if (!thread->tls) {
        c.x[0] = kResultOutOfMemory;
        return;
    }
    thread->handle = Insert(thread);
    Log("[thread] criada 0x%x: entrada %s, prioridade %d, nucleo %d", thread->handle, DescribeAddress(thread->entry).c_str(), thread->priority, thread->core);
    c.x[1] = thread->handle;
    c.x[0] = kResultSuccess;
}

void SvcStartThread(Context& c) {
    const auto thread = GetAs<Thread>(static_cast<uint32_t>(c.x[0]));
    if (!thread) {
        c.x[0] = kResultInvalidHandle;
        return;
    }
    thread->started = true;
    std::thread([thread] {
        t_current = thread.get();
        Context context;
        context.x[0] = thread->argument;
        context.sp = thread->stack_top & ~uint64_t{0xF};
        context.tpidrro = thread->tls;
        try {
            Call(context, thread->entry);
        } catch (const ThreadExit&) {
        }
        thread->exited = true;
        NotifyWaiters();
        Log("[thread] 0x%x terminou", thread->handle);
    }).detach();
    c.x[0] = kResultSuccess;
}

void SvcExitThread(Context&) { throw ThreadExit{}; }

void SvcGetThreadPriority(Context& c) {
    const auto thread = Resolve(static_cast<uint32_t>(c.x[1]));
    if (!thread) {
        c.x[0] = kResultInvalidHandle;
        return;
    }
    c.x[1] = static_cast<uint32_t>(thread->priority);
    c.x[0] = kResultSuccess;
}

void SvcSetThreadPriority(Context& c) {
    const auto thread = Resolve(static_cast<uint32_t>(c.x[0]));
    if (!thread) {
        c.x[0] = kResultInvalidHandle;
        return;
    }
    thread->priority = static_cast<int32_t>(c.x[1]);
    c.x[0] = kResultSuccess;
}

// GetThreadCoreMask(W2 = handle) -> W1 = nucleo ideal, X2 = mascara
void SvcGetThreadCoreMask(Context& c) {
    const auto thread = Resolve(static_cast<uint32_t>(c.x[2]));
    if (!thread) {
        c.x[0] = kResultInvalidHandle;
        return;
    }
    c.x[1] = static_cast<uint32_t>(thread->core < 0 ? 0 : thread->core);
    c.x[2] = 0xF;
    c.x[0] = kResultSuccess;
}

void SvcSetThreadCoreMask(Context& c) {
    const auto thread = Resolve(static_cast<uint32_t>(c.x[0]));
    if (!thread) {
        c.x[0] = kResultInvalidHandle;
        return;
    }
    if (static_cast<int32_t>(c.x[1]) >= 0) thread->core = static_cast<int32_t>(c.x[1]);
    c.x[0] = kResultSuccess;
}

// GetThreadId(W1 = handle) -> X1
void SvcGetThreadId(Context& c) {
    const auto thread = Resolve(static_cast<uint32_t>(c.x[1]));
    if (!thread) {
        c.x[0] = kResultInvalidHandle;
        return;
    }
    c.x[1] = thread->id;
    c.x[0] = kResultSuccess;
}

// ArbitrateLock(W0 = handle do dono, X1 = endereco do mutex, W2 = handle de quem pede)
void SvcArbitrateLock(Context& c) {
    const uint32_t owner = static_cast<uint32_t>(c.x[0]);
    const uint64_t address = c.x[1];
    Thread* self = t_current;
    std::unique_lock lock(g_arbiter);
    if (Load32(address) != (owner | kWaitMask)) {  // o dono ja liberou
        c.x[0] = kResultSuccess;
        return;
    }
    self->wait_mutex = address;
    self->woken = false;
    g_mutex_waiters.push_back(self);
    self->cv.wait(lock, [self] { return self->woken; });
    c.x[0] = self->wake_result;
}

void SvcArbitrateUnlock(Context& c) {
    std::lock_guard lock(g_arbiter);
    ReleaseMutexLocked(c.x[0]);
    c.x[0] = kResultSuccess;
}

// WaitProcessWideKeyAtomic(X0 = mutex, X1 = chave, W2 = handle, X3 = timeout): libera o mutex e espera a chave; ao ser
// sinalizada, a thread volta a disputar o mutex (no timeout, o codigo do usuario readquire sozinho)
void SvcWaitProcessWideKeyAtomic(Context& c) {
    const uint64_t mutex = c.x[0], key = c.x[1];
    const int64_t timeout = static_cast<int64_t>(c.x[3]);
    Thread* self = t_current;
    std::unique_lock lock(g_arbiter);
    ReleaseMutexLocked(mutex);
    Store32(key, 1);
    self->wait_key = key;
    self->wait_mutex_after = mutex;
    self->woken = false;
    g_key_waiters.push_back(self);
    const auto done = [self] { return self->woken; };
    bool ok = true;
    if (timeout < 0) self->cv.wait(lock, done);
    else ok = self->cv.wait_for(lock, std::chrono::nanoseconds(timeout), done);
    if (!ok) {
        g_key_waiters.remove(self);
        g_mutex_waiters.remove(self);
        self->wait_key = self->wait_mutex = 0;
        c.x[0] = kResultTimedOut;
        return;
    }
    c.x[0] = self->wake_result;
}

// SignalProcessWideKey(X0 = chave, W1 = quantidade; <= 0 = todas)
void SvcSignalProcessWideKey(Context& c) {
    const uint64_t key = c.x[0];
    int32_t count = static_cast<int32_t>(c.x[1]);
    std::lock_guard lock(g_arbiter);
    while (count <= 0 || count-- > 0) {
        auto best = g_key_waiters.end();
        for (auto it = g_key_waiters.begin(); it != g_key_waiters.end(); ++it) {
            if ((*it)->wait_key == key && (best == g_key_waiters.end() || (*it)->priority < (*best)->priority)) best = it;
        }
        if (best == g_key_waiters.end()) break;
        Thread* t = *best;
        g_key_waiters.erase(best);
        t->wait_key = 0;
        // Readquire o mutex: livre -> e dele; ocupado -> marca que ha espera e entra na fila do mutex
        const uint64_t address = t->wait_mutex_after;
        uint32_t value = Load32(address);
        if (value == 0) {
            Store32(address, t->handle);
            t->woken = true;
            t->wake_result = kResultSuccess;
            t->cv.notify_all();
        } else {
            Store32(address, value | kWaitMask);
            t->wait_mutex = address;
            g_mutex_waiters.push_back(t);
        }
    }
    if (std::none_of(g_key_waiters.begin(), g_key_waiters.end(), [key](Thread* t) { return t->wait_key == key; })) Store32(key, 0);
    c.x[0] = kResultSuccess;
}

} // namespace recomp::kernel
