#pragma once
// Partes internas do gerador de C++ compartilhadas entre emit_c.cpp (inteiros) e emit_simd.cpp (SIMD/FP)

#include "recomp/a64.hpp"

#include <string>

namespace recomp::emit {

// Leitura de registrador geral: 31 = XZR (zero) ou SP, conforme a instrucao
std::string Reg(unsigned n, bool sf, bool sp = false);
// Escrita: 32 bits zeram a parte alta; XZR descarta (mas avalia a expressao, que pode atualizar flags)
std::string Write(unsigned n, bool sf, const std::string& value, bool sp = false);

// Instrucao SIMD/FP (exceto LDR/STR/LDP/STP de V, tratados junto dos inteiros); false se nao suportada
bool Simd(const a64::Insn& d, std::string& code);

} // namespace recomp::emit
