// Implementacoes provisorias do runtime para testes da fase R2: qualquer chamada indireta, instrucao nao traduzida
// ou BRK interrompe o processo com o endereco. O runtime real (R4) substitui este arquivo.

#include "recomp/context.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace recomp {

void Dispatch(Context&, uint64_t guest_address) {
    std::fprintf(stderr, "[recomp] Dispatch para 0x%llx ainda nao implementado\n", static_cast<unsigned long long>(guest_address));
    std::abort();
}

void Unimplemented(Context&, uint64_t rva, uint32_t word) {
    std::fprintf(stderr, "[recomp] instrucao nao traduzida em RVA 0x%llx (0x%08x)\n", static_cast<unsigned long long>(rva), word);
    std::abort();
}

void Trap(Context&, uint64_t rva, uint32_t imm) {
    std::fprintf(stderr, "[recomp] BRK #%u em RVA 0x%llx\n", imm, static_cast<unsigned long long>(rva));
    std::abort();
}

uint64_t ReadCounter() {
    const auto ns = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(ns).count()) * 192 / 10000;  // 19,2 MHz
}

} // namespace recomp
