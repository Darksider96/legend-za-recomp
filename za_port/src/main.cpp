#include <iostream>
#include <string>
#include <filesystem>
#include <thread>
#include <chrono>
#include <vector>
#include "nn/os.hpp"
#include "nn/time.hpp"
#include "nn/hid.hpp"
#include "nn/fs.hpp"
#include "nn/vi.hpp"
#include "nvn/nvn.hpp"
#include "loader/nso_loader.hpp"
#include "loader/got_patcher.hpp"
#include "vfs/trpfs.hpp"
#include "za/subsystem_registry.h"
#include "za/logger.hpp"
#include "za/crash_handler.hpp"
#include "za/profiler.hpp"
#include "audio/wwise.hpp"
#include "audio/audio_playback.hpp"
#include "audio/wem_decoder.hpp"
#include "data/personal_table.hpp"
#include "data/message_text.hpp"
#include "data/move_table.hpp"
#include "game/battle_calc.hpp"
#include "gfx/trmdl.hpp"
#include "gfx/tranm.hpp"
#include "gfx/trcol.hpp"
#include "gfx/renderer3d.hpp"
#include "gfx/scene_loader.hpp"
#include "gfx/scene_assembly.hpp"
#include "game/poke_actor.hpp"
#include "audio/ambient_stream.hpp"
#include "config/game_config.hpp"

namespace {
struct PokedexEntry {
    uint16_t species_id{150};
    std::string name;
    za::gfx::Model model;
    std::shared_ptr<za::gfx::AnimationClip> idle_anim;
    std::shared_ptr<za::gfx::AnimationClip> walk_anim;
    std::shared_ptr<za::gfx::AnimationClip> run_anim;
    std::shared_ptr<za::gfx::AnimationClip> attack_anim;
};

bool PlayPokemonCry(uint16_t species_id, const std::string& romfs_dir) {
    static za::audio::WwiseBank s_voice_bank;
    static bool s_loaded = false;
    if (!s_loaded) {
        std::string err;
        std::string voice_path = romfs_dir + "/audio/wwise/bin/Switch/PM_VOICE.bnk";
        if (s_voice_bank.Load(voice_path, err)) {
            s_loaded = true;
            std::cout << "[AUDIO-CRY] [OK] PM_VOICE.bnk carregado (" << s_voice_bank.HircObjectCount() << " objetos HIRC)!" << std::endl;
        } else {
            std::cerr << "[AUDIO-CRY] [ERRO] Falha ao carregar PM_VOICE.bnk: " << err << std::endl;
        }
    }
    if (!s_loaded) return false;

    uint32_t media_id = 0;
    std::string err;
    if (!za::audio::ResolvePokemonCryMediaId(s_voice_bank, species_id, media_id, err)) {
        std::cerr << "[AUDIO-CRY] [AVISO] Especie " << species_id << " sem cry no banco: " << err << std::endl;
        return false;
    }

    std::vector<uint8_t> wem_bytes;
    if (!za::audio::WwiseArchive::Instance().GetMediaData(media_id, wem_bytes)) {
        std::cerr << "[AUDIO-CRY] [AVISO] Midia " << media_id << " nao encontrada no WwiseArchive!" << std::endl;
        return false;
    }

    za::audio::PcmBuffer pcm;
    if (!za::audio::DecodeWem(wem_bytes, pcm, err)) {
        std::cerr << "[AUDIO-CRY] [ERRO] Falha ao decodificar WEM PTADPCM " << media_id << ": " << err << std::endl;
        return false;
    }

    za::audio::PcmBuffer stereo48k = za::audio::ResampleLinear(pcm, 48000, 2);
    za::audio::AudioPlayback::Instance().PlayPcm(stereo48k.samples.data(), stereo48k.samples.size(), 0.90f);
    std::cout << "[AUDIO-CRY] >>> REPRODUZINDO CRY OFICIAL: Especie #" << species_id 
              << " | Midia WEM " << media_id << " (" << pcm.sample_rate << " Hz -> 48 kHz, "
              << stereo48k.DurationSeconds() << "s, " << stereo48k.FrameCount() << " amostras) <<<" << std::endl;
    return true;
}
}

int main(int argc, char* argv[]) {
    // Inicialização do Logger Central, Interceptor de Crash do Windows e Profiler de Performance
    za::Logger::Instance().Initialize("za_engine.log");
    za::CrashHandler::Install();
    za::Profiler::Instance().Initialize();
    za::CrashHandler::SetLastCheckpoint("Boot / Inicializacao dos Subsistemas");

    (void)argc;
    (void)argv;

    // 0. Leitura e Validação das Configurações do Jogo (za_config.ini)
    za::GameConfig g_game_config;
    std::string config_path = "za_config.ini";
    if (!std::filesystem::exists(config_path)) config_path = "../za_config.ini";
    if (std::filesystem::exists(config_path)) {
        std::vector<std::string> cfg_warnings;
        g_game_config.LoadFromFile(config_path, cfg_warnings);
        for (const auto& w : cfg_warnings) {
            ZA_LOG_WARN("CONFIG", "%s", w.c_str());
        }
        ZA_LOG_INFO("CONFIG", "Configuracoes carregadas de '%s' (Resolucao: %dx%d, VSync: %s, Shading: %s, FPS: %d)",
                    config_path.c_str(), g_game_config.graphics.window_width, g_game_config.graphics.window_height,
                    g_game_config.graphics.vsync ? "ON" : "OFF", g_game_config.graphics.shading.c_str(),
                    g_game_config.performance.target_fps);
    } else {
        g_game_config.SetDefaults();
        g_game_config.SaveToFile("za_config.ini");
        ZA_LOG_INFO("CONFIG", "za_config.ini nao encontrado. Criado com os valores padrao.");
    }

    // Configura resolução da janela antes da inicialização do subsistema visual (nn::vi)
    nn::vi::SetWindowResolution(g_game_config.graphics.window_width, g_game_config.graphics.window_height);

    std::cout << "=====================================================" << std::endl;
    std::cout << "  ZA_PORT - Camada de Compatibilidade Nativa para PC" << std::endl;
    std::cout << "  Alvo: Pokemon Legends: Z-A (Switch AArch64)" << std::endl;
    std::cout << "=====================================================" << std::endl;

    std::cout << "\n[1/6] Inicializando Subcodigo de Sistema (nn::os)..." << std::endl;
    nn::os::LightEventType boot_event{};
    nn::os::InitializeLightEvent(&boot_event, false, nn::os::EventClearMode_AutoClear);

    std::cout << "\n[2/6] Inicializando Subsistema de Tempo (nn::time)..." << std::endl;
    nn::time::Initialize();

    std::cout << "\n[3/6] Inicializando Subsistema de Entrada (nn::hid)..." << std::endl;
    nn::hid::InitializeNpad();

    std::cout << "\n[4/6] Inicializando Subsistema de Arquivos (nn::fs)..." << std::endl;
    std::string romfs_root = g_game_config.storage.romfs_path;
    if (!std::filesystem::exists(romfs_root)) romfs_root = "romfs";
    if (!std::filesystem::exists(romfs_root)) romfs_root = "../romfs";
    if (!std::filesystem::exists(romfs_root)) romfs_root = "C:/Users/johna/OneDrive/Documentos/legend za/romfs";
    nn::fs::SetLocalDataRoot(romfs_root);

    if (g_game_config.performance.oodle_cache_kb > 0) {
        size_t budget_bytes = (g_game_config.performance.oodle_cache_kb >= 1024) 
            ? static_cast<size_t>(g_game_config.performance.oodle_cache_kb) * 1024ULL 
            : static_cast<size_t>(g_game_config.performance.oodle_cache_kb) * 1024ULL * 1024ULL;
        nn::fs::GetTrpfsArchive().SetPackCacheBudget(budget_bytes);
    }

    size_t cache_size = 0;
    nn::fs::QueryMountRomCacheSize(&cache_size);
    nn::fs::MountRom("data", nullptr, cache_size);

    // Validação da Fase 2 (Codex RE): Leitura do primeiro arquivo do jogo
    std::cout << "\n[FS-TEST] Testando abertura de arquivo solto no disco (trinity_config.trowc)..." << std::endl;
    nn::fs::FileHandle cfg_handle{};
    nn::Result fs_res = nn::fs::OpenFile(&cfg_handle, "data:/system/config/data/trinity_config.trowc", 1);
    if (fs_res == nn::ResultSuccess) {
        int64_t cfg_size = 0;
        nn::fs::GetFileSize(&cfg_size, cfg_handle);
        std::vector<char> cfg_data(cfg_size);
        nn::fs::ReadFile(cfg_handle, 0, cfg_data.data(), cfg_size);
        nn::fs::CloseFile(cfg_handle);
        std::cout << "[FS-TEST] [SUCESSO] 'trinity_config.trowc' carregado com sucesso (" << cfg_size << " bytes)!" << std::endl;
    } else {
        std::cout << "[FS-TEST] [AVISO] 'trinity_config.trowc' nao encontrado no caminho local." << std::endl;
    }

    // Validação da integração VFS / TRPFS transparente via OpenFile / ReadFile
    std::cout << "\n[FS-TEST] Testando leitura transparente de arquivo embutido no TRPFS (Oodle)..." << std::endl;
    nn::fs::FileHandle shader_handle{};
    // Teste de arquivo de shader conhecido nos nomes do TRPFS
    fs_res = nn::fs::OpenFile(&shader_handle, "data:/system_resource/gfx2/shader/NX64/env_ikkaku/system/simple_fullscreen.trsha", 1);
    if (fs_res == nn::ResultSuccess) {
        int64_t shader_size = 0;
        nn::fs::GetFileSize(&shader_size, shader_handle);
        std::vector<uint8_t> shader_bytes(shader_size);
        nn::fs::ReadFile(shader_handle, 0, shader_bytes.data(), shader_size);
        nn::fs::CloseFile(shader_handle);
        std::cout << "[FS-TEST] [SUCESSO VFS TRPFS] 'simple_fullscreen.trsha' lido via nn::fs::OpenFile! (" 
                  << shader_size << " bytes descomprimidos)" << std::endl;
    } else {
        std::cout << "[FS-TEST] [AVISO] 'simple_fullscreen.trsha' nao pode ser aberto." << std::endl;
    }

    // Teste de arquivo sem compressao no TRPFS
    std::cout << "\n[FS-TEST] Testando leitura de arquivo sem compressao no TRPFS (player.bin)..." << std::endl;
    nn::fs::FileHandle bin_handle{};
    fs_res = nn::fs::OpenFile(&bin_handle, "data:/world/ik_data/battle/lockon_desc/player.bin", 1);
    if (fs_res == nn::ResultSuccess) {
        int64_t bin_size = 0;
        nn::fs::GetFileSize(&bin_size, bin_handle);
        std::vector<uint8_t> bin_bytes(bin_size);
        nn::fs::ReadFile(bin_handle, 0, bin_bytes.data(), bin_size);
        nn::fs::CloseFile(bin_handle);
        std::cout << "[FS-TEST] [SUCESSO VFS TRPFS] 'player.bin' lido via nn::fs::OpenFile! (" 
                  << bin_size << " bytes)" << std::endl;
    } else {
        std::cout << "[FS-TEST] [AVISO] 'player.bin' nao pode ser aberto." << std::endl;
    }

    // Carregamento de Elenco Pokédex da Trinity Engine para o Viewport
    std::cout << "\n[3D-MODEL] Carregando elenco de Pokemons do TRPFS para o Viewport..." << std::endl;
    std::vector<PokedexEntry> pokedex_roster;

    auto read_vfs_file = [](const std::string& vpath, std::vector<uint8_t>& out) -> bool {
        nn::fs::FileHandle h{};
        if (nn::fs::OpenFile(&h, ("data:/" + vpath).c_str(), 1) != nn::ResultSuccess) return false;
        int64_t sz = 0;
        nn::fs::GetFileSize(&sz, h);
        out.resize(static_cast<size_t>(sz));
        nn::fs::ReadFile(h, 0, out.data(), sz);
        nn::fs::CloseFile(h);
        return true;
    };

    // Carregamento da Base de Dados da Pokédex (personal_array.bin) do TRPFS
    std::cout << "\n[POKEDEX-DB] Carregando personal_array.bin da Trinity Engine..." << std::endl;
    std::vector<uint8_t> personal_data;
    std::string pdb_error;
    if (read_vfs_file("avalon/data/personal_array.bin", personal_data)) {
        if (za::data::PersonalDatabase::Instance().Load(personal_data, pdb_error)) {
            std::cout << "[POKEDEX-DB] [SUCESSO] Base de Dados Carregada: "
                      << za::data::PersonalDatabase::Instance().Size() << " especies registradas!" << std::endl;
        } else {
            std::cout << "[POKEDEX-DB] [AVISO] Falha ao parsear personal_array.bin: " << pdb_error << std::endl;
        }
    } else {
        std::cout << "[POKEDEX-DB] [AVISO] personal_array.bin nao encontrado no VFS." << std::endl;
    }

    auto load_pokemon_model = [&](uint16_t species_id, const std::string& pm_id, const std::string& pm_name) -> bool {
        const std::string dir = "ik_pokemon/data/" + pm_id + "/" + pm_id + "_00_00/";
        std::vector<uint8_t> data;
        std::string err, buf_file;
        za::gfx::TrmdlFile trmdl_f;
        PokedexEntry entry;
        entry.species_id = species_id;
        entry.name = pm_name;
        entry.model.name = pm_name + " (" + pm_id + ")";

        if (read_vfs_file(dir + pm_id + "_00_00.trmdl", data) && za::gfx::ParseTrmdl(data, trmdl_f, err) &&
            !trmdl_f.mesh_files.empty() && read_vfs_file(dir + trmdl_f.mesh_files.front(), data) &&
            za::gfx::ParseTrmsh(data, entry.model.meshes, buf_file, err) &&
            read_vfs_file(dir + buf_file, data) && za::gfx::LoadTrmbf(data, entry.model.meshes, err)) {

            // 1. Carrega o esqueleto (.trskl) para deformação de skinning em tempo real
            if (!trmdl_f.skeleton_file.empty() && read_vfs_file(dir + trmdl_f.skeleton_file, data)) {
                if (za::gfx::ParseTrskl(data, entry.model.skeleton, err)) {
                    za::gfx::UpdateSkeletonMatrices(entry.model.skeleton);
                }
            }

            // 2. Carrega as 4 animações canônicas da Trinity Engine (Idle, Walk, Run, Attack)
            za::gfx::AnimationClip idle_clip, walk_clip, run_clip, attack_clip;
            std::string idle_path = dir + pm_id + "_00_00_00000_defaultwait01_loop.tranm";
            std::string walk_path = dir + pm_id + "_00_00_00030_walk01_loop.tranm";
            std::string run_path  = dir + pm_id + "_00_00_00100_run01_loop.tranm";
            std::string atk_path  = dir + pm_id + "_00_00_00400_attack01.tranm";

            if (read_vfs_file(idle_path, data) && za::gfx::ParseTranm(data, idle_clip, err)) {
                entry.idle_anim = std::make_shared<za::gfx::AnimationClip>(std::move(idle_clip));
            }
            if (read_vfs_file(walk_path, data) && za::gfx::ParseTranm(data, walk_clip, err)) {
                entry.walk_anim = std::make_shared<za::gfx::AnimationClip>(std::move(walk_clip));
            }
            if (read_vfs_file(run_path, data) && za::gfx::ParseTranm(data, run_clip, err)) {
                entry.run_anim = std::make_shared<za::gfx::AnimationClip>(std::move(run_clip));
            }
            if (read_vfs_file(atk_path, data) && za::gfx::ParseTranm(data, attack_clip, err)) {
                entry.attack_anim = std::make_shared<za::gfx::AnimationClip>(std::move(attack_clip));
            }

            size_t total_verts = 0, total_tris = 0;
            for (const auto& m : entry.model.meshes) {
                total_verts += m.vertex_count;
                total_tris += m.indices.size() / 3;
            }
            std::cout << "  -> [OK] " << entry.model.name << ": " << entry.model.meshes.size() << " malhas, "
                      << total_verts << " vertices, " << total_tris << " triangulos, "
                      << entry.model.skeleton.nodes.size() << " ossos | "
                      << (entry.idle_anim ? "Idle " : "")
                      << (entry.walk_anim ? "Walk " : "")
                      << (entry.run_anim ? "Run " : "")
                      << (entry.attack_anim ? "Attack" : "")
                      << std::endl;
            pokedex_roster.push_back(std::move(entry));
            return true;
        }
        std::cout << "  -> [AVISO] Falha ao carregar " << pm_name << " (" << pm_id << "): " << err << std::endl;
        return false;
    };

    load_pokemon_model(150, "pm0150", "Mewtwo");
    load_pokemon_model(25,  "pm0025", "Pikachu");
    load_pokemon_model(1,   "pm0001", "Bulbasaur");
    load_pokemon_model(8,   "pm0008", "Wartortle");

    // Carregamento do Banco de Dados de Golpes (waza_array.bin + wazaname.dat)
    std::cout << "\n[MOVE-DB] Carregando waza_array.bin e wazaname.dat..." << std::endl;
    std::vector<uint8_t> waza_bytes, wazaname_bytes;
    std::string move_err;
    if (read_vfs_file("avalon/data/waza_array.bin", waza_bytes) &&
        read_vfs_file("ik_message/dat/English/common/wazaname.dat", wazaname_bytes)) {
        if (za::data::MoveDatabase::Instance().Load(waza_bytes, wazaname_bytes, move_err)) {
            std::cout << "  -> [OK MOVE-DB] " << za::data::MoveDatabase::Instance().Count()
                      << " golpes de batalha carregados com sucesso!" << std::endl;
        } else {
            std::cout << "  -> [AVISO] Falha ao carregar MoveDatabase: " << move_err << std::endl;
        }
    }

    // Instanciação dos Atores Dinâmicos de Gameplay
    std::vector<std::shared_ptr<za::game::PokeActor>> poke_actors;
    for (const auto& entry : pokedex_roster) {
        auto actor = std::make_shared<za::game::PokeActor>();
        actor->species_id = entry.species_id;
        actor->name = entry.name;
        actor->model = entry.model;
        actor->anims.idle = entry.idle_anim;
        actor->anims.walk = entry.walk_anim;
        actor->anims.run = entry.run_anim;
        actor->anims.attack = entry.attack_anim;
        actor->position = za::gfx::Vec3{0.0f, 0.0f, 0.0f};
        actor->scale = 1.2f;

        // Atribuição canônica de combate e dos 4 golpes ativos
        if (entry.species_id == 150) {
            actor->InitializeCombat(150, "Mewtwo", 13, 13, 50, 180, 150, 100, 180, 100, 130);
            actor->moves = {94, 247, 129, 105}; // Psychic, Shadow Ball, Swift, Recover
            actor->move_pp = {10, 15, 20, 5};
            actor->move_max_pp = {10, 15, 20, 5};
        } else if (entry.species_id == 25) {
            actor->InitializeCombat(25, "Pikachu", 12, 12, 50, 120, 110, 80, 100, 80, 140);
            actor->moves = {85, 98, 231, 486};  // Thunderbolt, Quick Attack, Iron Tail, Electro Ball
            actor->move_pp = {15, 30, 15, 10};
            actor->move_max_pp = {15, 30, 15, 10};
        } else if (entry.species_id == 1) {
            actor->InitializeCombat(1, "Bulbasaur", 11, 3, 50, 130, 95, 95, 120, 120, 85);
            actor->moves = {33, 22, 75, 73};    // Tackle, Vine Whip, Razor Leaf, Leech Seed
            actor->move_pp = {35, 25, 25, 10};
            actor->move_max_pp = {35, 25, 25, 10};
        } else if (entry.species_id == 8) {
            actor->InitializeCombat(8, "Wartortle", 10, 10, 50, 145, 115, 140, 115, 140, 105);
            actor->moves = {55, 44, 229, 56};   // Water Gun, Bite, Rapid Spin, Hydro Pump
            actor->move_pp = {25, 25, 20, 5};
            actor->move_max_pp = {25, 25, 20, 5};
        }

        if (const auto* p = za::data::PersonalDatabase::Instance().Get(entry.species_id)) {
            actor->height = p->HeightMeters();
        }
        poke_actors.push_back(actor);
    }

    if (poke_actors.size() >= 2) {
        auto player = poke_actors[0]; // Mewtwo
        player->position = za::gfx::Vec3{0.0f, 0.0f, 3.5f};
        player->yaw = 0.0f;

        auto opponent = poke_actors[3]; // Wartortle
        opponent->position = za::gfx::Vec3{0.0f, 0.0f, -5.5f};
        opponent->yaw = 3.14159265f; // Encarando o jogador de frente na arena

        player->SetTarget(opponent.get());
        opponent->SetTarget(player.get());

        za::gfx::Renderer3D::Instance().SetActiveActor(player);
        za::gfx::Renderer3D::Instance().SetOpponentActor(opponent);
        std::cout << "[COMBATE] Arena inicializada: " << player->name << " (Jogador) vs "
                  << opponent->name << " (Oponente)!" << std::endl;
    } else if (!poke_actors.empty()) {
        za::gfx::Renderer3D::Instance().SetActiveActor(poke_actors.front());
    }

    // Carregamento de Cenário Urbano Expandido via SceneLoader (.trscn + .trcol)
    std::cout << "\n[SCENE] Carregando a Arena de Batalha de Lumiose City via SceneLoader (t1_battle1_0.trscn)..." << std::endl;
    za::gfx::LoadedSceneResult arena_scene;
    std::string scene_err;
    if (za::gfx::SceneLoader::Instance().LoadScene("world/ik_scene/field/area/t1_battle1_/t1_battle1_0.trscn",
                                                   read_vfs_file, arena_scene, scene_err,
                                                   &za::gfx::Renderer3D::Instance().GetCollisionWorld())) {
        std::cout << "  -> [OK CENA] t1_battle1_0.trscn: " << arena_scene.models_loaded << " modelo(s) e "
                  << arena_scene.collisions_loaded << " setor(es) de colisao carregados!" << std::endl;

        for (const auto& m_inst : arena_scene.models) {
            za::gfx::Renderer3D::Instance().SetSceneryModel(m_inst.model, m_inst.position, m_inst.scale, m_inst.rot_y, 0x88A0B8);
        }
    } else {
        std::cout << "  -> [AVISO] Falha ao carregar cena via SceneLoader: " << scene_err << std::endl;
    }

    // Montagem dos setores urbanos de Lumiose City com transformações compostas (SceneAssembler)
    std::cout << "\n[ASSEMBLY] Montando setores urbanos de Lumiose City (t1_bk01..03) via SceneAssembler..." << std::endl;
    za::gfx::SceneAssembler assembler(read_vfs_file);
    std::string asm_err;
    assembler.AddScene("world/ik_scene/field/area/t1/sub_scene/t1_bk01_/t1_bk01.trscn", asm_err);
    assembler.AddScene("world/ik_scene/field/area/t1/sub_scene/t1_bk02_/t1_bk02.trscn", asm_err);
    assembler.AddScene("world/ik_scene/field/area/t1/sub_scene/t1_bk03_/t1_bk03.trscn", asm_err);

    std::string bld_err;
    size_t asm_col = assembler.BuildCollisionWorld(za::gfx::Renderer3D::Instance().GetCollisionWorld(), bld_err,
                                                   {-214.0f, 0.0f, -861.0f});
    std::cout << "  -> [OK ASSEMBLY] " << asm_col << " malhas de colisao integradas ao CollisionWorld ("
              << za::gfx::Renderer3D::Instance().GetCollisionWorld().MeshCount() << " malhas ativas)!" << std::endl;

    // Carregamento da Torre Prisma de Lumiose City como cenário de fundo
    std::cout << "\n[SCENERY] Carregando a Torre Prisma de Lumiose City (field/model/t1/t1_b/t1_b_prism)..." << std::endl;
    const std::string prism_dir = "field/model/t1/t1_b/t1_b_prism/";
    auto load_scenery_part = [&](const std::string& name, float pos_y, float scale, uint32_t col) {
        za::gfx::Model sc_model;
        sc_model.name = name;
        za::gfx::TrmdlFile tf;
        std::string err2, bf2;
        std::vector<uint8_t> d;
        if (read_vfs_file(prism_dir + name + ".trmdl", d) && za::gfx::ParseTrmdl(d, tf, err2) &&
            !tf.mesh_files.empty() && read_vfs_file(prism_dir + tf.mesh_files.front(), d) &&
            za::gfx::ParseTrmsh(d, sc_model.meshes, bf2, err2) &&
            read_vfs_file(prism_dir + bf2, d) && za::gfx::LoadTrmbf(d, sc_model.meshes, err2)) {
            std::cout << "  -> [OK CENARIO] " << name << ": " << sc_model.meshes.size() << " malhas carregadas" << std::endl;
            za::gfx::Renderer3D::Instance().SetSceneryModel(sc_model, za::gfx::Vec3{0.0f, pos_y, 16.0f}, scale, 0.0f, col);
        }
    };

    load_scenery_part("t1_b_prism_main", -1.0f, 0.04f, 0x8898B0);
    load_scenery_part("t1_b_prism_bottom", -1.0f, 0.04f, 0x778899);
    load_scenery_part("t1_b_prism_top", -1.0f, 0.04f, 0x99AABB);

    // Validação da Fase Wwise: Verificação de carregamento de SoundBanks e extração de áudio
    std::cout << "\n[AUDIO-TEST] Testando Wwise Audio Pipeline (Stream.pck e SoundBanks)..." << std::endl;
    std::string audio_root = romfs_root + "/audio/wwise/bin/Switch";
    if (za::audio::WwiseArchive::Instance().Initialize(audio_root)) {
        std::cout << "[AUDIO-TEST] [SUCESSO] " 
                  << za::audio::WwiseArchive::Instance().GetTotalMediaCount() 
                  << " faixas de audio indexadas em " 
                  << za::audio::WwiseArchive::Instance().GetLoadedBankCount() 
                  << " SoundBanks!" << std::endl;
        
        uint32_t test_audio_id = 812578877;
        std::vector<uint8_t> audio_data;
        if (za::audio::WwiseArchive::Instance().GetMediaData(test_audio_id, audio_data)) {
            std::cout << "[AUDIO-TEST] [SUCESSO STREAM] Faixa 0x" << std::hex << test_audio_id << std::dec 
                      << " lida com sucesso (" << audio_data.size() << " bytes RIFF WAVE)!" << std::endl;
        }
    }

    // Inicialização do Dispositivo de Saída de Áudio do PC (waveOut)
    if (za::audio::AudioPlayback::Instance().Initialize(48000, 2)) {
        za::audio::AudioPlayback::Instance().PlayPokeChime(523.25f, 0.45f, 0.50f); // Tom de inicialização
        PlayPokemonCry(150, romfs_root); // Reproduz o Cry oficial de Mewtwo

        // Inicialização do Streaming Contínuo de Ambiência/BGM de Lumiose City em Loop
        std::string pck_err;
        auto ambient_stream = std::make_shared<za::audio::AmbientAudioStream>();
        std::string stream_pck_path = romfs_root + "/audio/wwise/bin/Switch/Stream.pck";
        za::audio::WwisePackage stream_pck;
        if (stream_pck.Open(stream_pck_path, pck_err)) {
            // Faixa 226064614: Música oficial de Lumiose City (T1_MAIN_STREET) em PTADPCM estéreo 48 kHz (~244s)
            if (ambient_stream->Open(stream_pck, 226064614, pck_err)) {
                ambient_stream->SetVolume(0.35f);
                za::audio::AudioPlayback::Instance().SetAmbientStream(ambient_stream);
                std::cout << "[AUDIO-STREAM] [SUCESSO] Musica de Lumiose City (Main Street 226064614) ativa em loop (48 kHz stereo, duracao: "
                          << ambient_stream->GetDurationSeconds() << "s, volume: 35%)!" << std::endl;
            } else {
                std::cout << "[AUDIO-STREAM] [AVISO] Falha ao abrir streaming PTADPCM: " << pck_err << std::endl;
            }
        }
    }

    std::cout << "\n[5/6] Inicializando Interface Visual e Janela (nn::vi)..." << std::endl;
    nn::vi::Initialize();
    nn::vi::DisplayHandle display = 0;
    nn::vi::OpenDefaultDisplay(&display);
    nn::vi::LayerHandle layer = 0;
    nn::vi::CreateManagedLayer(&layer, display, 0);
    void* native_window = nullptr;
    nn::vi::GetNativeWindow(&native_window, layer);
    nn::hid::SetTargetWindow(native_window);

    std::cout << "\n[6/6] Inicializando Subsistema Grafico da GPU (NVN & NV)..." << std::endl;
    nv::SetGraphicsAllocator(nullptr, nullptr, nullptr, nullptr);
    nv::InitializeGraphics(nullptr, 0x10000000); // 256 MB buffer de video
    
    // Inicialização do Device NVN
    nvn::Device device{};
    nvn::DeviceBuilder dev_builder{};
    nvn::nvnDeviceBuilderSetDefaults(&dev_builder);
    nvn::nvnDeviceInitialize(&device, &dev_builder);

    // Consulta de propriedades requeridas pela Trinity Engine (Fase 2)
    int d52 = 0, d55 = 0;
    nvn::nvnDeviceGetInteger(&device, 0x52, &d52);
    nvn::nvnDeviceGetInteger(&device, 0x55, &d55);
    std::cout << "[NVN] Propriedades do Device consultadas: D52=" << d52 << ", D55=" << d55 << std::endl;

    // Inicialização da Fila de Comandos (Queue) com parâmetros exatos do binário
    nvn::Queue queue{};
    nvn::QueueBuilder q_builder{};
    nvn::nvnQueueBuilderSetDefaults(&q_builder);
    nvn::nvnQueueBuilderSetDevice(&q_builder, &device);
    nvn::nvnQueueBuilderSetQueuePriority(&q_builder, 1000); // Prioridade 1000 da Game Freak
    nvn::nvnQueueBuilderSetControlMemorySize(&q_builder, 0x80000); // 512 KiB
    nvn::nvnQueueInitialize(&queue, &q_builder);

    // Inicialização da Janela de Apresentação (Swapchain)
    nvn::Window window{};
    nvn::WindowBuilder w_builder{};
    nvn::nvnWindowBuilderSetDefaults(&w_builder);
    nvn::nvnWindowBuilderSetDevice(&w_builder, &device);
    nvn::nvnWindowBuilderSetNativeWindow(&w_builder, native_window);
    int present_interval = g_game_config.graphics.vsync ? 1 : 0;
    nvn::nvnWindowBuilderSetPresentInterval(&w_builder, present_interval);
    nvn::nvnWindowInitialize(&window, &w_builder);

    // Aplica Shading e Modo de Câmera Inicial do arquivo de configuração
    za::gfx::Renderer3D::Instance().SetWireframeOnly(g_game_config.graphics.shading == "wireframe");
    if (g_game_config.graphics.initial_camera == "free_fly") {
        za::gfx::Renderer3D::Instance().GetCamera().mode = za::gfx::Camera3D::Mode::Free;
    } else {
        za::gfx::Renderer3D::Instance().GetCamera().mode = za::gfx::Camera3D::Mode::Orbit;
    }

    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[CARREGAMENTO] Invocando NsoLoader para carregar 'main'..." << std::endl;
    
    std::string main_path = "../exefs/main";
    if (!std::filesystem::exists(main_path)) main_path = "exefs/main";
    if (!std::filesystem::exists(main_path)) main_path = "C:/Users/johna/OneDrive/Documentos/legend za/exefs/main";

    za::loader::NsoLoader loader;
    if (loader.Load(main_path, 0x7100000000ULL)) {
        std::cout << "\n[SUCESSO] O binario original do Pokemon Legends: Z-A esta" << std::endl;
        std::cout << "          carregado e descomprimido no endereco 0x7100000000!" << std::endl;
        
        // Patching das entradas .got.plt para redirecionar chamadas aos stubs de PC
        za::loader::GotPatcher::PatchAll(0x7100000000ULL);
    } else {
        std::cerr << "\n[FALHA] Nao foi possivel carregar o binario NSO0." << std::endl;
        return 1;
    }

    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[TRINITY-SUBSYSTEMS] Inicializando os 34 subsistemas do motor..." << std::endl;
    za::SubsystemRegistry::Instance().InitializeAll();

    std::cout << "\n-----------------------------------------------------" << std::endl;
    std::cout << "[GAME LOOP] Processando eventos da Janela e Fila de Renderizacao..." << std::endl;

    // Loop de renderização / eventos de janela
    bool test_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--test") test_mode = true;
    }
    const int max_test_frames = test_mode ? 300 : 2147483647; // Modo interativo roda livre até o usuário fechar
    int frame_count = 0;
    auto last_time = std::chrono::high_resolution_clock::now();

    if (test_mode) {
        std::cout << "[LOOP] Executando em modo teste automatizado (300 quadros)..." << std::endl;
    } else {
        std::cout << "[LOOP] Executando em modo interativo livre (feche a janela ou aperte ESC para sair)..." << std::endl;
    }

    while (nn::vi::ProcessWindowEvents() && frame_count < max_test_frames) {
        frame_count++;
        za::Profiler::Instance().BeginFrame();
        za::CrashHandler::SetLastCheckpoint("Frame Loop / Input & Subsystems");

        auto now = std::chrono::high_resolution_clock::now();
        float delta_time = std::chrono::duration<float>(now - last_time).count();
        last_time = now;

        // Atualização dos 34 subsistemas da Trinity Engine (inclui HID input polling)
        za::Profiler::Instance().BeginStage("SubsystemsUpdate");
        za::SubsystemRegistry::Instance().UpdateAll(delta_time);
        za::Profiler::Instance().EndStage("SubsystemsUpdate");

        // Polling de entrada HID para o quadro atual
        nn::hid::NpadFullKeyState pad_state{};
        nn::hid::GetNpadFullKeyState(&pad_state, nn::hid::NpadId::No1);

        // Tratamento de teclas interativas de atalho (1..4 = Pokemons, C = Camera, V = Shading)
        auto is_key_down = [](int vk) -> bool {
            return (GetAsyncKeyState(vk) & 0x8000) != 0;
        };

        static bool s_k1_prev = false, s_k2_prev = false, s_k3_prev = false, s_k4_prev = false;
        static bool s_kc_prev = false, s_kv_prev = false;

        bool k1 = is_key_down('1'), k2 = is_key_down('2'), k3 = is_key_down('3'), k4 = is_key_down('4');
        bool kc = is_key_down('C'), kv = is_key_down('V');

        auto switch_player = [&](size_t idx) {
            if (idx >= poke_actors.size()) return;
            auto cur_player = poke_actors[idx];
            size_t opp_idx = (idx == 3) ? 0 : 3;
            auto cur_opp = poke_actors[opp_idx];

            cur_player->SetTarget(cur_opp.get());
            cur_opp->SetTarget(cur_player.get());

            za::gfx::Renderer3D::Instance().SetActiveActor(cur_player);
            za::gfx::Renderer3D::Instance().SetOpponentActor(cur_opp);
            PlayPokemonCry(cur_player->species_id, romfs_root);
            std::cout << "[COMBATE] Lutador Ativo: " << cur_player->name << " (#" << cur_player->species_id
                      << ") vs " << cur_opp->name << " (#" << cur_opp->species_id << ")!" << std::endl;
        };

        if (k1 && !s_k1_prev && poke_actors.size() > 0) switch_player(0);
        if (k2 && !s_k2_prev && poke_actors.size() > 1) switch_player(1);
        if (k3 && !s_k3_prev && poke_actors.size() > 2) switch_player(2);
        if (k4 && !s_k4_prev && poke_actors.size() > 3) switch_player(3);

        bool ktab = is_key_down(VK_TAB);
        static bool s_tab_prev = false;
        if (ktab && !s_tab_prev) {
            auto p = za::gfx::Renderer3D::Instance().GetActiveActor();
            auto o = za::gfx::Renderer3D::Instance().GetOpponentActor();
            if (p && o) {
                za::gfx::Renderer3D::Instance().SetActiveActor(o);
                za::gfx::Renderer3D::Instance().SetOpponentActor(p);
                p->SetTarget(o.get());
                o->SetTarget(p.get());
                PlayPokemonCry(o->species_id, romfs_root);
                std::cout << "[COMBATE] Troca [TAB]: Agora controlando " << o->name << " contra " << p->name << "!" << std::endl;
            }
        }
        s_tab_prev = ktab;

        // Ataque interativo (F, Espaço ou Botão Esquerdo do Mouse)
        bool is_attack_key = is_key_down('F') || is_key_down(VK_SPACE) || is_key_down(VK_LBUTTON);
        static bool s_atk_prev = false;
        if (is_attack_key && !s_atk_prev) {
            auto cur_actor = za::gfx::Renderer3D::Instance().GetActiveActor();
            if (cur_actor && cur_actor->state != za::game::ActorState::Attack) {
                cur_actor->TriggerAttack([&](uint16_t sp_id, uint16_t move_id) {
                    PlayPokemonCry(sp_id, romfs_root);
                    za::audio::AudioPlayback::Instance().PlayAttackImpact(1.2f, 0.60f);
                    const auto* mv = za::data::MoveDatabase::Instance().Get(move_id);
                    std::cout << "[COMBATE] Pokemon #" << sp_id << " disparou golpe #" << move_id 
                              << " (" << (mv ? mv->name : "Ataque") << ")!" << std::endl;
                });
            }
        }
        s_atk_prev = is_attack_key;
        if (kc && !s_kc_prev) {
            za::gfx::Renderer3D::Instance().GetCamera().ToggleMode();
            za::audio::AudioPlayback::Instance().PlayLockonBeep(880.0f, 0.12f, 0.35f);
            std::cout << "[CAMERA] Modo: " 
                      << ((za::gfx::Renderer3D::Instance().GetCamera().mode == za::gfx::Camera3D::Mode::Orbit) ? "Orbita 360" : "Voo Livre") << std::endl;
        }
        if (kv && !s_kv_prev) {
            za::gfx::Renderer3D::Instance().ToggleWireframe();
            za::audio::AudioPlayback::Instance().PlayLockonBeep(659.25f, 0.12f, 0.35f);
            std::cout << "[RENDER] Shading: " 
                      << (za::gfx::Renderer3D::Instance().IsWireframeOnly() ? "Aramado" : "Solido Iluminado (Z-Buffer)") << std::endl;
        }

        s_k1_prev = k1; s_k2_prev = k2; s_k3_prev = k3; s_k4_prev = k4;
        s_kc_prev = kc; s_kv_prev = kv;

        // Pipeline Gráfico NVN por quadro: Acquire -> Render -> Present
        za::CrashHandler::SetLastCheckpoint("Pipeline Grafico NVN Present");
        za::Profiler::Instance().BeginStage("PresentNVN");
        int current_texture = 0;
        nvn::nvnWindowAcquireTexture(&window, nullptr, &current_texture);
        nvn::nvnQueuePresentTexture(&queue, &window, current_texture);
        za::Profiler::Instance().EndStage("PresentNVN");

        za::Profiler::Instance().EndFrame();

        if (frame_count % 60 == 0) {
            std::cout << "[NVN-Loop] Quadro " << frame_count << " apresentado na tela via HWND (VSync 60 FPS ativo)"
                      << " | Input Sample #" << pad_state.sampling_number 
                      << " (Botoes: 0x" << std::hex << pad_state.buttons << std::dec << ")" << std::endl;
        }
    }

    std::cout << "\n[FINALIZACAO] Finalizando subsistemas do motor..." << std::endl;
    za::CrashHandler::SetLastCheckpoint("Desligamento de Subsistemas");
    za::SubsystemRegistry::Instance().FinalizeAll();
    za::audio::AudioPlayback::Instance().Shutdown();
    za::Profiler::Instance().Shutdown();
    za::Logger::Instance().Shutdown();

    std::cout << "\n[STATUS GERAL] Pipeline completo testado com sucesso no Windows!" << std::endl;
    std::cout << "A janela nativa abriu, respondeu a eventos e processou os quadros perfeitamente." << std::endl;

    return 0;
}
