#pragma once
// Colisao da Trinity Engine (.trcol): tagfile binario do Havok 2022 (SDKV 20220200) com um hknpMeshShape
//
// Secoes: cabecalho u32 big-endian (2 bits de flags + 30 de tamanho, incluindo o cabecalho) + tag de 4 chars
//   TAG0 { SDKV "20220200", DATA (objetos), TYPE { TST1, TNA1, FST1, TBDY, TPAD }, INDX { ITEM } }
//   TST1/FST1: nomes de tipos/campos separados por '\0'; TNA1/TBDY: tipos em varints (tamanho, campos e offsets)
//   ITEM [{ u32 (flags << 24 | tipo); u32 offset em DATA; u32 contagem }] — ponteiros e arrays nos objetos
//   sao indices de ITEM (4 bytes), ou seja, o layout e o "compacto" descrito pelo TBDY, nao o nativo
// Raiz: hkRootLevelContainer -> { "hknpMeshShape", "hknpMaterialPalette" } (todos os 1062 .trcol do RomFS)
// hknpMeshShape.geometrySections[] { primitives [{u8 a, b, c, d}] (triangulo se c == d), vertexBuffer
//   [u16 x, y, z], sectionOffset i32[3] }; vertice = (sectionOffset + v) * vertexConversionUtil.bitScale16Inv
//   (1/512 nos dados). Winding anti-horario: normais do chao apontam para +Y.
//   Excecao: sectionOffset = INT32_MAX nos 3 eixos marca secao sem quantizacao — vertexBuffer com float32 xyz.
//   Sao 2598 das 7591 secoes do RomFS (130 arquivos, ex.: a01_col, t1_g02_1_col); conferido: todos os vertices
//   dentro da BVH de 8 bits da secao ((bitOffset + q8) * bitScale8Inv) e indices < bytes / 12.

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace za::gfx {

struct CollisionMesh {
    std::vector<std::array<float, 3>> vertices;
    std::vector<std::array<uint32_t, 3>> triangles;
    std::vector<std::string> materials;     // nomes do hknpMaterialPalette (ex.: "attr_iwa_forceslip")
    std::array<float, 3> aabb_min{0, 0, 0};
    std::array<float, 3> aabb_max{0, 0, 0};
    uint32_t section_count{0};
    uint32_t quad_count{0};                 // primitivas quad (cada uma vira 2 triangulos)
    uint32_t unquantized_section_count{0};  // secoes com vertices float32 (sectionOffset = INT32_MAX)

    // Grade XZ de aceleracao do raycast vertical (montada por ParseTrcol / BuildGroundGrid)
    uint32_t grid_x{0}, grid_z{0};
    float cell_size_x{1}, cell_size_z{1};
    std::vector<uint32_t> cell_start;       // grid_x * grid_z + 1 offsets em cell_triangles
    std::vector<uint32_t> cell_triangles;

    bool Empty() const { return triangles.empty(); }
};

bool ParseTrcol(std::span<const uint8_t> data, CollisionMesh& out, std::string& error);
void BuildGroundGrid(CollisionMesh& mesh);

// Raio vertical descendo de +infinito em (x, z): altura da superficie mais alta e sua normal (com y >= 0)
bool RaycastGround(const CollisionMesh& col, float x, float z, float& out_ground_y, std::array<float, 3>& out_normal);
// Variante que so aceita superficies em y <= from_y (chao sob um personagem dentro de um predio)
bool RaycastGround(const CollisionMesh& col, float x, float from_y, float z, float& out_ground_y,
                   std::array<float, 3>& out_normal);

// Aplica uma matriz 4x4 column-major (ex.: SRT da cena ou instancia do .trins) e reconstroi AABB e grade
void TransformMesh(CollisionMesh& mesh, const std::array<float, 16>& matrix);

// Conjunto de malhas de colisao de um cenario (tiles de chao, props, arena) ja em coordenadas de mundo
class CollisionWorld {
public:
    // Superficies ate esta distancia acima da camera contam como chao atravessado; acima disso sao teto/ponte
    static constexpr float kCameraProbeAbove = 4.0f;

    void Clear();
    size_t Add(CollisionMesh mesh);
    size_t MeshCount() const { return meshes_.size(); }
    const CollisionMesh& Mesh(size_t index) const { return meshes_[index]; }
    const std::array<float, 3>& BoundsMin() const { return bounds_min_; }
    const std::array<float, 3>& BoundsMax() const { return bounds_max_; }

    // Superficie mais alta entre todas as malhas (de +infinito, ou so abaixo de from_y)
    bool GetGroundHeight(float x, float z, float& out_y, std::array<float, 3>& out_normal) const;
    bool GetGroundHeight(float x, float from_y, float z, float& out_y, std::array<float, 3>& out_normal) const;

    // Spring-arm: trata a camera como esfera de raio min_height_above_ground e a empurra para cima ate ela
    // ficar acima do chao — no centro a folga e medida na perpendicular do declive (h / normal.y) e em 4 pontos
    // no raio da esfera o equador nao pode ficar abaixo do terreno vizinho. Sem chao, devolve a posicao pedida.
    // (std::array e nao Vec3: camera3d.hpp ja declara um za::gfx::Vec3 proprio)
    std::array<float, 3> ClampCameraAboveGround(const std::array<float, 3>& target_camera_pos,
                                                float min_height_above_ground = 0.5f) const;

private:
    std::vector<CollisionMesh> meshes_;
    std::array<float, 3> bounds_min_{0, 0, 0};
    std::array<float, 3> bounds_max_{0, 0, 0};
    bool has_bounds_{false};
};

} // namespace za::gfx
