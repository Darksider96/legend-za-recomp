#pragma once
// Decodificacao de texturas (BC1/BC3/BC4/BC5/BC7 e formatos RGBA) para RGBA8 e gravacao de PNG

#include "gfx/bntx.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace za::gfx {

// Decodifica o mip 0 da camada 0 (dados lineares de DeswizzleTexture) em RGBA8, width x height.
// Formatos SRGB saem com os bytes como gravados (codificados em sRGB); BC4 replica R em RGB; BC5 grava B = 0.
bool DecodeTexture(const BntxTexture& texture, std::span<const uint8_t> linear, std::vector<uint8_t>& out_rgba,
                   std::string& error);

// Deswizzle + DecodeTexture direto do arquivo BNTX
bool DecodeBntxTexture(std::span<const uint8_t> file, const BntxTexture& texture, std::vector<uint8_t>& out_rgba,
                       std::string& error);

bool IsSrgb(const BntxFormat& format);

// PNG sem compressao (deflate "stored"): channels = 1 (cinza), 3 (RGB) ou 4 (RGBA)
bool EncodePng(uint32_t width, uint32_t height, uint32_t channels, std::span<const uint8_t> pixels,
               std::vector<uint8_t>& out, std::string& error);

} // namespace za::gfx
