#pragma once
// Cenas da Trinity Engine (.trscn) e buffers de instancias (.trins) — FlatBuffers
//
// .trscn { 0: nome, 1: camada ("---- Default ----"), 4: [Entity] raizes, 5: [string] sub-cenas relativas, 6: bool }
//   Entity { 0: tipo ("trinity_SceneObject", "SubScene", "trinity_ModelComponent"...), 1: [u8] FlatBuffer
//            aninhado com os dados do tipo, 2: [Entity] filhos (componentes e objetos) }
//   trinity_SceneObject { 0: nome, 1: SRT {0: escala vec3, 1: rotacao Euler XYZ em radianos, 2: translacao} }
//   trinity_ScenePoint { 0: nome, 1: posicao vec3 }
//   trinity_ModelComponent { 0: .trmdl, 1: nome }   SubScene { 0: .trscn relativo }
//   trinity_ObjectTemplate { 0: nome, 2: .trsot, 5: [u8] SceneObject aninhado (SRT do objeto) }
//   trinity_CollisionComponent { 0: u8 tipo; 1: { 1: forma { 0: .trcol, 1: T, 2: R, 3: S } } } (tipo 1; sem
//     .trcol e uma caixa). Os SRT de entidades sao relativos ao pai: o mundo e o produto pela hierarquia.
//   Os demais componentes (colisao, camera, luz, script...) tem os caminhos de assets extraidos por varredura
//   de strings FlatBuffer com extensao no buffer aninhado.
// .trins { 0: "SrtInstanceBuffer", 1: u32 (1), 2: u32 instancias, 4: {0: [u8] instancias x mat4 f32} }
//   matrizes column-major (translacao nos elementos 12..14)

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace za::gfx {

struct SceneTransform {
    std::array<float, 3> scale{1, 1, 1};
    std::array<float, 3> rotation{0, 0, 0};     // Euler XYZ (radianos), mesma convencao do .trskl
    std::array<float, 3> translation{0, 0, 0};
};

struct SceneEntity {
    std::string type;
    std::string name;
    bool has_transform{false};
    SceneTransform transform;
    std::vector<std::string> assets;            // caminhos como gravados (podem ser relativos a cena)
    std::vector<SceneEntity> children;
    uint32_t data_size{0};                      // bytes do FlatBuffer aninhado
};

struct Scene {
    std::string name;
    std::string layer;
    std::vector<SceneEntity> entities;
    std::vector<std::string> sub_scenes;

    // Percorre todas as entidades (pre-ordem) com a profundidade
    template <typename Visitor>
    void Visit(Visitor&& visit) const {
        for (const SceneEntity& entity : entities) VisitEntity(entity, 0, visit);
    }

private:
    template <typename Visitor>
    static void VisitEntity(const SceneEntity& entity, int depth, Visitor& visit) {
        visit(entity, depth);
        for (const SceneEntity& child : entity.children) VisitEntity(child, depth + 1, visit);
    }
};

struct InstanceBuffer {
    std::string type;
    std::vector<std::array<float, 16>> matrices;
};

bool ParseTrscn(std::span<const uint8_t> data, Scene& out, std::string& error);
bool ParseTrins(std::span<const uint8_t> data, InstanceBuffer& out, std::string& error);

// As cenas referenciam "<dir>/<nome>.trscn" (ou .trsot, template de objeto), mas o pacote guarda os blocos
// "<nome>_<n>.trscn" (todos os 173 .trscn conhecidos sao o bloco 0): "a/b.trscn" -> "a/b_0.trscn";
// outros caminhos voltam inalterados
std::string SceneChunkPath(std::string_view scene_path, unsigned chunk = 0);

// Matriz 4x4 column-major T * R * S, com R = Rz * Ry * Rx (Euler XYZ extrinseco, como no .trskl)
std::array<float, 16> ComposeTransform(const SceneTransform& srt);
// a * b (column-major): aplica b e depois a
std::array<float, 16> MultiplyTransforms(const std::array<float, 16>& a, const std::array<float, 16>& b);

} // namespace za::gfx
