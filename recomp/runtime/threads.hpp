#pragma once
// Threads do jogo e SVCs de threads/sincronizacao

#include "handles.hpp"
#include "recomp/context.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>

namespace recomp::kernel {

struct Thread final : Object, std::enable_shared_from_this<Thread> {
    bool Signaled() override { return exited; }

    uint64_t entry{0}, argument{0}, stack_top{0}, tls{0}, id{0};
    int32_t priority{44}, core{0};
    uint32_t handle{0};
    bool started{false};
    std::atomic<bool> exited{false};
    // Arbitragem (protegida pelo mutex do arbitro em threads.cpp)
    std::condition_variable cv;
    bool woken{false};
    uint32_t wake_result{0};
    uint64_t wait_mutex{0}, wait_key{0}, wait_mutex_after{0};
};

Thread* CurrentThread();
// Registra a thread do host atual como a thread principal do jogo e devolve o handle dela
uint32_t CreateMainThread(Context& c);

void SvcCreateThread(Context& c);
void SvcStartThread(Context& c);
void SvcExitThread(Context& c);
void SvcGetThreadPriority(Context& c);
void SvcSetThreadPriority(Context& c);
void SvcGetThreadCoreMask(Context& c);
void SvcSetThreadCoreMask(Context& c);
void SvcGetThreadId(Context& c);
void SvcArbitrateLock(Context& c);
void SvcArbitrateUnlock(Context& c);
void SvcWaitProcessWideKeyAtomic(Context& c);
void SvcSignalProcessWideKey(Context& c);

} // namespace recomp::kernel
