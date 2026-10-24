// Despacho por endereco (BR/BLR e ponteiros de funcao), funcoes nativas e diagnostico

#include "recomp/runtime.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <mutex>
#include <vector>

namespace recomp {

extern const GuestFunction kMainFunctions[];
extern const size_t kMainFunctionsCount;
extern const GuestFunction kSdkFunctions[];
extern const size_t kSdkFunctionsCount;

bool FindModule(uint64_t address, std::string& name, uint64_t& rva);

namespace {

struct Host {
    std::string name;
    HostFunction fn;
};

std::vector<Host>& Hosts() {
    static std::vector<Host> hosts;
    return hosts;
}

thread_local const char* g_current_host = nullptr;  // funcao nativa em execucao (para o Fatal)
std::mutex g_log_mutex;
FILE* g_log = nullptr;

// Tabelas dos modulos, ordenadas por endereco (main antes do sdk)
const GuestFunction* Find(uint64_t address) {
    for (const auto& [table, count] : {std::pair{kMainFunctions, kMainFunctionsCount}, std::pair{kSdkFunctions, kSdkFunctionsCount}}) {
        const GuestFunction* end = table + count;
        const GuestFunction* it = std::lower_bound(table, end, address, [](const GuestFunction& f, uint64_t a) { return f.address < a; });
        if (it != end && it->address == address) return it;
    }
    return nullptr;
}

// Funcao que contem o endereco (a de maior inicio <= address)
const GuestFunction* Containing(uint64_t address) {
    const GuestFunction* best = nullptr;
    for (const auto& [table, count] : {std::pair{kMainFunctions, kMainFunctionsCount}, std::pair{kSdkFunctions, kSdkFunctionsCount}}) {
        const GuestFunction* end = table + count;
        const GuestFunction* it = std::upper_bound(table, end, address, [](uint64_t a, const GuestFunction& f) { return a < f.address; });
        if (it != table && (!best || (it - 1)->address > best->address)) best = it - 1;
    }
    return best;
}

void VLog(const char* format, va_list args) {
    std::lock_guard lock(g_log_mutex);
    if (!g_log) g_log = std::fopen("za_recomp.log", "w");
    va_list copy;
    va_copy(copy, args);
    std::vfprintf(stderr, format, args);
    std::fputc('\n', stderr);
    if (g_log) {
        std::vfprintf(g_log, format, copy);
        std::fputc('\n', g_log);
        std::fflush(g_log);
    }
    va_end(copy);
}

}  // namespace

uint64_t RegisterHost(const std::string& name, HostFunction fn) {
    Hosts().push_back({name, fn});
    return kHleBase + kHleStride * (Hosts().size() - 1);
}

void Call(Context& c, uint64_t address) {
    if (address >= kHleBase && address < kHleBase + kHleStride * Hosts().size() && (address - kHleBase) % kHleStride == 0) {
        const Host& h = Hosts()[(address - kHleBase) / kHleStride];
        const char* previous = g_current_host;
        g_current_host = h.name.c_str();
        h.fn(c);
        g_current_host = previous;
        return;
    }
    if (const GuestFunction* f = Find(address)) {
        f->fn(c);
        return;
    }
    Fatal(c, "salto indireto para endereco sem funcao traduzida: %s", DescribeAddress(address).c_str());
}

void Dispatch(Context& c, uint64_t guest_address) { Call(c, guest_address); }

std::string DescribeAddress(uint64_t address) {
    if (address >= kHleBase && address < kHleBase + kHleStride * Hosts().size()) {
        return "hle:" + Hosts()[(address - kHleBase) / kHleStride].name;
    }
    std::string module;
    uint64_t rva = 0;
    if (!FindModule(address, module, rva)) return std::format("0x{:x}", address);
    std::string out = std::format("{}+0x{:x}", module, rva);
    if (const GuestFunction* f = Containing(address); f && address - f->address < 0x100000) {
        out += std::format(" (fn 0x{:x}+0x{:x})", f->address, address - f->address);
    }
    return out;
}

void Log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    VLog(format, args);
    va_end(args);
}

void Fatal(const Context& c, const char* format, ...) {
    va_list args;
    va_start(args, format);
    VLog(format, args);
    va_end(args);
    if (g_current_host) Log("  funcao nativa: %s", g_current_host);
    Log("  LR = %s", DescribeAddress(c.x[30]).c_str());
    Log("  x0..x7 = %llx %llx %llx %llx %llx %llx %llx %llx", static_cast<unsigned long long>(c.x[0]), static_cast<unsigned long long>(c.x[1]),
        static_cast<unsigned long long>(c.x[2]), static_cast<unsigned long long>(c.x[3]), static_cast<unsigned long long>(c.x[4]),
        static_cast<unsigned long long>(c.x[5]), static_cast<unsigned long long>(c.x[6]), static_cast<unsigned long long>(c.x[7]));
    std::exit(3);
}

void MissingFunction(Context& c, uint64_t address) { Fatal(c, "chamada a endereco que nao e inicio de funcao: %s", DescribeAddress(address).c_str()); }

void Unimplemented(Context& c, uint64_t address, uint32_t word) {
    Fatal(c, "instrucao nao traduzida 0x%08x em %s", word, DescribeAddress(address).c_str());
}

void Trap(Context& c, uint64_t address, uint32_t imm) { Fatal(c, "BRK/UDF #%u em %s", imm, DescribeAddress(address).c_str()); }

} // namespace recomp
