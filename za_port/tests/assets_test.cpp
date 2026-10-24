// Testes do pipeline de assets: BNTX (layout/deswizzle/DDS), reconstrucao de nomes e modelos Trinity
// uso: assets_test <romfs_dir>   (os testes de integracao usam o data.trpfs real e exigem o Oodle)

#include "gfx/bntx.hpp"
#include "gfx/ik_character.hpp"
#include "gfx/texture_codec.hpp"
#include "gfx/tranm.hpp"
#include "gfx/trcol.hpp"
#include "gfx/trmdl.hpp"
#include "gfx/trscn.hpp"
#include "gfx/scene_loader.hpp"
#include "vfs/asset_export.hpp"
#include "vfs/name_recovery.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

using namespace za;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    std::printf("%s %s\n", condition ? "[PASS]" : "[FAIL]", what);
    if (!condition) ++g_failures;
}

// Endereco block-linear de referencia (formula classica por byte), independente da implementacao testada
size_t ReferenceAddress(size_t x_bytes, size_t y, size_t width_bytes, size_t block_height) {
    const size_t width_in_gobs = (width_bytes + 63) / 64;
    const size_t gob = (y / (8 * block_height)) * 512 * block_height * width_in_gobs +
                       (x_bytes / 64) * 512 * block_height + (y % (8 * block_height) / 8) * 512;
    return gob + ((x_bytes % 64) / 32) * 256 + ((y % 8) / 2) * 64 + ((x_bytes % 32) / 16) * 32 + (y % 2) * 16 + (x_bytes % 16);
}

gfx::BntxTexture MakeTexture(uint32_t format, uint32_t width, uint32_t height, uint16_t mips, uint32_t block_height_log2,
                             uint32_t array_length = 1, gfx::SurfaceDim dim = gfx::SurfaceDim::Dim2D, uint32_t depth = 1,
                             uint32_t block_depth_log2 = 0) {
    gfx::BntxTexture texture;
    texture.format = gfx::DescribeFormat(format);
    texture.width = width;
    texture.height = height;
    texture.depth = depth;
    texture.array_length = array_length;
    texture.mip_count = mips;
    texture.block_height_log2 = block_height_log2;
    texture.block_depth_log2 = block_depth_log2;
    texture.surface_dim = dim;
    return texture;
}

void TestBntxFormats() {
    const gfx::BntxFormat bc7 = gfx::DescribeFormat(0x2006);
    Check(bc7.name == "BC7_SRGB" && bc7.block_width == 4 && bc7.bytes_per_block == 16 && bc7.dxgi == 99, "formato 0x2006 = BC7_SRGB / DXGI 99");
    const gfx::BntxFormat half = gfx::DescribeFormat(0x1505);
    Check(half.name == "R16G16B16A16_FLOAT" && half.bytes_per_block == 8 && half.dxgi == 10, "formato 0x1505 = R16G16B16A16_FLOAT");
    Check(gfx::DescribeFormat(0x1F0A).dxgi == 95 && gfx::DescribeFormat(0x1F05).dxgi == 96, "BC6H UF16/SF16 -> DXGI 95/96");
    Check(!gfx::DescribeFormat(0x7701).Known(), "formato desconhecido nao e aceito");
}

void TestBntxLayout() {
    // Valores conferidos em texturas do jogo (offsets de mip e image_size do BRTI)
    gfx::SurfaceLayout bc5 = gfx::ComputeLayout(MakeTexture(0x1E01, 1024, 1024, 11, 4));
    Check(bc5.mips.size() == 11 && bc5.mips[1].offset == 1048576 && bc5.mips[2].offset == 1310720 &&
          bc5.mips[3].offset == 1376256 && bc5.total_size == 1400320, "layout BC5 1024x1024, 11 mips");
    gfx::SurfaceLayout cube = gfx::ComputeLayout(MakeTexture(0x1F05, 128, 128, 8, 2, 6, gfx::SurfaceDim::Cube));
    Check(cube.layer_stride == 24576 && cube.total_size == 147456, "layout cubemap BC6H 128x128x6, 8 mips");
    gfx::SurfaceLayout volume = gfx::ComputeLayout(MakeTexture(0x1505, 1, 1, 1, 0, 1, gfx::SurfaceDim::Dim3D, 7, 3));
    Check(volume.total_size == 4096, "layout 3D 1x1x7 com 8 GOBs em Z");
}

void TestDeswizzle() {
    // RGBA8 100x70, bloco de 8 GOBs: largura sem potencia de 2 e padding em X e Y
    gfx::BntxTexture texture = MakeTexture(0x0B01, 100, 70, 1, 3);
    const gfx::SurfaceLayout layout = gfx::ComputeLayout(texture);
    texture.image_size = static_cast<uint32_t>(layout.total_size);
    texture.mip_offsets = {0};

    std::vector<uint8_t> swizzled(layout.total_size, 0), expected(100 * 70 * 4);
    for (size_t y = 0; y < 70; ++y) {
        for (size_t x = 0; x < 100 * 4; ++x) {
            const auto value = static_cast<uint8_t>((x * 7 + y * 13) & 0xFF);
            swizzled[ReferenceAddress(x, y, 100 * 4, 8)] = value;
            expected[y * 400 + x] = value;
        }
    }
    std::vector<uint8_t> linear;
    std::string error;
    Check(texture.image_size == 57344 && gfx::DeswizzleTexture(swizzled, texture, linear, error) && linear == expected,
          "deswizzle confere com a formula de referencia");

    std::vector<uint8_t> dds;
    const bool built = gfx::BuildDds(texture, linear, dds, error);
    uint32_t header_size = 0, dxgi = 0;
    if (built) {
        std::memcpy(&header_size, dds.data() + 4, 4);
        std::memcpy(&dxgi, dds.data() + 128, 4);
    }
    Check(built && dds.size() == 148 + linear.size() && std::memcmp(dds.data(), "DDS ", 4) == 0 && header_size == 124 &&
          std::memcmp(dds.data() + 84, "DX10", 4) == 0 && dxgi == 28, "DDS DX10: cabecalho de 148 bytes e DXGI 28");
}

void TestTextureCodec() {
    // BC1: c0 = vermelho, c1 = azul; pixels 0..3 usam os indices 0, 1, 2 (2/3 c0 + 1/3 c1) e 3
    gfx::BntxTexture texture = MakeTexture(0x1A01, 4, 4, 1, 0);
    const uint8_t block[8] = {0x00, 0xF8, 0x1F, 0x00, 0xE4, 0x00, 0x00, 0x00};
    std::vector<uint8_t> rgba;
    std::string error;
    const bool decoded = gfx::DecodeTexture(texture, block, rgba, error);
    Check(decoded && rgba[0] == 255 && rgba[2] == 0 && rgba[4] == 0 && rgba[6] == 255 && rgba[8] == 170 && rgba[10] == 85 &&
              rgba[12] == 85 && rgba[14] == 170 && rgba[16] == 255, "DecodeTexture BC1 (paleta de 4 cores)");

    const uint8_t pixels[6] = {255, 0, 0, 0, 255, 0};
    std::vector<uint8_t> png;
    const bool encoded = gfx::EncodePng(2, 1, 3, pixels, png, error);
    const uint8_t iend_crc[4] = {0xAE, 0x42, 0x60, 0x82};
    Check(encoded && std::memcmp(png.data(), "\x89PNG\r\n\x1a\n", 8) == 0 && png[19] == 2 && png[23] == 1 && png[25] == 2 &&
              std::memcmp(png.data() + png.size() - 4, iend_crc, 4) == 0, "EncodePng: assinatura, IHDR e CRC do IEND");
}

void TestBakeSynthetic() {
    // Mascara 2x2: (0,0) camada 1, (1,0) camada 2, (0,1) nenhuma, (1,1) camada 3 "nao configurada" (cor 0,0,0)
    const gfx::BntxTexture base = MakeTexture(0x0B01, 2, 2, 1, 0), mask = MakeTexture(0x0B01, 2, 2, 1, 0);
    const std::vector<uint8_t> white(16, 255);
    const std::vector<uint8_t> masks = {255, 0, 0, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0, 255, 0};
    const std::vector<std::array<float, 4>> layers = {{0.5f, 0, 0, 1}, {0, 0.5f, 0, 1}, {0, 0, 0, 1}, {0, 0, 0, 1}};
    std::vector<uint8_t> out;
    std::string error;
    const bool baked = gfx::BakeIkCharacterTexture(base, white, mask, masks, layers, out, error);
    // 0.5 linear = 188 em sRGB
    Check(baked && out.size() == 16 && out[0] == 188 && out[1] == 0 && out[5] == 188 && out[4] == 0 &&
              out[8] == 255 && out[9] == 255 && out[12] == 255 && out[14] == 255,
          "BakeIkCharacterTexture: camadas, area neutra e camada nao configurada");
}

void TestQuaternion() {
    const gfx::Quat identity = gfx::UnpackQuaternion48(0x0003, 0x0002, 0x8001);
    const gfx::Quat negated = gfx::UnpackQuaternion48(0x0003 | 0x4, 0x0002, 0x8001);
    Check(std::abs(identity[3] - 1.0f) < 1e-4f && std::abs(identity[0]) < 1e-4f && std::abs(negated[3] + 1.0f) < 1e-4f,
          "UnpackQuaternion48: identidade e bit de sinal (nega o quaternion)");
}

void TestPaths() {
    Check(vfs::ResolveRelativePath("ik_pokemon/data/pm0150/pm0150_00_00", "../../../share/specular_prb/x.bntx") ==
              "ik_pokemon/share/specular_prb/x.bntx", "ResolveRelativePath com '..'");
    Check(vfs::ResolveRelativePath("a/b", "./c.trmsh") == "a/b/c.trmsh", "ResolveRelativePath com '.'");
}

// Raycast de referencia sem a grade: superficie mais alta entre todos os triangulos
bool BruteForceGround(const gfx::CollisionMesh& mesh, float x, float z, float& out_y) {
    bool hit = false;
    for (const auto& t : mesh.triangles) {
        const auto& a = mesh.vertices[t[0]];
        const auto& b = mesh.vertices[t[1]];
        const auto& c = mesh.vertices[t[2]];
        const double det = double{b[0] - a[0]} * (c[2] - a[2]) - double{c[0] - a[0]} * (b[2] - a[2]);
        if (std::abs(det) < 1e-12) continue;
        const double u = (double{x - a[0]} * (c[2] - a[2]) - double{c[0] - a[0]} * (z - a[2])) / det;
        const double v = (double{b[0] - a[0]} * (z - a[2]) - double{x - a[0]} * (b[2] - a[2])) / det;
        if (u < 0 || v < 0 || u + v > 1) continue;
        const float y = static_cast<float>(a[1] + u * (b[1] - a[1]) + v * (c[1] - a[1]));
        if (!hit || y > out_y) out_y = y;
        hit = true;
    }
    return hit;
}

void TestCollision(vfs::TrpfsArchive& archive) {
    std::vector<uint8_t> data;
    std::string error;

    // Cristal da Torre Prisma: 1 secao, 9 vertices, 6 quads, material de deslize
    gfx::CollisionMesh crystal;
    const bool crystal_ok = archive.ReadFile(vfs::HashPath("field/model/t1/t1_b/t1_b_lb/t1_b_lb_mega_crystal_a_col.trcol"), data) &&
                            gfx::ParseTrcol(data, crystal, error);
    Check(crystal_ok && crystal.section_count == 1 && crystal.vertices.size() == 9 && crystal.quad_count == 6 &&
              crystal.triangles.size() == 12 && crystal.materials.size() == 1 && crystal.materials.front() == "attr_iwa_forceslip",
          "ParseTrcol: mega_crystal_a com 9 vertices, 6 quads e material attr_iwa_forceslip");
    // AABB conferida com o no raiz do hkcdSimdTree gravado no proprio arquivo
    Check(crystal_ok && std::abs(crystal.aabb_min[0] + 0.681640625f) < 1e-6f && std::abs(crystal.aabb_max[1] - 2.259765625f) < 1e-6f &&
              std::abs(crystal.aabb_min[2] + 0.7109375f) < 1e-6f, "ParseTrcol: AABB = no raiz do hkcdSimdTree (quantizacao 1/512)");

    // Tile de chao de Lumiose (t1_g09_1): 12168 triangulos
    gfx::CollisionMesh ground;
    const bool ground_ok = archive.ReadFile(vfs::HashPath("field/model/t1/t1_g/t1_g09_1/t1_g09_1_col.trcol"), data) &&
                           gfx::ParseTrcol(data, ground, error);
    Check(ground_ok && ground.vertices.size() == 10324 && ground.triangles.size() == 12168 && ground.quad_count == 5809 &&
              ground.aabb_min[0] > 563.0f && ground.aabb_max[0] < 837.0f, "ParseTrcol: t1_g09_1 com 10324 vertices e 12168 triangulos");

    float y = 0;
    std::array<float, 3> normal{};
    const float cx = (ground.aabb_min[0] + ground.aabb_max[0]) / 2, cz = (ground.aabb_min[2] + ground.aabb_max[2]) / 2;
    const bool center = ground_ok && gfx::RaycastGround(ground, cx, cz, y, normal);
    const float length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
    Check(center && y >= ground.aabb_min[1] && y <= ground.aabb_max[1] && std::abs(length - 1.0f) < 1e-4f && normal[1] > 0,
          "RaycastGround no centro do tile: altura dentro da AABB e normal unitaria para cima");
    Check(ground_ok && !gfx::RaycastGround(ground, ground.aabb_max[0] + 10.0f, cz, y, normal),
          "RaycastGround fora da AABB nao encontra chao");

    // Grade x forca bruta em 400 pontos pseudo-aleatorios
    uint32_t seed = 12345, compared = 0, agree = 0;
    const auto next = [&seed]() { return static_cast<float>((seed = seed * 1664525u + 1013904223u) / 4294967296.0); };
    for (int i = 0; ground_ok && i < 400; ++i) {
        const float x = ground.aabb_min[0] + next() * (ground.aabb_max[0] - ground.aabb_min[0]);
        const float z = ground.aabb_min[2] + next() * (ground.aabb_max[2] - ground.aabb_min[2]);
        float fast = 0, reference = 0;
        const bool hit_fast = gfx::RaycastGround(ground, x, z, fast, normal), hit_reference = BruteForceGround(ground, x, z, reference);
        ++compared;
        agree += (hit_fast == hit_reference && (!hit_fast || std::abs(fast - reference) < 1e-3f)) ? 1 : 0;
    }
    Check(compared == 400 && agree == compared, "RaycastGround com grade XZ confere com a forca bruta (400 pontos)");

    // Centroides de faces voltadas para cima: o raio de cima nunca fica abaixo delas e, partindo
    // logo acima, encontra exatamente a altura da face
    size_t faces = 0, above = 0, exact = 0;
    for (size_t i = 0; ground_ok && i < ground.triangles.size(); i += 7) {
        const auto& t = ground.triangles[i];
        const auto& a = ground.vertices[t[0]];
        const auto& b = ground.vertices[t[1]];
        const auto& c = ground.vertices[t[2]];
        const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const float n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
        const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len < 1e-6f || n[1] / len < 0.7f) continue;
        const float px = (a[0] + b[0] + c[0]) / 3, py = (a[1] + b[1] + c[1]) / 3, pz = (a[2] + b[2] + c[2]) / 3;
        ++faces;
        float top = 0, floor_y = 0;
        above += gfx::RaycastGround(ground, px, pz, top, normal) && top >= py - 1e-3f ? 1 : 0;
        exact += gfx::RaycastGround(ground, px, py + 1e-3f, pz, floor_y, normal) && std::abs(floor_y - py) < 1e-3f ? 1 : 0;
    }
    Check(faces > 500 && above == faces && exact == faces,
          "RaycastGround consistente com os centroides das faces de chao (topo e from_y)");

    // CollisionWorld: o tile original e uma copia transladada 300 m em X
    gfx::CollisionWorld world;
    gfx::CollisionMesh shifted = ground;
    std::array<float, 16> translate{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 300, 0, 0, 1};
    gfx::TransformMesh(shifted, translate);
    world.Add(ground);
    world.Add(std::move(shifted));
    float tile_y = 0, world_y = 0, shifted_y = 0;
    const bool world_ok = ground_ok && gfx::RaycastGround(ground, 700, 700, tile_y, normal) &&
                          world.GetGroundHeight(700, 700, world_y, normal) && world.GetGroundHeight(1000, 700, shifted_y, normal);
    Check(world_ok && world.MeshCount() == 2 && world_y == tile_y && std::abs(shifted_y - tile_y) < 1e-4f &&
              std::abs(world.BoundsMax()[0] - (ground.aabb_max[0] + 300)) < 1e-3f && !world.GetGroundHeight(0, 0, world_y, normal),
          "CollisionWorld: chao unificado de 2 malhas (TransformMesh desloca a copia 300 m)");

    const std::array<float, 3> sunk = world.ClampCameraAboveGround({700, tile_y - 3.0f, 700}, 0.5f);
    const std::array<float, 3> high = world.ClampCameraAboveGround({700, tile_y + 10.0f, 700}, 0.5f);
    const std::array<float, 3> outside = world.ClampCameraAboveGround({0, -50, 0}, 0.5f);
    Check(world_ok && sunk[0] == 700 && sunk[2] == 700 && sunk[1] >= tile_y + 0.5f - 1e-4f && sunk[1] < tile_y + 1.5f &&
              high[1] == tile_y + 10.0f && outside[1] == -50.0f,
          "ClampCameraAboveGround: camera enterrada sobe para >= chao + 0,5 m; acima do chao ou fora dele fica onde esta");

    // Declive: no centroide de uma face inclinada a folga perpendicular ao plano fica >= 0,5 m
    bool slope_ok = false;
    for (size_t i = 0; ground_ok && i < ground.triangles.size() && !slope_ok; ++i) {
        const auto& t = ground.triangles[i];
        const auto& a = ground.vertices[t[0]];
        const auto& b = ground.vertices[t[1]];
        const auto& c = ground.vertices[t[2]];
        const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        float n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
        const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len < 0.5f || n[1] / len < 0.6f || n[1] / len > 0.9f) continue;  // face grande e inclinada (26..53 graus)
        for (float& v : n) v /= len;
        const float px = (a[0] + b[0] + c[0]) / 3, py = (a[1] + b[1] + c[1]) / 3, pz = (a[2] + b[2] + c[2]) / 3;
        float top = 0;
        if (!world.GetGroundHeight(px, py + 1e-3f, pz, top, normal) || std::abs(top - py) > 1e-3f) continue;  // face nao coberta
        const std::array<float, 3> cam = world.ClampCameraAboveGround({px, py - 1.0f, pz}, 0.5f);
        const float distance = (cam[0] - a[0]) * n[0] + (cam[1] - a[1]) * n[1] + (cam[2] - a[2]) * n[2];
        slope_ok = distance >= 0.5f - 1e-3f && cam[1] >= py + 0.5f / n[1] - 1e-3f;
    }
    Check(slope_ok, "ClampCameraAboveGround em declive: folga perpendicular ao plano >= 0,5 m");
}

void TestScene(vfs::TrpfsArchive& archive) {
    std::vector<uint8_t> data;
    std::string error;

    const std::string boss_dir = "world/ik_scene/field/area/boss/ba_battle_area_0003_01_";
    gfx::Scene boss;
    const bool boss_ok = archive.ReadFile(vfs::HashPath(boss_dir + "/ba_battle_area_0003_01_0.trscn"), data) &&
                         gfx::ParseTrscn(data, boss, error);
    Check(boss_ok && boss.name == "ba_battle_area_0003_01" && boss.layer == "---- Default ----" && boss.sub_scenes.size() == 2 &&
              boss.entities.size() == 2 && boss.entities[0].type == "SubScene" && boss.entities[0].assets.size() == 1 &&
              std::find(boss.sub_scenes.begin(), boss.sub_scenes.end(), boss.entities[0].assets[0]) != boss.sub_scenes.end(),
          "ParseTrscn: arena ba_battle_area_0003_01 com 2 SubScene e caminhos .trscn");
    size_t resolved = 0;
    for (const std::string& sub : boss.sub_scenes) {
        resolved += archive.HasFile(vfs::HashPath(gfx::SceneChunkPath(vfs::ResolveRelativePath(boss_dir, sub)))) ? 1 : 0;
    }
    Check(gfx::SceneChunkPath("a/b.trscn") == "a/b_0.trscn" && boss_ok && resolved == 2,
          "Sub-cenas relativas da arena resolvem para o bloco <nome>_0.trscn no pacote");

    gfx::Scene net;
    const bool net_ok = archive.ReadFile(vfs::HashPath("world/ik_scene/field/area/t1_net_battle5_/t1_net_battle5_0.trscn"), data) &&
                        gfx::ParseTrscn(data, net, error);
    const gfx::SceneEntity* respawn = nullptr;
    size_t models = 0;
    net.Visit([&](const gfx::SceneEntity& entity, int) {
        if (entity.name == "respawn_1") respawn = &entity;
        for (const std::string& asset : entity.assets) models += asset.ends_with(".trmdl") ? 1 : 0;
    });
    Check(net_ok && respawn && respawn->has_transform && std::abs(respawn->transform.translation[0] - 5.0f) < 1e-4f &&
              std::abs(respawn->transform.rotation[1] + 1.5707963f) < 1e-4f && respawn->transform.scale[0] == 1.0f,
          "trinity_SceneObject respawn_1: T(5, 0, 0), R(0, -pi/2, 0), S(1)");
    Check(models > 0, "Cena t1_net_battle5 referencia modelos .trmdl");

    gfx::InstanceBuffer instances;
    const bool trins_ok = archive.ReadFile(vfs::HashPath("world/ik_scene/field/area/t1/sub_scene/t1_mob/generated/t1_day_mob_/"
                                                         "tr0566_00_04030_landscape07_loop_tr0566_00_ra00_000.trins"), data) &&
                          gfx::ParseTrins(data, instances, error);
    bool affine = trins_ok;
    for (const auto& m : instances.matrices) affine = affine && m[3] == 0 && m[7] == 0 && m[11] == 0 && m[15] == 1.0f;
    Check(trins_ok && instances.type == "SrtInstanceBuffer" && instances.matrices.size() == 67 && affine &&
              std::abs(instances.matrices[0][12] + 592.0566f) < 1e-3f && std::abs(instances.matrices[0][14] + 613.3157f) < 1e-3f,
          "ParseTrins: 67 matrizes afins column-major (primeira em -592.06, 0.008, -613.32)");
}

void TestSceneLoader(vfs::TrpfsArchive& archive) {
    auto vfs_reader = [&](const std::string& path, std::vector<uint8_t>& out_bytes) -> bool {
        return archive.ReadFile(vfs::HashPath(path), out_bytes);
    };

    gfx::CollisionWorld world;
    size_t loaded = 0;
    std::string error;
    std::vector<std::string> sectors = {
        "field/model/t1/t1_battle/t1_battle1/t1_battle1_col.trcol",
        "field/model/t1/t1_g/t1_g01_1/t1_g01_1_col.trcol",
        "field/model/t1/t1_b/t1_b_lb/t1_b_lb_mega_crystal_a_col.trcol"
    };

    // 1. Carregamento de multiplos setores de colisao
    const bool sectors_ok = gfx::SceneLoader::Instance().LoadCollisionSectors(sectors, vfs_reader, world, loaded, error);
    Check(sectors_ok && loaded == 3 && world.MeshCount() == 3,
          "SceneLoader::LoadCollisionSectors: carrega 3 setores de colisao no CollisionWorld");

    // 2. Teste de RaycastGround na arena de batalha circular (posicao 5.0, 5.0, solo em y=0.0)
    float ground_y = 0.0f;
    std::array<float, 3> normal{0.0f, 1.0f, 0.0f};
    const bool ray_ok = world.GetGroundHeight(5.0f, 5.0f, ground_y, normal);
    Check(ray_ok && std::abs(ground_y) < 1e-4f && std::abs(normal[1] - 1.0f) < 1e-4f,
          "CollisionWorld: RaycastGround detecta solo perfeitamente horizontal na arena (y=0.0)");

    // 3. Carregamento do modelo visual avulso da arena (.trmdl + .trmsh + .trmbf)
    gfx::Model arena_model;
    const bool model_ok = gfx::SceneLoader::Instance().LoadModelFile("field/model/t1/t1_battle/t1_battle1/t1_battle1.trmdl",
                                                                     vfs_reader, arena_model, error);
    Check(model_ok && arena_model.meshes.size() == 1 && arena_model.meshes.front().vertex_count == 1538,
          "SceneLoader::LoadModelFile: carrega modelo da arena (1538 vertices, 2592 triangulos)");
    // (Havia aqui um LoadScene de "t1_battle1_/t1_battle1_0.trscn": esse caminho nao existe no data.trpfd, entao o
    // teste falhava desde o primeiro commit. A montagem de cena real e coberta por world_test com t1_/t1_0.trscn.)
}

void TestArchive(const std::filesystem::path& romfs) {
    const std::filesystem::path arc = romfs / "arc";
    vfs::TrpfsArchive archive;
    if (!archive.Mount((arc / "data.trpfd").string(), (arc / "data.trpfs").string())) {
        Check(false, "Mount do data.trpfd/data.trpfs");
        return;
    }

    // Reconstrucao de nome: sem vocabulario, 4 barras livres
    vfs::NameRecovery recovery(archive, vfs::PathDictionary{});
    Check(recovery.ResolvePackStem("worldik_databattlelockon_descplayer.bin", 4).value_or("") ==
              "world/ik_data/battle/lockon_desc/player.bin", "ResolvePackStem reinsere as 4 barras");
    Check(!recovery.ResolvePackStem("worldik_databattlelockon_descplayer.bin", 3).has_value(),
          "ResolvePackStem respeita o limite de barras livres");

    if (!vfs::oodle::IsAvailable()) {
        Check(false, "Oodle disponivel para os testes de modelo (ZA_OODLE_DLL)");
        return;
    }
    TestCollision(archive);
    TestScene(archive);
    TestSceneLoader(archive);
    const std::string dir = "ik_pokemon/data/pm0150/pm0150_00_00/";
    std::vector<uint8_t> data;
    std::string error, buffer_file;
    gfx::TrmdlFile trmdl;
    gfx::Model model;
    model.name = "pm0150_00_00";
    const bool loaded = archive.ReadFile(vfs::HashPath(dir + "pm0150_00_00.trmdl"), data) && gfx::ParseTrmdl(data, trmdl, error) &&
                        archive.ReadFile(vfs::HashPath(dir + trmdl.mesh_files.front()), data) &&
                        gfx::ParseTrmsh(data, model.meshes, buffer_file, error) &&
                        archive.ReadFile(vfs::HashPath(dir + buffer_file), data) && gfx::LoadTrmbf(data, model.meshes, error) &&
                        archive.ReadFile(vfs::HashPath(dir + trmdl.material_files.front()), data) &&
                        gfx::ParseTrmtr(data, model.materials, error) &&
                        archive.ReadFile(vfs::HashPath(dir + trmdl.skeleton_file), data) && gfx::ParseTrskl(data, model.skeleton, error);
    Check(loaded && trmdl.mesh_files.size() == 4, "Mewtwo: .trmdl com 4 LODs + .trmsh/.trmbf/.trmtr/.trskl");
    if (!loaded) {
        std::printf("       erro: %s\n", error.c_str());
        return;
    }

    size_t vertices = 0, triangles = 0;
    double max_normal_error = 0, max_weight_error = 0;
    for (const gfx::MeshShape& mesh : model.meshes) {
        vertices += mesh.vertex_count;
        triangles += mesh.indices.size() / 3;
        for (size_t v = 0; v < mesh.vertex_count; ++v) {
            const float* n = mesh.normals.data() + v * 3;
            const float* w = mesh.weights.data() + v * 4;
            max_normal_error = std::max(max_normal_error, std::abs(std::sqrt(double{n[0]} * n[0] + double{n[1]} * n[1] + double{n[2]} * n[2]) - 1.0));
            max_weight_error = std::max(max_weight_error, std::abs(double{w[0]} + w[1] + w[2] + w[3] - 1.0));
        }
    }
    Check(model.meshes.size() == 2 && vertices == 6061 && triangles == 9986, "Mewtwo: 2 malhas, 6061 vertices, 9986 triangulos");
    Check(max_normal_error < 0.01 && max_weight_error < 0.001, "Mewtwo: normais unitarias e pesos somando 1");
    Check(model.materials.size() == 4 && model.materials.front().Find("BaseColorMap") != nullptr &&
          model.materials.front().shader == "IkCharacter", "Mewtwo: 4 materiais IkCharacter com BaseColorMap");

    // Esqueleto: composicao Euler XYZ extrinseca ate a raiz deve inverter a inverse bind matrix
    const auto& nodes = model.skeleton.nodes;
    std::vector<std::array<double, 16>> world(nodes.size());
    double max_bind_error = 0;
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& r = nodes[i].rotation;
        const double cx = std::cos(r[0]), sx = std::sin(r[0]), cy = std::cos(r[1]), sy = std::sin(r[1]);
        const double cz = std::cos(r[2]), sz = std::sin(r[2]);
        const double rot[9] = {cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx,
                               sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx,
                               -sy, cy * sx, cy * cx};
        std::array<double, 16> local{};  // coluna-major
        for (int c = 0; c < 3; ++c) {
            for (int row = 0; row < 3; ++row) local[static_cast<size_t>(c * 4 + row)] = rot[row * 3 + c] * nodes[i].scale[static_cast<size_t>(c)];
            local[static_cast<size_t>(12 + c)] = nodes[i].translation[static_cast<size_t>(c)];
        }
        local[15] = 1;
        if (nodes[i].parent < 0) {
            world[i] = local;
        } else {
            const auto& p = world[static_cast<size_t>(nodes[i].parent)];
            for (int c = 0; c < 4; ++c) {
                for (int row = 0; row < 4; ++row) {
                    double sum = 0;
                    for (int k = 0; k < 4; ++k) sum += p[static_cast<size_t>(k * 4 + row)] * local[static_cast<size_t>(c * 4 + k)];
                    world[i][static_cast<size_t>(c * 4 + row)] = sum;
                }
            }
        }
        if (nodes[i].bone < 0) continue;
        const auto& ibm = model.skeleton.inverse_bind[static_cast<size_t>(nodes[i].bone)];
        for (int c = 0; c < 4; ++c) {
            for (int row = 0; row < 4; ++row) {
                double sum = 0;
                for (int k = 0; k < 4; ++k) sum += world[i][static_cast<size_t>(k * 4 + row)] * ibm[static_cast<size_t>(c * 4 + k)];
                max_bind_error = std::max(max_bind_error, std::abs(sum - (row == c ? 1.0 : 0.0)));
            }
        }
    }
    Check(nodes.size() == 88 && model.skeleton.inverse_bind.size() == 73 && max_bind_error < 1e-4,
          "Mewtwo: 88 nos, 73 ossos, Euler XYZ confere com as inverse bind matrices");

    // UpdateSkeletonMatrices na pose de repouso: skinning = world * IBM = identidade
    double max_skin_error = 0;
    for (const auto& matrix : model.skeleton.skinning) {
        for (size_t k = 0; k < 16; ++k) max_skin_error = std::max(max_skin_error, std::abs(double{matrix[k]} - (k % 5 == 0 ? 1.0 : 0.0)));
    }
    Check(model.skeleton.skinning.size() == 73 && max_skin_error < 1e-4, "UpdateSkeletonMatrices: skinning identidade em repouso");

    // Bake IkCharacter do body_b (corpo/cauda): resultado nao e branco e contem o roxo de BaseColorLayer2
    const gfx::Material* body_b = nullptr;
    for (const gfx::Material& material : model.materials) {
        if (material.name == "body_b") body_b = &material;
    }
    gfx::BntxFile base_file, mask_file;
    std::vector<uint8_t> base_data, mask_data, base_rgba, mask_rgba, baked;
    const bool textures_ok = body_b && archive.ReadFile(vfs::HashPath(dir + body_b->Find("BaseColorMap")->file), base_data) &&
                             archive.ReadFile(vfs::HashPath(dir + body_b->Find("LayerMaskMap")->file), mask_data) &&
                             gfx::ParseBntx(base_data, base_file, error) && gfx::ParseBntx(mask_data, mask_file, error) &&
                             gfx::DecodeBntxTexture(base_data, base_file.textures.front(), base_rgba, error) &&
                             gfx::DecodeBntxTexture(mask_data, mask_file.textures.front(), mask_rgba, error);
    std::vector<std::array<float, 4>> layers;
    const gfx::IkCharacterBakeSettings settings = body_b ? gfx::BakeSettingsFromMaterial(*body_b, layers) : gfx::IkCharacterBakeSettings{};
    const bool baked_ok = textures_ok && gfx::BakeIkCharacterTexture(base_file.textures.front(), base_rgba, mask_file.textures.front(),
                                                                     mask_rgba, layers, baked, error, settings);
    size_t purple = 0, white = 0;
    for (size_t p = 0; baked_ok && p < baked.size(); p += 4) {
        purple += (std::abs(baked[p] - 166) < 14 && std::abs(baked[p + 1] - 120) < 14 && std::abs(baked[p + 2] - 143) < 14) ? 1 : 0;
        white += (baked[p] > 250 && baked[p + 1] > 250 && baked[p + 2] > 250) ? 1 : 0;
    }
    Check(baked_ok && settings.mirror_mask_u && settings.mask_uv_transform[0] == 2.0f && purple > baked.size() / 40 &&
              white < baked.size() / 40, "Bake do Mewtwo body_b: roxo da camada 2 presente, sem areas brancas");

    // Animacao real: defaultwait01_loop
    gfx::AnimationClip clip;
    const bool clip_ok = archive.ReadFile(vfs::HashPath(dir + "pm0150_00_00_00000_defaultwait01_loop.tranm"), data) &&
                         gfx::ParseTranm(data, clip, error);
    Check(clip_ok && clip.frame_count == 181 && clip.fps == 60 && clip.loop && clip.tracks.size() == 56 &&
              std::abs(clip.DurationSeconds() - 3.0f) < 1e-4f, "ParseTranm: 181 quadros @ 60 fps, loop, 56 trilhas");

    size_t fixed_translations = 0, matching_rest = 0;
    for (const gfx::AnimationTrack& track : clip.tracks) {
        if (track.translation.kind != gfx::AnimationChannelKind::Fixed) continue;
        for (const gfx::SkeletonNode& node : model.skeleton.nodes) {
            if (node.name != track.bone) continue;
            ++fixed_translations;
            const auto& value = track.translation.keys.front().value;
            matching_rest += (std::abs(value[0] - node.translation[0]) < 1e-3f && std::abs(value[1] - node.translation[1]) < 1e-3f &&
                              std::abs(value[2] - node.translation[2]) < 1e-3f) ? 1 : 0;
        }
    }
    Check(fixed_translations > 40 && matching_rest * 10 >= fixed_translations * 9,
          "Trilhas fixas de translacao conferem com a pose de repouso do .trskl");

    // Junta do loop: o fim do clipe (antes do wrap) volta a pose do inicio; no meio a pose difere do repouso
    gfx::Skeleton start = model.skeleton, end = model.skeleton, middle = model.skeleton;
    gfx::EvaluateAnimation(clip, 0.0f, start);
    gfx::EvaluateAnimation(clip, clip.DurationSeconds() - 1e-4f, end);
    gfx::EvaluateAnimation(clip, 1.0f, middle);
    double loop_error = 0, motion = 0;
    bool finite = true;
    for (size_t b = 0; b < start.skinning.size(); ++b) {
        for (size_t k = 0; k < 16; ++k) {
            loop_error = std::max(loop_error, std::abs(double{start.skinning[b][k]} - end.skinning[b][k]));
            motion = std::max(motion, std::abs(double{middle.skinning[b][k]} - (k % 5 == 0 ? 1.0 : 0.0)));
            finite = finite && std::isfinite(start.skinning[b][k]) && std::isfinite(middle.skinning[b][k]);
        }
    }
    Check(finite && loop_error < 5e-3 && motion > 1e-2, "EvaluateAnimation: matrizes finitas, loop fecha e o meio do clipe se move");
    clip.name = "defaultwait01_loop";

    const auto out = std::filesystem::temp_directory_path() / "za_assets_test";
    std::filesystem::remove_all(out);
    gfx::ModelExportOptions options;
    options.animations.push_back(&clip);
    options.base_color_uris["body_b"] = "textures/pm0150_00_00_body_b_baked.png";
    const bool exported = gfx::ExportToGltf(model, (out / "pm0150_00_00.gltf").string(), {}, error, options) &&
                          gfx::ExportToObj(model, (out / "pm0150_00_00.obj").string(), {}, error, options);
    std::ifstream gltf(out / "pm0150_00_00.gltf");
    const std::string json((std::istreambuf_iterator<char>(gltf)), std::istreambuf_iterator<char>());
    Check(exported && json.find("\"JOINTS_0\"") != std::string::npos && json.find("\"skins\"") != std::string::npos &&
          std::filesystem::file_size(out / "pm0150_00_00.bin") > 0 && std::filesystem::exists(out / "pm0150_00_00.mtl"),
          "ExportToGltf/ExportToObj gravam glTF com skin, .bin e OBJ+MTL");
    Check(json.find("\"animations\"") != std::string::npos && json.find("\"path\":\"rotation\"") != std::string::npos &&
              json.find("\"interpolation\":\"LINEAR\"") != std::string::npos && json.find("image/png") != std::string::npos &&
              json.find("\"name\":\"defaultwait01_loop\"") != std::string::npos,
          "glTF com animations[] (LINEAR) e difusa assada em PNG no nucleo do glTF");
    gltf.close();
    std::filesystem::remove_all(out);
}

} // namespace

int main(int argc, char** argv) {
    TestBntxFormats();
    TestBntxLayout();
    TestDeswizzle();
    TestTextureCodec();
    TestBakeSynthetic();
    TestQuaternion();
    TestPaths();
    if (argc > 1) {
        TestArchive(argv[1]);
    } else {
        std::puts("(sem <romfs_dir>: testes de integracao ignorados)");
    }
    std::printf("%s (%d falhas)\n", g_failures == 0 ? "OK" : "FALHOU", g_failures);
    return g_failures == 0 ? 0 : 1;
}
