#pragma once
// Montagem de cenario: percorre a hierarquia .trscn (SubScene, SceneObject, ObjectTemplate, componentes) e
// devolve cada .trmdl / .trcol com a matriz de mundo acumulada — sem depender do Renderer3D
//
// Lumiose City: world/ik_scene/field/area/t1_/t1_0.trscn -> t1/sub_scene/field_/field_0.trscn -> 25 blocos
//   t1/sub_scene/t1_bkNN_/t1_bkNN_0.trscn, cada um com unity_output/t1_gNN_ (chao) e t1_bNN_ (predios).
//   Os .trcol de chao estao no espaco local do tile: o SceneObject "t1_gNN_1" aplica R(0, -pi, 0), o que leva
//   os vertices para o quadrante negativo onde ficam os demais objetos (ex.: t1_b_salon_rouge em -214, -861).
// Sub-cenas sao referenciadas como "x.trscn" e lidas como o bloco "x_0.trscn" (SceneChunkPath).
// Cada cena e carregada uma vez (a primeira referencia define o pai); colisoes "_hicol_col" / "_allchrcol_col"
// (camadas auxiliares) e caixas sem .trcol ficam de fora por padrao.

#include "gfx/trcol.hpp"
#include "gfx/trscn.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace za::gfx {

struct ScenePlacement {
    std::string path;                 // caminho virtual normalizado
    std::array<float, 16> world{};    // column-major
    std::string scene;                // .trscn de origem
    std::string node;                 // SceneObject/ObjectTemplate nomeado mais proximo
};

struct SceneAssemblyOptions {
    unsigned max_depth{32};
    bool auxiliary_collision{false};
    // Filtro opcional de cenas (caminho normalizado); false pula a cena e tudo abaixo dela
    std::function<bool(const std::string& scene_path)> scene_filter;
};

class SceneAssembler {
public:
    // Mesma assinatura do VfsReaderFn do SceneLoader
    using Reader = std::function<bool(const std::string& path, std::vector<uint8_t>& out_bytes)>;

    explicit SceneAssembler(Reader reader, SceneAssemblyOptions options = {});

    // Acumula a cena (e suas sub-cenas) sob a matriz parent; false so se a propria cena falhar
    bool AddScene(std::string_view scene_path, std::string& error);
    bool AddScene(std::string_view scene_path, const std::array<float, 16>& parent, std::string& error);

    const std::vector<ScenePlacement>& Models() const { return models_; }
    const std::vector<ScenePlacement>& Collisions() const { return collisions_; }
    const std::vector<std::string>& Scenes() const { return scenes_; }
    const std::vector<std::string>& MissingScenes() const { return missing_; }
    size_t EntityCount() const { return entities_; }
    size_t BoxCollisionCount() const { return boxes_; }

    // Le cada .trcol posicionado (uma leitura por arquivo), aplica a matriz de mundo menos origin e adiciona ao
    // world; devolve quantas malhas entraram. Falhas individuais vao para error sem interromper.
    size_t BuildCollisionWorld(CollisionWorld& world, std::string& error, const std::array<float, 3>& origin = {0, 0, 0}) const;

private:
    bool LoadScene(const std::string& path, const std::array<float, 16>& parent, unsigned depth, std::string& error);
    void VisitEntity(const SceneEntity& entity, const std::array<float, 16>& parent, const std::string& scene,
                     const std::string& node, unsigned depth);
    std::string Resolve(const std::string& scene, const std::string& reference) const;

    Reader reader_;
    SceneAssemblyOptions options_;
    std::set<std::string> visited_;
    std::vector<std::string> scenes_;
    std::vector<std::string> missing_;
    std::vector<ScenePlacement> models_;
    std::vector<ScenePlacement> collisions_;
    size_t entities_{0};
    size_t boxes_{0};
};

constexpr std::array<float, 16> kIdentityTransform{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

} // namespace za::gfx
