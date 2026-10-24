// Testes do cenario de Lumiose City: hierarquia .trscn -> CollisionWorld (SceneAssembler), secoes float32 do
// .trcol e identificacao das faixas de musica/ambiencia da cidade nos SoundBanks
// uso: world_test <romfs_dir>   (data.trpfs real com Oodle + romfs/audio/wwise/bin/Switch)

#include "audio/wem_decoder.hpp"
#include "audio/wwise.hpp"
#include "gfx/scene_assembly.hpp"
#include "vfs/trpfs.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>

using namespace za;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    std::printf("%s %s\n", condition ? "[PASS]" : "[FAIL]", what);
    if (!condition) ++g_failures;
}

const std::string kSectors[] = {
    "world/ik_scene/field/area/t1/sub_scene/t1_bk01_/t1_bk01.trscn",
    "world/ik_scene/field/area/t1/sub_scene/t1_bk02_/t1_bk02.trscn",
    "world/ik_scene/field/area/t1/sub_scene/t1_bk03_/t1_bk03.trscn",
};
const std::string kGroundTiles[] = {
    "field/model/t1/t1_g/t1_g01_1/t1_g01_1_col.trcol",
    "field/model/t1/t1_g/t1_g02_1/t1_g02_1_col.trcol",
    "field/model/t1/t1_g/t1_g03_1/t1_g03_1_col.trcol",
};

void TestTransforms() {
    gfx::SceneTransform yaw;
    yaw.rotation = {0, -3.14159265f, 0};
    yaw.translation = {10, 2, -5};
    const auto m = gfx::ComposeTransform(yaw);
    // (1, 0, 0) gira para (-1, 0, 0) e soma a translacao
    Check(std::abs(m[0] + 1) < 1e-6f && std::abs(m[2]) < 1e-6f && std::abs(m[10] + 1) < 1e-6f && m[12] == 10 && m[13] == 2 &&
              m[14] == -5 && m[15] == 1, "ComposeTransform: R(0, -pi, 0) + T");
    gfx::SceneTransform scale;
    scale.scale = {2, 3, 4};
    const auto s = gfx::MultiplyTransforms(m, gfx::ComposeTransform(scale));
    Check(std::abs(s[0] + 2) < 1e-5f && std::abs(s[5] - 3) < 1e-5f && std::abs(s[10] + 4) < 1e-5f && s[12] == 10,
          "MultiplyTransforms: pai * filho aplica a escala do filho antes da rotacao do pai");
}

bool ReadArchive(vfs::TrpfsArchive& archive, const std::string& path, std::vector<uint8_t>& out) {
    const uint64_t hash = vfs::HashPath(path);
    return archive.HasFile(hash) && archive.ReadFile(hash, out);
}

void TestUnquantizedSections(vfs::TrpfsArchive& archive) {
    // t1_g02_1: 1 das 37 secoes tem sectionOffset = INT32_MAX e vertices float32 (antes viravam ~4194304 m)
    std::vector<uint8_t> data;
    std::string error;
    gfx::CollisionMesh mesh;
    const bool ok = ReadArchive(archive, kGroundTiles[1], data) && gfx::ParseTrcol(data, mesh, error);
    Check(ok && mesh.section_count == 37 && mesh.unquantized_section_count == 1 && mesh.vertices.size() == 3987 &&
              mesh.triangles.size() == 4338 && mesh.aabb_min[0] > 189.0f && mesh.aabb_max[0] < 408.0f && mesh.aabb_max[1] < 6.0f,
          "ParseTrcol: secao float32 (sectionOffset = INT32_MAX) do t1_g02_1 decodificada dentro da AABB do tile");
}

void TestSectors(vfs::TrpfsArchive& archive) {
    const auto reader = [&archive](const std::string& path, std::vector<uint8_t>& out) { return ReadArchive(archive, path, out); };
    gfx::SceneAssembler assembler(reader);
    std::string error;
    bool loaded = true;
    for (const std::string& sector : kSectors) loaded = assembler.AddScene(sector, error) && loaded;
    size_t sector_scenes = 0;
    for (const std::string& scene : assembler.Scenes()) sector_scenes += scene.find("/t1_bk0") != std::string::npos ? 1 : 0;
    Check(loaded && sector_scenes == 3 && assembler.Scenes().size() == 11 && assembler.MissingScenes().empty() &&
              assembler.Models().size() == 39 && assembler.Collisions().size() == 51,
          "SceneAssembler: 3 setores (t1_bk01..03_0.trscn) -> 11 cenas, 39 modelos, 51 colisoes, nenhuma ausente");

    // Tiles de chao dos 3 setores e a rotacao de -pi do SceneObject do tile
    const gfx::ScenePlacement* tiles[3] = {};
    const gfx::ScenePlacement* salon = nullptr;
    for (const gfx::ScenePlacement& p : assembler.Collisions()) {
        for (int i = 0; i < 3; ++i) {
            if (p.path == kGroundTiles[i]) tiles[i] = &p;
        }
        if (p.path.ends_with("t1_b_salon_rouge_col.trcol")) salon = &p;
    }
    Check(tiles[0] && tiles[1] && tiles[2] && std::abs(tiles[0]->world[0] + 1) < 1e-5f && std::abs(tiles[0]->world[10] + 1) < 1e-5f,
          "Tiles de chao t1_g01_1/g02_1/g03_1 posicionados com R(0, -pi, 0) do SceneObject");
    if (!tiles[0] || !tiles[1] || !tiles[2]) return;

    gfx::CollisionWorld world;
    std::string col_error;
    const size_t meshes = assembler.BuildCollisionWorld(world, col_error);
    Check(meshes == 51 && col_error.empty() && world.BoundsMin()[0] > -620 && world.BoundsMax()[0] < -91 &&
              world.BoundsMin()[2] > -1003 && world.BoundsMax()[2] < -709,
          "BuildCollisionWorld: 51 malhas em coordenadas de mundo (X -620..-91, Z -1003..-709)");

    // Emenda entre setores vizinhos: na faixa onde as AABBs se sobrepoem, pontos em que a superficie no nivel da rua
    // (abaixo de 3 m) do setor A e a do setor B coincidem (|dy| < 10 cm) — o chao de um continua no do outro
    const auto sector_of = [](const std::string& scene) {
        const size_t at = scene.find("unity_output/t1_");
        return at == std::string::npos ? -1 : std::atoi(scene.c_str() + at + 18);
    };
    gfx::CollisionWorld sectors[3];
    for (size_t i = 0; i < world.MeshCount(); ++i) {
        const int sector = sector_of(assembler.Collisions()[i].scene);
        if (sector >= 1 && sector <= 3) sectors[sector - 1].Add(world.Mesh(i));
    }
    size_t joined = 0;
    for (int pair = 0; pair < 2; ++pair) {
        const gfx::CollisionWorld& a = sectors[pair];
        const gfx::CollisionWorld& b = sectors[pair + 1];
        const float x0 = std::max(a.BoundsMin()[0], b.BoundsMin()[0]), x1 = std::min(a.BoundsMax()[0], b.BoundsMax()[0]);
        const float z0 = std::max(a.BoundsMin()[2], b.BoundsMin()[2]), z1 = std::min(a.BoundsMax()[2], b.BoundsMax()[2]);
        size_t shared = 0, samples = 0;
        for (int i = 0; x0 < x1 && z0 < z1 && i < 60; ++i) {
            for (int j = 0; j < 60; ++j, ++samples) {
                const float x = x0 + (x1 - x0) * (static_cast<float>(i) + 0.5f) / 60, z = z0 + (z1 - z0) * (static_cast<float>(j) + 0.5f) / 60;
                float ya = 0, yb = 0;
                std::array<float, 3> n{};
                if (a.GetGroundHeight(x, 3.0f, z, ya, n) && b.GetGroundHeight(x, 3.0f, z, yb, n) && std::abs(ya - yb) < 0.1f) ++shared;
            }
        }
        joined += shared >= 20 ? 1 : 0;  // medido: 71 (bk01/bk02) e 34 (bk02/bk03) de 3600
        std::printf("       emenda bk0%d/bk0%d: %zu de %zu pontos com o mesmo chao nos dois setores\n", pair + 1, pair + 2, shared, samples);
    }
    // Controle: bk01 e bk03 nao sao vizinhos (nenhuma sobreposicao em X)
    const bool apart = sectors[0].BoundsMin()[0] > sectors[2].BoundsMax()[0] || sectors[2].BoundsMin()[0] > sectors[0].BoundsMax()[0];
    Check(joined == 2 && apart && sectors[0].MeshCount() > 0 && sectors[1].MeshCount() > 0 && sectors[2].MeshCount() > 0,
          "Setores interligados: bk01-bk02 e bk02-bk03 compartilham o chao (>= 20 pontos); bk01/bk03 nao se tocam");

    gfx::CollisionWorld ground;
    std::vector<uint8_t> data;
    for (const auto* tile : tiles) {
        gfx::CollisionMesh mesh;
        if (ReadArchive(archive, tile->path, data) && gfx::ParseTrcol(data, mesh, error)) {
            gfx::TransformMesh(mesh, tile->world);
            ground.Add(std::move(mesh));
        }
    }

    // O salao Rouge (ObjectTemplate em -214, 0.094, -861) fica apoiado no chao do tile girado
    float floor_y = 0;
    std::array<float, 3> n{};
    const bool on_ground = salon && ground.GetGroundHeight(salon->world[12], salon->world[13] + 1.0f, salon->world[14], floor_y, n);
    Check(on_ground && std::abs(salon->world[12] + 213.992f) < 1e-2f && std::abs(salon->world[14] + 860.944f) < 1e-2f &&
              std::abs(floor_y - salon->world[13]) < 0.25f,
          "Predio t1_b_salon_rouge (ObjectTemplate) assenta no chao do setor: transformacoes compostas corretas");
}

void TestWholeCity(vfs::TrpfsArchive& archive) {
    const auto reader = [&archive](const std::string& path, std::vector<uint8_t>& out) { return ReadArchive(archive, path, out); };
    gfx::SceneAssembler assembler(reader);
    std::string error;
    const bool ok = assembler.AddScene("world/ik_scene/field/area/t1/sub_scene/field_/field.trscn", error);
    std::set<std::string> blocks;
    for (const std::string& scene : assembler.Scenes()) {
        if (const size_t at = scene.find("/t1_bk"); at != std::string::npos) blocks.insert(scene.substr(at, 9));
    }
    gfx::CollisionWorld world;
    std::string col_error;
    const size_t meshes = assembler.BuildCollisionWorld(world, col_error);
    Check(ok && assembler.Scenes().size() == 361 && assembler.MissingScenes().empty() && blocks.size() == 25 &&
              meshes > 18000 && col_error.empty() && world.BoundsMin()[0] > -1200 && world.BoundsMax()[2] < 100,
          "Lumiose inteira (field_0.trscn): 361 cenas, 25 blocos t1_bk, > 18000 malhas de colisao sem falhas");
}

void TestCityAudio(const std::filesystem::path& romfs) {
    const std::filesystem::path dir = romfs / "audio" / "wwise" / "bin" / "Switch";
    std::string error;
    audio::WwiseBank bgm, env;
    audio::WwisePackage stream;
    if (!bgm.Load((dir / "BGM.bnk").string(), error) || !env.Load((dir / "ENV.bnk").string(), error) ||
        !stream.Open((dir / "Stream.pck").string(), error)) {
        Check(false, "BGM.bnk, ENV.bnk e Stream.pck abertos");
        return;
    }
    // SET_STATE_T1_MAIN_STREET: acao SetState (0x1204) com o valor de estado "T1_MAIN_STREET" no offset 13
    const audio::HircObject* event = bgm.FindObject(audio::WwiseHash("SET_STATE_T1_MAIN_STREET"));
    const uint32_t state = audio::WwiseHash("T1_MAIN_STREET");
    bool sets_state = false;
    for (size_t a = 0; event && a < bgm.Body(*event)[0]; ++a) {
        uint32_t action_id = 0, value = 0;
        uint16_t type = 0;
        std::memcpy(&action_id, bgm.Body(*event).data() + 1 + 4 * a, 4);
        const audio::HircObject* action = bgm.FindObject(action_id);
        if (!action || action->body_size < 17) continue;
        std::memcpy(&type, bgm.Body(*action).data(), 2);
        std::memcpy(&value, bgm.Body(*action).data() + 13, 4);
        sets_state = sets_state || (type == 0x1204 && value == state);
    }
    // MusicTracks (tipo 11) que dependem desse estado e suas fontes no Stream.pck
    std::set<uint32_t> sources;
    for (const audio::HircObject& object : bgm.HircObjects()) {
        if (object.type != 11) continue;
        const auto body = bgm.Body(object);
        bool uses_state = false;
        for (size_t i = 0; i + 4 <= body.size() && !uses_state; ++i) uses_state = std::memcmp(body.data() + i, &state, 4) == 0;
        uint32_t media = 0;
        if (uses_state && body.size() >= 14) {
            std::memcpy(&media, body.data() + 10, 4);
            sources.insert(media);
        }
    }
    size_t ptadpcm = 0;
    std::vector<uint8_t> header;
    for (const uint32_t media : sources) {
        const audio::WwiseMedia* entry = stream.Find(media);
        audio::WemInfo info;
        ptadpcm += entry && stream.Read(*entry, header, error, 64) && audio::ParseWemInfo(header, info) && info.codec == 0x8311 &&
                           info.channels == 2 && info.sample_rate == 48000 ? 1 : 0;
    }
    Check(state == 120572130 && sets_state && sources == std::set<uint32_t>{226064614, 461972315} && ptadpcm == 2,
          "Musica de Lumiose (estado T1_MAIN_STREET): MusicTracks 226064614 e 461972315, PTADPCM estereo 48 kHz");

    // Primeiros 64 KB da faixa principal decodificam para audio nao silencioso
    const audio::WwiseMedia* main_street = stream.Find(226064614);
    std::vector<uint8_t> wem;
    audio::WemInfo info;
    std::vector<int16_t> pcm;
    int peak = 0;
    if (main_street && stream.Read(*main_street, wem, error, 64 * 1024) && audio::ParseWemInfo(wem, info)) {
        const size_t block = info.block_align;
        const size_t usable = (wem.size() - info.data_offset) / block * block;
        audio::DecodePtadpcm(std::span<const uint8_t>(wem.data() + info.data_offset, usable), info.channels, block / info.channels, pcm, error);
        for (const int16_t s : pcm) peak = std::max(peak, std::abs(int{s}));
    }
    Check(!pcm.empty() && pcm.size() % 2 == 0 && peak > 1000, "Faixa 226064614: inicio decodificado com amplitude real");

    // A ambiencia propriamente dita (ENV.bnk) e toda Opus NX: nao ha faixa de ambiencia em PTADPCM para decodificar
    size_t opus = 0;
    for (const audio::WwiseMedia& media : env.Media()) {
        audio::WemInfo env_info;
        opus += audio::ParseWemInfo(env.MediaData(media), env_info) && env_info.codec == 0x3039 ? 1 : 0;
    }
    Check(!env.Media().empty() && opus == env.Media().size(), "ENV.bnk: todas as midias embutidas sao Opus NX (0x3039)");
}

} // namespace

int main(int argc, char** argv) {
    TestTransforms();
    if (argc > 1) {
        const std::filesystem::path romfs = argv[1];
        vfs::TrpfsArchive archive;
        if (archive.Mount((romfs / "arc" / "data.trpfd").string(), (romfs / "arc" / "data.trpfs").string()) &&
            vfs::oodle::IsAvailable()) {
            TestUnquantizedSections(archive);
            TestSectors(archive);
            TestWholeCity(archive);
        } else {
            Check(false, "Mount do data.trpfs com Oodle disponivel (ZA_OODLE_DLL)");
        }
        TestCityAudio(romfs);
    } else {
        std::puts("(sem <romfs_dir>: testes de integracao ignorados)");
    }
    std::printf("%s (%d falhas)\n", g_failures == 0 ? "OK" : "FALHOU", g_failures);
    return g_failures == 0 ? 0 : 1;
}
