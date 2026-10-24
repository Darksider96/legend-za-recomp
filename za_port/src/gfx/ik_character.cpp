#include "gfx/ik_character.hpp"

#include "gfx/texture_codec.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

namespace za::gfx {

namespace {

constexpr uint32_t kWrapMirror = 6;
constexpr uint32_t kMaxBakeSize = 4096;

float SrgbToLinear(float value) {
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

// Linear -> sRGB 8 bits por tabela (4096 degraus cobrem a precisao de 8 bits da saida)
uint8_t LinearToSrgb8(float value) {
    static const std::array<uint8_t, 4097> table = [] {
        std::array<uint8_t, 4097> values{};
        for (size_t i = 0; i < values.size(); ++i) {
            const float linear = static_cast<float>(i) / 4096.0f;
            const float encoded = linear <= 0.0031308f ? linear * 12.92f : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
            values[i] = static_cast<uint8_t>(std::lround(std::clamp(encoded, 0.0f, 1.0f) * 255.0f));
        }
        return values;
    }();
    return table[static_cast<size_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 4096.0f))];
}

// Imagem RGBA em float com amostragem bilinear; repeticao simples ou espelhada por eixo
struct FloatImage {
    uint32_t width{0}, height{0};
    std::vector<float> texels;

    static float Fold(float coordinate, bool mirror) {
        if (!mirror) return coordinate - std::floor(coordinate);
        const float period = coordinate - 2.0f * std::floor(coordinate * 0.5f);
        return period < 1.0f ? period : 2.0f - period;
    }

    static size_t Texel(float index, uint32_t size, bool mirror) {
        const auto n = static_cast<int64_t>(size);
        const auto i = static_cast<int64_t>(index);
        return static_cast<size_t>(mirror ? std::clamp<int64_t>(i, 0, n - 1) : ((i % n) + n) % n);
    }

    void Sample(float u, float v, bool mirror_u, bool mirror_v, float out[4]) const {
        const float x = Fold(u, mirror_u) * static_cast<float>(width) - 0.5f;
        const float y = Fold(v, mirror_v) * static_cast<float>(height) - 0.5f;
        const float fx = std::floor(x), fy = std::floor(y);
        const float tx = x - fx, ty = y - fy;
        const size_t x0 = Texel(fx, width, mirror_u), x1 = Texel(fx + 1, width, mirror_u);
        const size_t y0 = Texel(fy, height, mirror_v), y1 = Texel(fy + 1, height, mirror_v);
        for (size_t c = 0; c < 4; ++c) {
            const float top = texels[(y0 * width + x0) * 4 + c] * (1 - tx) + texels[(y0 * width + x1) * 4 + c] * tx;
            const float bottom = texels[(y1 * width + x0) * 4 + c] * (1 - tx) + texels[(y1 * width + x1) * 4 + c] * tx;
            out[c] = top * (1 - ty) + bottom * ty;
        }
    }
};

FloatImage ToFloat(const BntxTexture& texture, std::span<const uint8_t> rgba, bool linearize_rgb) {
    FloatImage image{texture.width, texture.height, std::vector<float>(rgba.size())};
    float lut[256];
    for (int i = 0; i < 256; ++i) {
        const float value = static_cast<float>(i) / 255.0f;
        lut[i] = linearize_rgb ? SrgbToLinear(value) : value;
    }
    for (size_t i = 0; i < rgba.size(); ++i) {
        image.texels[i] = (i % 4 == 3) ? static_cast<float>(rgba[i]) / 255.0f : lut[rgba[i]];
    }
    return image;
}

const std::array<float, 4>* FindVector(const Material& material, std::string_view name) {
    for (const auto& [key, value] : material.vectors) {
        if (key == name) return &value;
    }
    return nullptr;
}

} // namespace

IkCharacterBakeSettings BakeSettingsFromMaterial(const Material& material, std::vector<std::array<float, 4>>& color_layers) {
    IkCharacterBakeSettings settings;
    color_layers.clear();
    for (int layer = 1; layer <= 4; ++layer) {
        const std::array<float, 4>* color = FindVector(material, "BaseColorLayer" + std::to_string(layer));
        color_layers.push_back(color ? *color : std::array<float, 4>{0, 0, 0, 1});
    }
    if (const auto* transform = FindVector(material, "UVScaleOffset")) settings.mask_uv_transform = *transform;
    if (const auto* base = FindVector(material, "BaseColor")) settings.base_color = *base;
    if (const MaterialTexture* mask = material.Find("LayerMaskMap"); mask && mask->sampler < material.samplers.size()) {
        settings.mirror_mask_u = material.samplers[mask->sampler].wrap_u == kWrapMirror;
        settings.mirror_mask_v = material.samplers[mask->sampler].wrap_v == kWrapMirror;
    }
    for (const auto& [key, value] : material.floats) {
        if (key.starts_with("LayerMaskScale") && key.size() == 15 && key[14] >= '1' && key[14] <= '4') {
            settings.mask_scale[static_cast<size_t>(key[14] - '1')] = value;
        }
    }
    for (const auto& [key, value] : material.options) {
        if (key == "NumMaterialLayer") {
            settings.layer_count = static_cast<uint32_t>(std::clamp(std::atoi(value.c_str()) - 1, 0, 4));
        } else if (key == "BaseColorMultiply") {
            settings.multiply = value != "False";
        }
    }
    return settings;
}

std::array<uint32_t, 2> BakeOutputSize(const BntxTexture& base_color_tex, const BntxTexture& layer_mask_tex,
                                       const IkCharacterBakeSettings& settings) {
    const auto scaled = [](uint32_t size, float scale) {
        return static_cast<uint32_t>(std::ceil(static_cast<float>(size) * std::max(1.0f, std::abs(scale))));
    };
    return {std::min(kMaxBakeSize, std::max(base_color_tex.width, scaled(layer_mask_tex.width, settings.mask_uv_transform[0]))),
            std::min(kMaxBakeSize, std::max(base_color_tex.height, scaled(layer_mask_tex.height, settings.mask_uv_transform[1])))};
}

bool BakeIkCharacterTexture(const BntxTexture& base_color_tex, std::span<const uint8_t> base_color_pixels,
                            const BntxTexture& layer_mask_tex, std::span<const uint8_t> layer_mask_pixels,
                            const std::vector<std::array<float, 4>>& color_layers, std::vector<uint8_t>& out_rgba,
                            std::string& error, const IkCharacterBakeSettings& settings) {
    out_rgba.clear();
    if (base_color_pixels.size() != size_t{base_color_tex.width} * base_color_tex.height * 4 ||
        layer_mask_pixels.size() != size_t{layer_mask_tex.width} * layer_mask_tex.height * 4 ||
        base_color_tex.width == 0 || layer_mask_tex.width == 0) {
        error = "pixels RGBA8 nao conferem com as dimensoes das texturas";
        return false;
    }

    const FloatImage base = ToFloat(base_color_tex, base_color_pixels, IsSrgb(base_color_tex.format));
    const FloatImage mask = ToFloat(layer_mask_tex, layer_mask_pixels, false);
    const auto [width, height] = BakeOutputSize(base_color_tex, layer_mask_tex, settings);

    // Camadas ativas (cores lineares do material)
    struct Layer {
        size_t channel;
        float scale;
        std::array<float, 3> color;
    };
    std::vector<Layer> layers;
    const size_t count = std::min<size_t>({settings.layer_count, color_layers.size(), 4});
    for (size_t i = 0; i < count; ++i) {
        const auto& color = color_layers[i];
        if (settings.skip_unset_layers && color[0] == 0.0f && color[1] == 0.0f && color[2] == 0.0f) continue;
        layers.push_back({i, settings.mask_scale[i], {color[0], color[1], color[2]}});
    }

    const auto& transform = settings.mask_uv_transform;
    out_rgba.resize(size_t{width} * height * 4);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
            float m[4], b[4];
            base.Sample(u, v, false, false, b);
            mask.Sample(u * transform[0] + transform[2], v * transform[1] + transform[3], settings.mirror_mask_u,
                        settings.mirror_mask_v, m);

            float sum = 0, mixed[3] = {0, 0, 0};
            for (const Layer& layer : layers) {
                const float weight = std::clamp(m[layer.channel] * layer.scale, 0.0f, 1.0f);
                sum += weight;
                for (size_t c = 0; c < 3; ++c) mixed[c] += weight * layer.color[c];
            }
            const float normalize = sum > 1.0f ? 1.0f / sum : 1.0f;
            const float neutral = 1.0f - std::min(sum, 1.0f);

            uint8_t* out = out_rgba.data() + (size_t{y} * width + x) * 4;
            for (size_t c = 0; c < 3; ++c) {
                const float tint = mixed[c] * normalize;
                const float color = settings.multiply ? b[c] * (tint + neutral) : b[c] * neutral + tint;
                out[c] = LinearToSrgb8(color * settings.base_color[c]);
            }
            out[3] = static_cast<uint8_t>(std::lround(std::clamp(b[3] * settings.base_color[3], 0.0f, 1.0f) * 255.0f));
        }
    }
    return true;
}

} // namespace za::gfx
