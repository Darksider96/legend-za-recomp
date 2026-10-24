#pragma once
// Runtime do recomp (R4): tabela de despacho, funcoes nativas (HLE), diagnostico
//
// Modulos traduzidos: main em kImageBase e sdk em kSdkBase (o rtld nao e traduzido: o carregador do runtime faz
// o papel dele). Funcoes nativas recebem enderecos falsos em kHleBase; o GOT e os ponteiros de funcao do jogo
// apontam para eles e o Dispatch os reconhece.

#include "recomp/simd.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace recomp {

struct GuestFunction {
    uint64_t address;            // endereco absoluto do jogo
    void (*fn)(Context&);
};

inline constexpr uint64_t kSdkBase = 0x7108000000ull;   // depois do main (0x7100000000 + 0x5F7A000)
inline constexpr uint64_t kHleBase = 0x7F00000000ull;
inline constexpr uint64_t kHleStride = 16;

using HostFunction = void (*)(Context&);

// Registra uma funcao nativa e devolve o endereco falso pelo qual o jogo a chama
uint64_t RegisterHost(const std::string& name, HostFunction fn);

// Chama a funcao do jogo (ou nativa) que comeca em address; o retorno volta para quem chamou
void Call(Context& c, uint64_t address);

// "main+0x1234 (f_1200+0x34)", "sdk+0x...", "hle:nome" — para logs
std::string DescribeAddress(uint64_t address);

void Log(const char* format, ...) __attribute__((format(printf, 1, 2)));
[[noreturn]] void Fatal(const Context& c, const char* format, ...) __attribute__((format(printf, 2, 3)));
[[noreturn]] void MissingFunction(Context& c, uint64_t address);

} // namespace recomp
