#pragma once
// Composicao ("bake") do shader IkCharacter: a cor dos Pokemon vem de LayerMaskMap (RGBA = camadas 1..4) x
// BaseColorLayer1..4 do .trmtr; o BaseColorMap e quase branco. Formula (espaco linear, por pixel):
//   pesos w_i = clamp(mask_i * LayerMaskScale_i), soma S = sum(w_i)
//   tinta = sum(w_i * Layer_i.rgb) / max(1, S) + (1 - min(1, S))      (areas sem mascara mantem o BaseColor)
//   final = BaseColor.rgb * fator BaseColor * tinta                    (BaseColorMultiply = True)
// Mapeamento validado em render (Wartortle: carapaca/plastrao; Pikachu: listras simetricas nas costas):
//   BaseColorMap em UV0 puro; LayerMaskMap em UV0 * UVScaleOffset.xy + UVScaleOffset.zw com o modo de repeticao
//   do sampler da mascara (6 = espelhado, usado em U: os dois lados do modelo compartilham a mascara).
// Camadas com cor exatamente (0,0,0) sao "nao configuradas" e nao pesam (ex.: Wartortle body_b_00);
// cores escuras intencionais usam valores nao nulos (pupilas = 0.005).

#include "gfx/bntx.hpp"
#include "gfx/trmdl.hpp"

#include <array>
#include <span>
#include <string>
#include <vector>

namespace za::gfx {

struct IkCharacterBakeSettings {
    std::array<float, 4> mask_uv_transform{1, 1, 0, 0};  // UVScaleOffset: escala xy, deslocamento zw (LayerMaskMap)
    bool mirror_mask_u{false};                           // sampler da mascara com repeticao espelhada
    bool mirror_mask_v{false};
    std::array<float, 4> base_color{1, 1, 1, 1};         // vetor "BaseColor"
    std::array<float, 4> mask_scale{1, 1, 1, 1};         // LayerMaskScale1..4
    uint32_t layer_count{4};                             // NumMaterialLayer - 1 (maximo 4)
    bool multiply{true};                                 // BaseColorMultiply
    bool skip_unset_layers{true};
};

// Le BaseColorLayer1..4, UVScaleOffset, sampler da LayerMaskMap, BaseColor, LayerMaskScale*, NumMaterialLayer e
// BaseColorMultiply
IkCharacterBakeSettings BakeSettingsFromMaterial(const Material& material, std::vector<std::array<float, 4>>& color_layers);

// Resultado no espaco de UV0 da malha: max(BaseColorMap, mascara x escala do UVScaleOffset), limitado a 4096
std::array<uint32_t, 2> BakeOutputSize(const BntxTexture& base_color_tex, const BntxTexture& layer_mask_tex,
                                       const IkCharacterBakeSettings& settings = {});

// Pixels de entrada: RGBA8 do mip 0 (DecodeTexture). Saida: RGBA8 em sRGB com tamanho BakeOutputSize.
bool BakeIkCharacterTexture(const BntxTexture& base_color_tex, std::span<const uint8_t> base_color_pixels,
                            const BntxTexture& layer_mask_tex, std::span<const uint8_t> layer_mask_pixels,
                            const std::vector<std::array<float, 4>>& color_layers, std::vector<uint8_t>& out_rgba,
                            std::string& error, const IkCharacterBakeSettings& settings = {});

} // namespace za::gfx
