#pragma once
// Interface entre os testes por instrucao gerados (a64_insn_tests) e o executor insn_difftest

#include "recomp/context.hpp"

#include <cstddef>
#include <cstdint>

namespace recomp::test {

struct InsnTest {
    uint64_t rva;
    void (*fn)(recomp::Context&);
};

extern const InsnTest kInsnTests[];
extern const size_t kInsnTestCount;

// Registra a primeira saida da instrucao (endereco do jogo); chamada pelos stubs e pelo Dispatch
void Exit(uint64_t guest_address);

}  // namespace recomp::test
