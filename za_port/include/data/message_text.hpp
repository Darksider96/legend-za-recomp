#pragma once
// Textos do jogo (ik_message/dat/<idioma>/.../*.dat) — mesmo formato cifrado da Gen 8/9
//
// .dat: u16 n_idiomas (1); u16 n_linhas; u32 tamanho da secao; u32 0; u32 offset da secao
//   secao: u32 tamanho; [{ u32 offset (relativo a secao); u16 caracteres; u16 flags }] x n_linhas; UTF-16LE
//   cada linha e cifrada com XOR de 16 bits: chave inicial 0x7C89 + 0x2983 * linha, rotacionada 3 bits a
//   esquerda a cada caractere. O .tbl (AHTB) ao lado so mapeia hashes de rotulos ("MONSNAME_001").
// monsname.dat e indexado pelo id interno da especie (linha 1000 = Tinkatink).

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace za::data {

// Linhas decodificadas em UTF-8 (sem o terminador)
bool ParseMessageText(std::span<const uint8_t> data, std::vector<std::string>& out_lines, std::string& error);

} // namespace za::data
