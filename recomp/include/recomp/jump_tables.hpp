#pragma once
// Tabelas de salto (switch) resolvidas estaticamente: BR -> destinos dentro da mesma funcao

#include "recomp/a64.hpp"
#include "recomp/nso.hpp"

#include <map>
#include <vector>

namespace recomp {

// Para cada BR de tabela de salto reconhecido na funcao: os destinos distintos (RVAs), em ordem
std::map<uint64_t, std::vector<uint64_t>> ResolveJumpTables(const NsoModule& module, const FunctionRange& function, const std::vector<a64::Insn>& insns);

} // namespace recomp
