#pragma once
// Animacoes esqueleticas da Trinity Engine (.tranm) e descritores de conjunto de animacao (.tracn)
//
// .tranm { 0: info {0: loop, 1: quadros, 2: fps}, 1: {0: [BoneTrack]} }
//   BoneTrack { 0: nome do osso (= no do .trskl), 1/2: escala, 3/4: rotacao, 5/6: translacao }
//     cada canal e uma union FlatBuffer (campo de tipo u8 + tabela):
//       1 = Fixed {0: valor}, 2 = Dynamic {0: [valor por quadro]},
//       3 = Framed16 {0: [u16 quadro], 1: [valor]}, 4 = Framed8 {0: [u8 quadro], 1: [valor]}
//     escala/translacao: vec3 f32; rotacao: quaternion "smallest three" em 3 x u16 (48 bits):
//       bits 0-1 = indice do maior componente, bit 2 = sinal, bits 3-17 / 18-32 / 33-47 = demais componentes
//       (15 bits, v = q * (pi/2) / 32767 - pi/4; faixa conferida nos dados: 1638..31133)
// .tracm (curvas de material/visibilidade por mesh shape) e .traef (efeitos) acompanham o mesmo clipe.
// .tracn { 0: [{0: chave, 1: arquivo}] } — ex.: "parameter" -> .tracp, "resource" -> .tracr

#include "gfx/trmdl.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace za::gfx {

using Vec3f = std::array<float, 3>;
using Quat = std::array<float, 4>;  // x, y, z, w

enum class AnimationChannelKind : uint8_t { None = 0, Fixed = 1, Dynamic = 2, Framed16 = 3, Framed8 = 4 };

template <typename T>
struct AnimationKey {
    float frame{0};
    T value{};
};

template <typename T>
struct AnimationChannel {
    AnimationChannelKind kind{AnimationChannelKind::None};
    std::vector<AnimationKey<T>> keys;  // em ordem de quadro; uma chave = valor constante

    bool Empty() const { return keys.empty(); }
    bool Animated() const { return keys.size() > 1; }
};

struct AnimationTrack {
    std::string bone;
    AnimationChannel<Vec3f> scale;
    AnimationChannel<Quat> rotation;
    AnimationChannel<Vec3f> translation;
};

struct AnimationClip {
    std::string name;
    bool loop{false};
    uint32_t frame_count{0};
    uint32_t fps{30};
    std::vector<AnimationTrack> tracks;

    float DurationSeconds() const {
        return frame_count > 1 && fps ? static_cast<float>(frame_count - 1) / static_cast<float>(fps) : 0.0f;
    }
};

struct AnimationSetEntry {
    std::string key;
    std::string file;
};

bool ParseTranm(std::span<const uint8_t> data, AnimationClip& out, std::string& error);
bool ParseTracn(std::span<const uint8_t> data, std::vector<AnimationSetEntry>& out, std::string& error);

Quat UnpackQuaternion48(uint16_t x, uint16_t y, uint16_t z);

// Interpola as trilhas (linear para vetores, slerp para rotacoes) no instante dado — com repeticao se o clipe
// for loop — aplica nos nos de mesmo nome e recalcula as matrizes globais e de skinning
void EvaluateAnimation(const AnimationClip& clip, float time_seconds, Skeleton& inout_skeleton);

} // namespace za::gfx
