// HLE do kernel do Horizon: SVCs feitas pelo codigo traduzido (sobretudo pelo sdk)
//
// Convencao: argumentos em X0..X7; W0 volta com o Result (0 = sucesso) e as saidas seguem em X1.. Toda SVC e
// registrada no log; as que ainda nao tem implementacao param o jogo com o nome e os argumentos.
//
// Espaco de enderecos: como os ponteiros do jogo sao ponteiros do host, as regioes que o kernel informa (GetInfo)
// sao faixas fixas reservadas no host. Os modulos ficam em 0x71xxxxxxxx (dentro da regiao de ASLR de 39 bits).

#include "kernel.hpp"
#include "handles.hpp"
#include "ipc.hpp"
#include "threads.hpp"
#include "recomp/runtime.hpp"

#include <windows.h>

#include <chrono>
#include <cstring>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace recomp {

namespace {

constexpr uint64_t kAslrBase = 0x0008000000ull, kAslrSize = 0x7FF8000000ull;
constexpr uint64_t kHeapBase = 0x2000000000ull, kHeapRegionSize = 0x1000000000ull;
constexpr uint64_t kAliasBase = 0x3000000000ull, kAliasSize = 0x1000000000ull;
constexpr uint64_t kStackRegionBase = 0x4000000000ull, kStackRegionSize = 0x0080000000ull;
constexpr uint64_t kTotalMemory = 0xCD500000ull;  // pool de aplicacao do Switch (~3,2 GiB)

constexpr uint32_t kResultSuccess = 0;
constexpr uint32_t kResultInvalidEnumValue = 0xF001;   // 2001-0120
constexpr uint32_t kResultInvalidSize = 0xCA01;        // 2001-0101
constexpr uint32_t kResultOutOfMemory = 0xD001;        // 2001-0104
constexpr uint32_t kResultInvalidHandle = 0xE401;      // 2001-0114
constexpr uint32_t kResultTimedOut = 0xEA01;           // 2001-0117


uint64_t g_heap_size = 0;
uint64_t g_program_id = 0;
uint64_t g_code_size = 0;

const char* SvcName(uint32_t id) {
    switch (id) {
    case 0x01: return "SetHeapSize";
    case 0x02: return "SetMemoryPermission";
    case 0x03: return "SetMemoryAttribute";
    case 0x04: return "MapMemory";
    case 0x05: return "UnmapMemory";
    case 0x06: return "QueryMemory";
    case 0x07: return "ExitProcess";
    case 0x08: return "CreateThread";
    case 0x09: return "StartThread";
    case 0x0A: return "ExitThread";
    case 0x0B: return "SleepThread";
    case 0x0C: return "GetThreadPriority";
    case 0x0D: return "SetThreadPriority";
    case 0x0E: return "GetThreadCoreMask";
    case 0x0F: return "SetThreadCoreMask";
    case 0x10: return "GetCurrentProcessorNumber";
    case 0x11: return "SignalEvent";
    case 0x12: return "ClearEvent";
    case 0x13: return "MapSharedMemory";
    case 0x14: return "UnmapSharedMemory";
    case 0x15: return "CreateTransferMemory";
    case 0x16: return "CloseHandle";
    case 0x17: return "ResetSignal";
    case 0x18: return "WaitSynchronization";
    case 0x19: return "CancelSynchronization";
    case 0x1A: return "ArbitrateLock";
    case 0x1B: return "ArbitrateUnlock";
    case 0x1C: return "WaitProcessWideKeyAtomic";
    case 0x1D: return "SignalProcessWideKey";
    case 0x1E: return "GetSystemTick";
    case 0x1F: return "ConnectToNamedPort";
    case 0x20: return "SendSyncRequestLight";
    case 0x21: return "SendSyncRequest";
    case 0x22: return "SendSyncRequestWithUserBuffer";
    case 0x23: return "SendAsyncRequestWithUserBuffer";
    case 0x24: return "GetProcessId";
    case 0x25: return "GetThreadId";
    case 0x26: return "Break";
    case 0x27: return "OutputDebugString";
    case 0x28: return "ReturnFromException";
    case 0x29: return "GetInfo";
    case 0x2C: return "MapPhysicalMemory";
    case 0x2D: return "UnmapPhysicalMemory";
    case 0x32: return "SetThreadActivity";
    case 0x33: return "GetThreadContext3";
    case 0x34: return "WaitForAddress";
    case 0x35: return "SignalToAddress";
    case 0x36: return "SynchronizePreemptionState";
    case 0x40: return "CreateSession";
    case 0x41: return "AcceptSession";
    case 0x43: return "ReplyAndReceive";
    case 0x45: return "CreateEvent";
    case 0x4B: return "CreateCodeMemory";
    case 0x4C: return "ControlCodeMemory";
    case 0x6F: return "GetSystemInfo";
    default: return "?";
    }
}

const char* InfoName(uint32_t id) {
    static const char* names[] = {"CoreMask", "PriorityMask", "AliasRegionAddress", "AliasRegionSize", "HeapRegionAddress", "HeapRegionSize",
                                  "TotalMemorySize", "UsedMemorySize", "DebuggerAttached", "ResourceLimit", "IdleTickCount", "RandomEntropy",
                                  "AslrRegionAddress", "AslrRegionSize", "StackRegionAddress", "StackRegionSize", "SystemResourceSizeTotal",
                                  "SystemResourceSizeUsed", "ProgramId", "InitialProcessIdRange", "UserExceptionContextAddress",
                                  "TotalNonSystemMemorySize", "UsedNonSystemMemorySize", "IsApplication", "FreeThreadCount", "ThreadTickCount",
                                  "IsSvcPermitted", "IoRegionHint", "AliasRegionExtraSize"};
    return id < sizeof(names) / sizeof(names[0]) ? names[id] : "?";
}

void Return(Context& c, uint32_t result) { c.x[0] = result; }

// Reserva (sem commit) as regioes alias e de pilha: o Windows so reserva em blocos de 64 KiB, mas faz commit por
// pagina, entao MapMemory em paginas de 4 KiB funciona dentro de uma reserva previa
void ReserveRegions() {
    static const bool reserved = [] {
        const bool alias = VirtualAlloc(reinterpret_cast<void*>(kAliasBase), kAliasSize, MEM_RESERVE, PAGE_READWRITE) != nullptr;
        const bool stack = VirtualAlloc(reinterpret_cast<void*>(kStackRegionBase), kStackRegionSize, MEM_RESERVE, PAGE_READWRITE) != nullptr;
        if (!alias || !stack) Log("[svc] aviso: reserva das regioes alias/pilha falhou (alias=%d pilha=%d)", alias, stack);
        return true;
    }();
    (void)reserved;
}

// svcSetHeapSize(size em X1) -> X1 = endereco do heap. O heap cresce no mesmo lugar (reserva unica da regiao).
void SetHeapSize(Context& c) {
    const uint64_t size = c.x[1];
    if (size % 0x200000) return Return(c, kResultInvalidSize);
    static bool reserved = false;
    if (!reserved) {
        if (VirtualAlloc(reinterpret_cast<void*>(kHeapBase), kHeapRegionSize, MEM_RESERVE, PAGE_READWRITE) != reinterpret_cast<void*>(kHeapBase)) {
            Fatal(c, "[svc] SetHeapSize: nao foi possivel reservar a regiao do heap em 0x%llx", static_cast<unsigned long long>(kHeapBase));
        }
        reserved = true;
    }
    if (size > g_heap_size && !VirtualAlloc(reinterpret_cast<void*>(kHeapBase + g_heap_size), size - g_heap_size, MEM_COMMIT, PAGE_READWRITE)) {
        return Return(c, kResultOutOfMemory);
    }
    if (size < g_heap_size) VirtualFree(reinterpret_cast<void*>(kHeapBase + size), g_heap_size - size, MEM_DECOMMIT);
    g_heap_size = size;
    c.x[1] = kHeapBase;
    Return(c, kResultSuccess);
}

// svcGetInfo(W1 = tipo, W2 = handle, X3 = subtipo) -> X1 = valor
void GetInfo(Context& c) {
    const uint32_t id = static_cast<uint32_t>(c.x[1]);
    const uint64_t sub = c.x[3];
    uint64_t value = 0;
    switch (id) {
    case 0: value = 0xF; break;                        // CoreMask: nucleos 0-3
    case 1: value = 0xFFFFFFFFFFFFFFFFull; break;      // PriorityMask
    case 2: value = kAliasBase; break;
    case 3: value = kAliasSize; break;
    case 4: value = kHeapBase; break;
    case 5: value = kHeapRegionSize; break;
    case 6: case 21: value = kTotalMemory; break;
    case 7: case 22: value = g_heap_size + g_code_size; break;
    case 8: value = 0; break;                          // sem depurador
    case 9: value = 0; break;                          // handle de limite de recursos: nenhum
    case 11: {                                         // RandomEntropy[0..3]
        static std::mt19937_64 rng(std::random_device{}());
        if (sub > 3) return Return(c, kResultInvalidEnumValue);
        value = rng();
        break;
    }
    case 12: value = kAslrBase; break;
    case 13: value = kAslrSize; break;
    case 14: value = kStackRegionBase; break;
    case 15: value = kStackRegionSize; break;
    case 16: case 17: value = 0; break;
    case 18: value = g_program_id; break;
    case 20: value = 0; break;
    case 23: value = 1; break;                         // IsApplication
    case 24: value = 64; break;                        // FreeThreadCount
    case 26: value = 1; break;                         // IsSvcPermitted (14.0+)
    case 27: value = 0; break;                         // IoRegionHint (16.0+)
    case 28: value = 0; break;                         // AliasRegionExtraSize (18.0+): sem regiao extra
    default:
        Fatal(c, "[svc] GetInfo tipo %u (%s) subtipo %llu nao implementado", id, InfoName(id), static_cast<unsigned long long>(sub));
    }
    Log("[svc] GetInfo %s(%llu) = 0x%llx", InfoName(id), static_cast<unsigned long long>(sub), static_cast<unsigned long long>(value));
    c.x[1] = value;
    Return(c, kResultSuccess);
}

// svcQueryMemory(X0 = MemoryInfo*, X1 = PageInfo*, X2 = endereco): faixa contigua com o mesmo estado no host
void QueryMemory(Context& c) {
    const uint64_t address = c.x[2];
    MEMORY_BASIC_INFORMATION mbi{};
    struct MemoryInfo {
        uint64_t base, size;
        uint32_t type, attribute, permission, ipc_refcount, device_refcount, padding;
    } info{};
    ReserveRegions();
    if (VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi))) {
        // A partir da pagina consultada, a faixa contigua no mesmo estado; reservado sem commit conta como livre (as
        // regioes alias, de pilha e de heap sao reservadas de antemao no host)
        info.base = reinterpret_cast<uint64_t>(mbi.BaseAddress);
        info.size = mbi.RegionSize;
        info.type = mbi.State == MEM_COMMIT ? 5 : 0;        // Heap / livre
        info.permission = mbi.State == MEM_COMMIT ? 3 : 0;  // RW
    } else {  // fora do espaco do usuario do host
        info.base = address & ~uint64_t{0xFFF};
        info.size = 0x1000;
        info.type = 0x10;  // Inaccessible
    }
    std::memcpy(reinterpret_cast<void*>(c.x[0]), &info, sizeof(info));
    c.x[1] = 0;  // PageInfo
    Return(c, kResultSuccess);
}

}  // namespace

void SetKernelProcessInfo(uint64_t program_id, uint64_t code_size) {
    g_program_id = program_id;
    g_code_size = code_size;
}

void SupervisorCall(Context& c, uint32_t id) {
    switch (id) {
    case 0x01: return SetHeapSize(c);
    case 0x06: return QueryMemory(c);
    case 0x0B: {  // SleepThread(ns); 0/-1/-2 = ceder a vez
        const int64_t ns = static_cast<int64_t>(c.x[0]);
        if (ns > 0) std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
        else std::this_thread::yield();
        return;
    }
    case 0x04: {  // MapMemory(X0 = destino, X1 = origem, X2 = tamanho): pilhas na regiao alias; o destino ganha memoria nova
        ReserveRegions();
        if (VirtualAlloc(reinterpret_cast<void*>(c.x[0]), c.x[2], MEM_COMMIT, PAGE_READWRITE) != reinterpret_cast<void*>(c.x[0])) {
            Log("[svc] MapMemory 0x%llx <- 0x%llx (0x%llx bytes) falhou", static_cast<unsigned long long>(c.x[0]), static_cast<unsigned long long>(c.x[1]),
                static_cast<unsigned long long>(c.x[2]));
            return Return(c, kResultOutOfMemory);
        }
        return Return(c, kResultSuccess);
    }
    case 0x05:  // UnmapMemory(X0 = destino, X1 = origem, X2 = tamanho): volta a ser reservado (livre para o jogo)
        VirtualFree(reinterpret_cast<void*>(c.x[0]), c.x[2], MEM_DECOMMIT);
        return Return(c, kResultSuccess);
    case 0x08: return kernel::SvcCreateThread(c);
    case 0x09: return kernel::SvcStartThread(c);
    case 0x0A: return kernel::SvcExitThread(c);
    case 0x0C: return kernel::SvcGetThreadPriority(c);
    case 0x0D: return kernel::SvcSetThreadPriority(c);
    case 0x0E: return kernel::SvcGetThreadCoreMask(c);
    case 0x0F: return kernel::SvcSetThreadCoreMask(c);
    case 0x1A: return kernel::SvcArbitrateLock(c);
    case 0x1B: return kernel::SvcArbitrateUnlock(c);
    case 0x1C: return kernel::SvcWaitProcessWideKeyAtomic(c);
    case 0x1D: return kernel::SvcSignalProcessWideKey(c);
    case 0x10: c.x[0] = 0; return;  // GetCurrentProcessorNumber
    case 0x16:  // CloseHandle(W0)
        ipc::CloseHandle(static_cast<uint32_t>(c.x[0]));
        return Return(c, kResultSuccess);
    case 0x11:    // SignalEvent(W0)
    case 0x12:    // ClearEvent(W0)
    case 0x17: {  // ResetSignal(W0)
        const auto event = kernel::GetAs<kernel::Event>(static_cast<uint32_t>(c.x[0]));
        if (!event) return Return(c, kResultInvalidHandle);
        if (id == 0x11) event->Signal();
        else event->Clear();
        return Return(c, kResultSuccess);
    }
    case 0x18: {  // WaitSynchronization(X1 = handles, W2 = quantidade, X3 = timeout em ns) -> W1 = indice
        const auto* handles = reinterpret_cast<const uint32_t*>(c.x[1]);
        const int count = static_cast<int>(c.x[2]);
        std::vector<std::shared_ptr<kernel::Object>> objects;
        for (int i = 0; i < count; ++i) {
            objects.push_back(kernel::Get(handles[i]));
            if (!objects.back()) return Return(c, kResultInvalidHandle);
        }
        const int64_t timeout = static_cast<int64_t>(c.x[3]);
        if (timeout != 0) Log("[svc] WaitSynchronization em %d handle(s), timeout %lld ns", count, static_cast<long long>(timeout));
        const int index = kernel::WaitAny(objects.data(), count, timeout);
        if (index < 0) return Return(c, kResultTimedOut);
        c.x[1] = static_cast<uint64_t>(index);
        return Return(c, kResultSuccess);
    }
    case 0x45: {  // CreateEvent -> W1 = lado de escrita, W2 = lado de leitura (o mesmo objeto aqui)
        auto event = std::make_shared<kernel::Event>();
        c.x[1] = kernel::Insert(event);
        c.x[2] = kernel::Insert(event);
        return Return(c, kResultSuccess);
    }
    case 0x1F: return ipc::ConnectToNamedPort(c);
    case 0x21: return ipc::SendSyncRequest(c);
    case 0x1E: c.x[0] = ReadCounter(); return;  // GetSystemTick
    case 0x24:  // GetProcessId(W1 = handle) -> X1
        c.x[1] = 0x51;
        return Return(c, kResultSuccess);
    case 0x25: return kernel::SvcGetThreadId(c);
    case 0x26: {  // Break(reason, arg, size): o sdk passa o nn::Result do erro como argumento de 4 bytes
        uint32_t value = 0;
        if (c.x[1] && c.x[2] >= 4) std::memcpy(&value, reinterpret_cast<const void*>(c.x[1]), 4);
        Fatal(c, "[svc] Break: reason=0x%llx arg=0x%llx size=0x%llx valor=0x%08x (modulo %u, descricao %u)", static_cast<unsigned long long>(c.x[0]),
              static_cast<unsigned long long>(c.x[1]), static_cast<unsigned long long>(c.x[2]), value, value & 0x1FF, (value >> 9) & 0x1FFF);
    }
    case 0x27: {  // OutputDebugString(ptr, size)
        const std::string text(reinterpret_cast<const char*>(c.x[0]), c.x[1]);
        Log("[svc] OutputDebugString: %s", text.c_str());
        return Return(c, kResultSuccess);
    }
    case 0x29: return GetInfo(c);
    default:
        Fatal(c, "[svc] 0x%02x %s nao implementada (x0=%llx x1=%llx x2=%llx x3=%llx)", id, SvcName(id), static_cast<unsigned long long>(c.x[0]),
              static_cast<unsigned long long>(c.x[1]), static_cast<unsigned long long>(c.x[2]), static_cast<unsigned long long>(c.x[3]));
    }
}

} // namespace recomp
