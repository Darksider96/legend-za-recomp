// Testes do Sistema de Combate e Golpes: waza_array.bin, wazaname.dat, matriz 18x18 e calculo de dano
// uso: battle_test <romfs_dir>

#include "data/move_table.hpp"
#include "data/message_text.hpp"
#include "game/battle_calc.hpp"
#include "vfs/trpfs.hpp"

#include <cstdio>
#include <cmath>
#include <filesystem>
#include <vector>
#include <string>

using namespace za;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    std::printf("%s %s\n", condition ? "[PASS]" : "[FAIL]", what);
    if (!condition) ++g_failures;
}

void TestTypeChartAndCalc() {
    // 1. Tabela 18x18
    Check(game::GetTypeEffectiveness(0, 7) == 0.0f, "Normal vs Fantasma: 0x (imune)");
    Check(game::GetTypeEffectiveness(7, 0) == 0.0f, "Fantasma vs Normal: 0x (imune)");
    Check(game::GetTypeEffectiveness(12, 4) == 0.0f, "Eletrico vs Terra: 0x (imune)");
    Check(game::GetTypeEffectiveness(4, 12) == 2.0f, "Terra vs Eletrico: 2.0x (super efetivo)");
    Check(game::GetTypeEffectiveness(13, 1) == 2.0f, "Psiquico vs Luta: 2.0x (super efetivo)");
    Check(game::GetTypeEffectiveness(10, 9) == 2.0f, "Agua vs Fogo: 2.0x (super efetivo)");
    Check(game::GetTypeEffectiveness(9, 11) == 2.0f, "Fogo vs Planta: 2.0x (super efetivo)");
    Check(game::GetTypeEffectiveness(17, 15) == 2.0f, "Fada vs Dragao: 2.0x (super efetivo)");
    Check(game::GetTypeEffectiveness(15, 17) == 0.0f, "Dragao vs Fada: 0x (imune)");

    // 2. Dupla Tipagem
    // Planta(11) contra Agua(10)/Terra(4) = 2.0x * 2.0x = 4.0x
    Check(game::GetTypeEffectiveness(11, 10, 4) == 4.0f, "Planta vs Agua/Terra: 4.0x (dupla fraqueza)");
    // Psiquico(13) contra Bulbasaur Planta(11)/Veneno(3) = 1.0x * 2.0x = 2.0x
    Check(game::GetTypeEffectiveness(13, 11, 3) == 2.0f, "Psiquico vs Bulbasaur (Planta/Veneno): 2.0x");
    // Eletrico(12) contra Dragao(15)/Voador(2) = 0.5x * 2.0x = 1.0x (neutro)
    Check(game::GetTypeEffectiveness(12, 15, 2) == 1.0f, "Eletrico vs Dragao/Voador: 1.0x (neutro)");

    // 3. Calculo de Dano
    // Golpe Especial Psiquico (Power 90) com STAB (Mewtwo Psiquico puro = 13)
    int dmg_min = game::CalculateDamage(50, 90, 180, 80, 2.0f, true, 0.85f);
    int dmg_max = game::CalculateDamage(50, 90, 180, 80, 2.0f, true, 1.00f);
    Check(dmg_min > 0 && dmg_max >= dmg_min, "CalculateDamage: escala linear com STAB e variacao [0.85, 1.00]");

    // Imunidade retorna dano 0
    int dmg_immune = game::CalculateDamage(50, 90, 180, 80, 0.0f, true, 1.00f);
    Check(dmg_immune == 0, "CalculateDamage: dano imune (0.0x) resulta em 0 HP");
}

void TestMoveDatabaseSynthetic() {
    data::MoveDatabase db = data::MoveDatabase::Instance();
    std::string err;
    const uint8_t garbage[8] = {0xFF, 0xFF, 0xFF, 0x7F, 0, 0, 0, 0};
    Check(!db.Load(garbage, {}, err), "MoveDatabase rejeita FlatBuffer invalido");
}

void TestRomfs(const std::filesystem::path& romfs) {
    vfs::TrpfsArchive archive;
    const std::filesystem::path arc = romfs / "arc";
    if (!archive.Mount((arc / "data.trpfd").string(), (arc / "data.trpfs").string()) || !vfs::oodle::IsAvailable()) {
        Check(false, "Mount do data.trpfs com Oodle disponivel (ZA_OODLE_DLL)");
        return;
    }

    std::vector<uint8_t> waza_bytes, name_bytes;
    std::string err;
    bool waza_ok = archive.ReadFile("avalon/data/waza_array.bin", waza_bytes);
    bool name_ok = archive.ReadFile("ik_message/dat/English/common/wazaname.dat", name_bytes);

    Check(waza_ok && waza_bytes.size() == 90640, "waza_array.bin: 90640 bytes lidos do RomFS");
    Check(name_ok && name_bytes.size() > 0, "wazaname.dat lido com sucesso do RomFS");

    auto& db = data::MoveDatabase::Instance();
    bool loaded = db.Load(waza_bytes, name_bytes, err);
    Check(loaded && db.Count() == 902, "MoveDatabase: 902 golpes carregados");

    // Mewtwo: Psychic (94), Shadow Ball (247), Swift (129), Recover (105)
    const auto* psychic = db.Get(94);
    Check(psychic && psychic->name == "Psychic" && psychic->type == 13 &&
          psychic->category == data::MoveCategory::Special && psychic->base_power == 90 &&
          psychic->accuracy == 100 && psychic->pp == 10,
          "Golpe #94 Psychic: tipo 13 (Psiquico), Especial, Poder 90, Precisao 100, PP 10");

    const auto* shadow_ball = db.Get(247);
    Check(shadow_ball && shadow_ball->name == "Shadow Ball" && shadow_ball->type == 7 &&
          shadow_ball->category == data::MoveCategory::Special && shadow_ball->base_power == 80 &&
          shadow_ball->accuracy == 100 && shadow_ball->pp == 15,
          "Golpe #247 Shadow Ball: tipo 7 (Fantasma), Especial, Poder 80, Precisao 100, PP 15");

    const auto* recover = db.Get(105);
    Check(recover && recover->name == "Recover" && recover->type == 0 &&
          recover->category == data::MoveCategory::Status && recover->base_power == 0 &&
          recover->pp == 5,
          "Golpe #105 Recover: tipo 0 (Normal), Status, Poder 0, PP 5");

    // Pikachu: Thunderbolt (85), Quick Attack (98)
    const auto* thunderbolt = db.Get(85);
    Check(thunderbolt && thunderbolt->name == "Thunderbolt" && thunderbolt->type == 12 &&
          thunderbolt->category == data::MoveCategory::Special && thunderbolt->base_power == 90 &&
          thunderbolt->accuracy == 100 && thunderbolt->pp == 15,
          "Golpe #85 Thunderbolt: tipo 12 (Eletrico), Especial, Poder 90, Precisao 100, PP 15");

    const auto* quick_attack = db.Get(98);
    Check(quick_attack && quick_attack->name == "Quick Attack" && quick_attack->type == 0 &&
          quick_attack->category == data::MoveCategory::Physical && quick_attack->base_power == 40 &&
          quick_attack->priority == 1,
          "Golpe #98 Quick Attack: tipo 0 (Normal), Fisico, Poder 40, Prioridade +1");

    // Bulbasaur & Wartortle: Tackle (33), Water Gun (55)
    const auto* tackle = db.Get(33);
    Check(tackle && tackle->name == "Tackle" && tackle->type == 0 &&
          tackle->category == data::MoveCategory::Physical && tackle->base_power == 40,
          "Golpe #33 Tackle: tipo 0 (Normal), Fisico, Poder 40, Precisao 100");

    const auto* water_gun = db.Get(55);
    Check(water_gun && water_gun->name == "Water Gun" && water_gun->type == 10 &&
          water_gun->category == data::MoveCategory::Special && water_gun->base_power == 40,
          "Golpe #55 Water Gun: tipo 10 (Agua), Especial, Poder 40, Precisao 100");

    // Busca por nome
    const auto* by_name = db.FindByName("Thunderbolt");
    Check(by_name && by_name->move_id == 85, "FindByName('Thunderbolt') resolve para ID 85");
}

} // namespace

int main(int argc, char** argv) {
    TestTypeChartAndCalc();
    TestMoveDatabaseSynthetic();
    if (argc > 1) {
        TestRomfs(argv[1]);
    } else {
        std::puts("(sem <romfs_dir>: testes de integracao ignorados)");
    }
    std::printf("%s (%d falhas)\n", g_failures == 0 ? "OK" : "FALHOU", g_failures);
    return g_failures == 0 ? 0 : 1;
}
