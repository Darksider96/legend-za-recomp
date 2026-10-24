#pragma once
// Modelos da Trinity Engine (.trmdl, .trmsh, .trmbf, .trmtr, .trskl) — parser e exportacao glTF 2.0 / OBJ
//
// .trmdl { 1: [{0: arquivo .trmsh}] (um por LOD), 2: {0: arquivo .trskl}, 3: [arquivo .trmtr],
//          4: [{0: [{0: malha}], 1: tipo}] LODs, 5: {0: min vec3, 1: max vec3} }
// .trmsh { 1: [MeshShape], 2: arquivo .trmbf }
//   MeshShape { 0: nome, 1: bounds, 2: indices (0 = u8, 1 = u16, 2 = u32),
//               3: [{0: [{1: semantica, 2: camada, 3: formato, 4: offset}], 1: [{0: stride}]}] (um por stream),
//               4: [{0: qtd de indices, 1: primeiro indice, 3: material}], 12: nome da malha }
// .trmbf { 1: [{0: [{0: [u8] indices}], 1: [{0: [u8] vertices}]}] } — um por MeshShape, um buffer por stream
// .trmtr { 1: [{0: nome, 1: [{0: shader, 1: [{0: opcao, 1: valor}]}], 2: [{0: slot, 1: arquivo .bntx}],
//               4: [{0: nome, 1: f32}], 7: [{0: nome, 1: vec4}], 9: [{0: nome, 1: u32}]}] }
//   A cor final dos Pokemon (shader IkCharacter) vem de LayerMaskMap x BaseColorLayer1..4; o BaseColorMap e
//   quase branco. Os parametros seguem para o glTF em materials[].extras.trinity.
// .trskl { 1: [{0: nome, 1: {0: escala, 1: rotacao Euler XYZ extrinseca (rad), 2: translacao}, 4: pai, 5: osso}],
//          2: [{2: inverse bind matrix 4x3 em colunas}] } — BLEND_INDICES indexam os ossos

#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace za::gfx {

enum class VertexSemantic : uint32_t {
    Position = 1, Normal = 2, Tangent = 3, Binormal = 4, Color = 5, TexCoord = 6, BlendIndices = 7, BlendWeights = 8,
};

struct VertexAttribute {
    uint32_t semantic{0};
    uint32_t layer{0};
    uint32_t format{0};   // 20 RGBA8_UNORM, 22 RGBA8_UINT, 39 RGBA16_UNORM, 43 RGBA16_FLOAT, 48 RG32F, 51 RGB32F, 54 RGBA32F
    uint32_t offset{0};
    uint32_t stream{0};   // buffer de vertices do atributo
};

struct Submesh {
    uint32_t index_count{0};
    uint32_t index_offset{0};
    std::string material;
};

struct MeshShape {
    std::string name;
    std::vector<VertexAttribute> attributes;
    std::vector<uint32_t> strides;  // por stream
    uint32_t index_format{1};
    std::vector<Submesh> submeshes;

    // Preenchidos por LoadTrmbf
    uint32_t vertex_count{0};
    std::vector<float> positions;     // xyz
    std::vector<float> normals;       // xyz
    std::vector<float> tangents;      // xyzw
    std::vector<float> texcoords[2];  // uv das camadas 0 e 1
    std::vector<float> colors;        // rgba
    std::vector<uint16_t> joints;     // 4 ossos por vertice
    std::vector<float> weights;       // 4 pesos por vertice
    std::vector<uint32_t> indices;
};

struct MaterialTexture {
    std::string slot;      // "BaseColorMap", "NormalMap", ...
    std::string file;      // relativo ao diretorio do modelo
    uint32_t sampler{0};   // indice em Material::samplers
};

// Modos de repeticao do sampler (campos 9/10 do .trmtr): 6 = espelhado, 1 = repeticao
struct MaterialSampler {
    uint32_t wrap_u{0};
    uint32_t wrap_v{0};
};

struct Material {
    std::string name;
    std::string shader;                                                  // ex.: "IkCharacter"
    std::vector<std::pair<std::string, std::string>> options;            // flags do shader ("NumMaterialLayer"...)
    std::vector<MaterialTexture> textures;
    std::vector<MaterialSampler> samplers;                               // campo 3
    std::vector<std::pair<std::string, float>> floats;                   // campo 4
    std::vector<std::pair<std::string, std::array<float, 4>>> vectors;   // campo 7 ("BaseColorLayer1"...)
    std::vector<std::pair<std::string, uint32_t>> ints;                  // campo 9

    const MaterialTexture* Find(std::string_view slot) const;
};

struct SkeletonNode {
    std::string name;
    int32_t parent{-1};
    int32_t bone{-1};  // indice usado por BLEND_INDICES; -1 = no sem skinning
    std::array<float, 3> scale{1, 1, 1};
    std::array<float, 3> rotation{0, 0, 0};         // pose de repouso do .trskl (Euler XYZ)
    std::array<float, 3> translation{0, 0, 0};
    std::array<float, 4> orientation{0, 0, 0, 1};   // pose atual (quaternion x,y,z,w); EvaluateAnimation altera
};

struct Skeleton {
    std::vector<SkeletonNode> nodes;
    std::vector<std::array<float, 16>> inverse_bind;  // por osso, 4x4 coluna-major
    std::vector<std::array<float, 16>> world;         // por no, pose atual (UpdateSkeletonMatrices)
    std::vector<std::array<float, 16>> skinning;      // por osso: world(no do osso) * inverse_bind

    bool Empty() const { return nodes.empty() || inverse_bind.empty(); }
};

std::array<float, 4> EulerXyzToQuaternion(const std::array<float, 3>& euler);

// Recalcula world e skinning a partir de scale/orientation/translation dos nos
void UpdateSkeletonMatrices(Skeleton& skeleton);

struct TrmdlFile {
    std::vector<std::string> mesh_files;  // um .trmsh por LOD (0 = mais detalhado)
    std::string skeleton_file;
    std::vector<std::string> material_files;
    std::array<float, 3> bounds_min{};
    std::array<float, 3> bounds_max{};
};

struct Model {
    std::string name;
    std::vector<MeshShape> meshes;
    std::vector<Material> materials;
    Skeleton skeleton;
};

bool ParseTrmdl(std::span<const uint8_t> data, TrmdlFile& out, std::string& error);
bool ParseTrmsh(std::span<const uint8_t> data, std::vector<MeshShape>& out, std::string& buffer_file, std::string& error);
// Decodifica vertices e indices de cada MeshShape (mesma ordem do .trmsh)
bool LoadTrmbf(std::span<const uint8_t> data, std::vector<MeshShape>& meshes, std::string& error);
bool ParseTrmtr(std::span<const uint8_t> data, std::vector<Material>& out, std::string& error);
bool ParseTrskl(std::span<const uint8_t> data, Skeleton& out, std::string& error);

// texture_uris: arquivo de textura como aparece no material -> URI relativa da imagem convertida (.dds ou .png)
using TextureUris = std::map<std::string, std::string>;

struct AnimationClip;  // gfx/tranm.hpp

struct ModelExportOptions {
    std::vector<const AnimationClip*> animations;        // glTF animations[] (trilhas ligadas aos nos pelo nome)
    std::map<std::string, std::string> base_color_uris;  // material -> textura difusa assada (substitui BaseColorMap)
};

// glTF 2.0 (.gltf + .bin) com malhas, materiais, esqueleto/skin, animacoes e texturas (PNG no nucleo do
// glTF; DDS via MSFT_texture_dds)
bool ExportToGltf(const Model& model, const std::string& out_path, const TextureUris& texture_uris, std::string& error,
                  const ModelExportOptions& options = {});
// Wavefront OBJ + MTL (geometria, UV0, normais e map_Kd/map_Bump/map_Ka), sem skinning
bool ExportToObj(const Model& model, const std::string& out_path, const TextureUris& texture_uris, std::string& error,
                 const ModelExportOptions& options = {});

} // namespace za::gfx
