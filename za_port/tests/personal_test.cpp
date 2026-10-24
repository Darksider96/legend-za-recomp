// Testes dos dados da Pokedex: personal_array.bin (atributos, tipos, dimensoes) e nomes de especies (monsname.dat)
// uso: personal_test <romfs_dir>   (le avalon/data/personal_array.bin do data.trpfs real; exige o Oodle)

#include "data/message_text.hpp"
#include "data/personal_table.hpp"
#include "vfs/trpfs.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>

using namespace za;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    std::printf("%s %s\n", condition ? "[PASS]" : "[FAIL]", what);
    if (!condition) ++g_failures;
}

struct Expected {
    uint16_t species;
    const char* name;
    data::PokemonStats stats;
    uint8_t type_1, type_2;
    uint16_t height_cm, weight_hg;
};

bool SameStats(const data::PokemonStats& a, const data::PokemonStats& b) {
    return a.hp == b.hp && a.attack == b.attack && a.defense == b.defense && a.sp_attack == b.sp_attack &&
           a.sp_defense == b.sp_defense && a.speed == b.speed;
}

void TestSynthetic() {
    data::PersonalDatabase db;
    std::string error;
    const uint8_t garbage[8] = {0xFF, 0xFF, 0xFF, 0x7F, 0, 0, 0, 0};
    Check(!db.Load(garbage, error) && db.Size() == 0, "PersonalDatabase rejeita FlatBuffer invalido");
    Check(data::PersonalDatabase::MakeKey(150, 1) == 1500001, "chave canonica = especie * 10000 + forma");
    Check(db.GetTypeName(13) == "Psíquico" && db.GetTypeName(17) == "Fada" && db.GetTypeName(18) == "?",
          "GetTypeName: tabela de 18 tipos (13 = Psiquico, 17 = Fada)");
}

void TestRomfs(const std::filesystem::path& romfs) {
    vfs::TrpfsArchive archive;
    const std::filesystem::path arc = romfs / "arc";
    if (!archive.Mount((arc / "data.trpfd").string(), (arc / "data.trpfs").string()) || !vfs::oodle::IsAvailable()) {
        Check(false, "Mount do data.trpfs com Oodle disponivel (ZA_OODLE_DLL)");
        return;
    }
    std::vector<uint8_t> bytes;
    std::string error;
    data::PersonalDatabase db;
    const bool loaded = archive.ReadFile("avalon/data/personal_array.bin", bytes) && bytes.size() == 377176 && db.Load(bytes, error);
    Check(loaded && db.Size() == 1422, "personal_array.bin: 377176 bytes, exatamente 1422 registros");

    const Expected expected[] = {
        {150, "Mewtwo", {106, 110, 90, 154, 90, 130}, 13, 13, 200, 1220},
        {25, "Pikachu", {35, 55, 40, 50, 50, 90}, 12, 12, 40, 60},
        {1, "Bulbasaur", {45, 49, 49, 65, 65, 45}, 11, 3, 70, 69},
        {8, "Wartortle", {59, 63, 80, 65, 80, 58}, 10, 10, 100, 225},
    };
    for (const Expected& e : expected) {
        const data::PokemonPersonalData* p = db.Get(e.species);
        const bool ok = p && p->national_dex == e.species && SameStats(p->base_stats, e.stats) && p->type_1 == e.type_1 &&
                        p->type_2 == e.type_2 && p->height_cm == e.height_cm && p->weight_hg == e.weight_hg;
        const std::string what = std::string(e.name) + ": atributos, tipos, altura e peso canonicos";
        Check(ok, what.c_str());
    }
    const data::PokemonPersonalData* bulbasaur = db.Get(1);
    Check(bulbasaur && std::abs(bulbasaur->HeightMeters() - 0.7f) < 1e-6f && std::abs(bulbasaur->WeightKg() - 6.9f) < 1e-5f &&
              db.GetTypeName(bulbasaur->type_1) == "Planta" && db.GetTypeName(bulbasaur->type_2) == "Veneno",
          "Bulbasaur: 0,7 m, 6,9 kg, Planta/Veneno");

    const data::PokemonPersonalData* pikachu = db.Get(25);
    const data::PokemonPersonalData* mewtwo = db.Get(150);
    Check(pikachu && mewtwo && pikachu->catch_rate == 190 && pikachu->hatch_cycles == 10 && pikachu->ev_yield.speed == 2 &&
              mewtwo->hatch_cycles == 120 && mewtwo->base_friendship == 0 && mewtwo->ev_yield.sp_attack == 3 &&
              mewtwo->egg_group_1 == 15 && mewtwo->base_stats.Total() == 680,
          "Captura, ciclos de ovo, amizade, EVs e grupo de ovo (Pikachu, Mewtwo)");

    const data::PokemonPersonalData* mega_x = db.Get(150, 1);
    Check(mega_x && SameStats(mega_x->base_stats, {106, 190, 100, 154, 100, 130}) && mega_x->type_2 == 1 &&
              db.Forms(150).size() == 3, "Mewtwo forma 1 (Mega X): Psiquico/Luta, 190 de ataque; 3 formas");

    // Id interno x nacional: 1000 = Tinkatink (nacional 957)
    const data::PokemonPersonalData* tinkatink = db.Get(1000);
    const data::PokemonPersonalData* by_dex = db.FindByNationalDex(957);
    Check(tinkatink && tinkatink->national_dex == 957 && by_dex == tinkatink && tinkatink->type_1 == 17 && tinkatink->type_2 == 8,
          "Id interno 1000 = n. nacional 957 (Fada/Aco); FindByNationalDex resolve de volta");

    std::vector<std::string> names;
    const bool names_ok = archive.ReadFile("ik_message/dat/English/common/monsname.dat", bytes) &&
                          data::ParseMessageText(bytes, names, error);
    Check(names_ok && names.size() == 1026 && names[1] == "Bulbasaur" && names[150] == "Mewtwo" && names[1000] == "Tinkatink",
          "monsname.dat decifrado: 1026 linhas indexadas pelo id interno");
}

} // namespace

int main(int argc, char** argv) {
    TestSynthetic();
    if (argc > 1) {
        TestRomfs(argv[1]);
    } else {
        std::puts("(sem <romfs_dir>: testes de integracao ignorados)");
    }
    std::printf("%s (%d falhas)\n", g_failures == 0 ? "OK" : "FALHOU", g_failures);
    return g_failures == 0 ? 0 : 1;
}
