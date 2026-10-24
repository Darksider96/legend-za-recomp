// Testes do LearnsetDatabase: tabela canonica (buffer vazio) e learnset real do personal_array.bin
// uso: learnset_test [romfs_dir]   (com romfs, le avalon/data/personal_array.bin do data.trpfs; exige o Oodle)

#include "data/learnset_table.hpp"
#include "vfs/trpfs.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>

using namespace za;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    std::printf("%s %s\n", condition ? "[PASS]" : "[FAIL]", what);
    if (!condition) ++g_failures;
}

bool Contains(const std::vector<uint16_t>& moves, uint16_t move) {
    return std::find(moves.begin(), moves.end(), move) != moves.end();
}

bool HasMove(const std::vector<data::LevelMove>& moves, uint16_t move) {
    return std::any_of(moves.begin(), moves.end(), [move](const data::LevelMove& m) { return m.move_id == move; });
}

void TestCanonical() {
    data::LearnsetDatabase& db = data::LearnsetDatabase::Instance();
    std::string error;
    Check(db.Load({}, error) && db.IsCanonicalFallback() && db.SpeciesCount() == 4, "Load com buffer vazio: tabela canonica de 4 especies");

    const std::vector<uint16_t> mewtwo = db.GetMovesAtLevel(150, 50);
    Check(mewtwo.size() == 4 && Contains(mewtwo, 247) && Contains(mewtwo, 105) && mewtwo.back() == 247,
          "GetMovesAtLevel(150, 50): 4 golpes com Recover (105) e Shadow Ball (247) por ultimo");
    const std::vector<uint16_t> pikachu = db.GetMovesAtLevel(25, 50);
    Check(pikachu.size() == 4 && Contains(pikachu, 85), "GetMovesAtLevel(25, 50): 4 golpes com Thunderbolt (85)");
    const std::vector<data::LevelMove> bulbasaur = db.GetLevelMoves(1);
    Check(bulbasaur.size() == 5 && HasMove(bulbasaur, 22) && HasMove(bulbasaur, 75), "GetLevelMoves(1): Vine Whip (22) e Razor Leaf (75)");
    Check(db.GetMovesAtLevel(8, 1) == std::vector<uint16_t>{33, 39, 55}, "GetMovesAtLevel(8, 1): Tackle, Tail Whip, Water Gun");
    Check(db.GetLevelMoves(9999).empty() && db.GetMovesAtLevel(9999, 100).empty(), "Especie sem registro devolve vetor vazio");
}

void TestRomfs(const std::filesystem::path& romfs) {
    vfs::TrpfsArchive archive;
    std::vector<uint8_t> bytes;
    if (!archive.Mount((romfs / "arc" / "data.trpfd").string(), (romfs / "arc" / "data.trpfs").string()) ||
        !vfs::oodle::IsAvailable() || !archive.ReadFile("avalon/data/personal_array.bin", bytes)) {
        Check(false, "personal_array.bin lido do data.trpfs (ZA_OODLE_DLL)");
        return;
    }
    data::LearnsetDatabase db;
    std::string error;
    Check(db.Load(bytes, error) && !db.IsCanonicalFallback() && db.SpeciesCount() > 1000,
          "Load(personal_array.bin): learnsets reais de mais de 1000 especies");

    const std::vector<data::LevelMove> bulbasaur = db.GetLevelMoves(1);
    Check(bulbasaur.size() == 15 && bulbasaur[2].move_id == 22 && bulbasaur[2].level == 3 && bulbasaur[2].mastery_level == 6 &&
              bulbasaur[5].move_id == 75 && bulbasaur[5].level == 12,
          "Bulbasaur real: 15 golpes, Vine Whip nv 3 (dominio 6), Razor Leaf nv 12");
    Check(db.GetMovesAtLevel(150, 50) == std::vector<uint16_t>{219, 133, 396, 94},
          "Mewtwo real no nv 50: Safeguard, Amnesia, Aura Sphere, Psychic (Recover so no nv 80)");
    Check(Contains(db.GetMovesAtLevel(25, 50), 85), "Pikachu real no nv 50 inclui Thunderbolt (85, aprendido no nv 25)");
}

} // namespace

int main(int argc, char** argv) {
    TestCanonical();
    if (argc > 1) {
        TestRomfs(argv[1]);
    } else {
        std::puts("(sem <romfs_dir>: teste do learnset real ignorado)");
    }
    std::printf("%s (%d falhas)\n", g_failures == 0 ? "OK" : "FALHOU", g_failures);
    return g_failures == 0 ? 0 : 1;
}
