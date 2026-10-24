#pragma once
// Gerador de C++ a partir de funcoes AArch64 decodificadas
//
// Cada funcao do jogo vira `void f_<rva>(recomp::Context& c)` em `namespace guest`. Saltos internos viram goto,
// chamadas (BL) e saltos para fora viram chamadas diretas a outras f_<rva>; BR/BLR passam por recomp::Dispatch.
// Instrucoes nao suportadas geram recomp::Unimplemented(c, pc, word) e ficam listadas no resultado.

#include "recomp/a64.hpp"
#include "recomp/nso.hpp"

#include <set>
#include <string>
#include <vector>

namespace recomp {

struct EmitResult {
    std::string code;                       // corpo da funcao (sem cabecalho de arquivo)
    std::vector<a64::Insn> unsupported;     // instrucoes que viraram Unimplemented
    std::set<uint64_t> callees;             // RVAs de funcoes chamadas ou alvo de tail call
    size_t instructions{0};
};

std::string FunctionName(uint64_t rva);
// name: nome da funcao gerada (padrao FunctionName(function.start)); chamadas continuam usando FunctionName
bool EmitFunction(const NsoModule& module, const FunctionRange& function, EmitResult& out, const std::string& name = {});
// Arquivo completo: include do runtime, declaracoes das funcoes chamadas e os corpos
std::string EmitFile(const std::vector<EmitResult>& functions);

} // namespace recomp
