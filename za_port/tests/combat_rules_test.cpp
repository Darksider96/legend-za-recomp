// Testes das regras de combate (estagios de atributos, precisao, critico) e dos eventos Wwise de SFX de batalha
// uso: combat_rules_test [romfs_dir]   (com romfs, confere os eventos no HIRC dos SoundBanks)

#include "audio/wwise.hpp"
#include "game/stat_stages.hpp"

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

static_assert(game::StatStageRatio(-6).numerator == 2 && game::StatStageRatio(-6).denominator == 8);
static_assert(game::StatStageRatio(6).numerator == 8 && game::StatStageRatio(6).denominator == 2);
static_assert(game::ApplyStatStage(100, 1) == 150 && game::ApplyStatStage(100, -1) == 66);

void TestStatStages() {
    // k = -6..+6: 2/8 2/7 2/6 2/5 2/4 2/3 2/2 3/2 4/2 5/2 6/2 7/2 8/2
    bool all = true;
    for (int k = -6; k <= 6; ++k) {
        const float expected = k >= 0 ? (2.0f + k) / 2.0f : 2.0f / (2.0f - k);
        all = all && std::abs(game::StatStageMultiplier(k) - expected) < 1e-6f;
    }
    Check(all, "13 multiplicadores de atributo (-6..+6) = max(2, 2+k) / max(2, 2-k)");
    Check(game::StatStageMultiplier(-6) == 0.25f && std::abs(game::StatStageMultiplier(-1) - 2.0f / 3.0f) < 1e-6f &&
              game::StatStageMultiplier(0) == 1.0f && game::StatStageMultiplier(1) == 1.5f && game::StatStageMultiplier(2) == 2.0f &&
              game::StatStageMultiplier(6) == 4.0f, "Valores de referencia: -6 = 0,25x, -1 = 0,667x, 0 = 1x, +1 = 1,5x, +2 = 2x, +6 = 4x");
    Check(game::StatStageMultiplier(9) == 4.0f && game::StatStageMultiplier(-9) == 0.25f, "Estagios fora de -6..+6 sao limitados");
    Check(game::ApplyStatStage(137, 2) == 274 && game::ApplyStatStage(137, -2) == 68 && game::ApplyStatStage(137, -6) == 34,
          "ApplyStatStage trunca stat * num / den (137: +2 = 274, -2 = 68, -6 = 34)");
}

void TestAccuracy() {
    bool all = true;
    for (int k = -6; k <= 6; ++k) {
        const float expected = k >= 0 ? (3.0f + k) / 3.0f : 3.0f / (3.0f - k);
        all = all && std::abs(game::AccuracyStageMultiplier(k) - expected) < 1e-6f;
    }
    Check(all && std::abs(game::AccuracyStageMultiplier(-6) - 1.0f / 3.0f) < 1e-6f && game::AccuracyStageMultiplier(6) == 3.0f,
          "13 multiplicadores de precisao/evasao = max(3, 3+k) / max(3, 3-k) (-6 = 1/3, +6 = 3x)");
    Check(game::CombinedAccuracyStage(2, 1) == 1 && game::CombinedAccuracyStage(6, -6) == 6 && game::CombinedAccuracyStage(-4, 5) == -6,
          "CombinedAccuracyStage: precisao - evasao limitado a -6..+6");
}

void TestCritical() {
    Check(std::abs(game::CriticalHitChance(0).Value() - 1.0f / 24) < 1e-7f && game::CriticalHitChance(1).Value() == 0.125f &&
              game::CriticalHitChance(2).Value() == 0.5f && game::CriticalHitChance(3).Value() == 1.0f &&
              game::CriticalHitChance(5).Value() == 1.0f && game::kCriticalDamageMultiplier == 1.5f,
          "Critico: 1/24, 1/8, 1/2, garantido a partir do estagio 3; dano 1,5x");
    // Frequencia exata sobre todos os restos possiveis
    unsigned hits[3] = {};
    for (uint32_t roll = 0; roll < 24 * 8 * 2 * 100; ++roll) {
        for (int stage = 0; stage < 3; ++stage) hits[stage] += game::IsCriticalHit(stage, roll) ? 1 : 0;
    }
    Check(hits[0] == 1600 && hits[1] == 4800 && hits[2] == 19200 && game::IsCriticalHit(3, 12345),
          "IsCriticalHit: 1600/4800/19200 criticos em 38400 sorteios (1/24, 1/8, 1/2)");
}

void TestSfxEvents(const std::filesystem::path& romfs) {
    // Hashes FNV-1 conferidos com uma implementacao independente em Python
    Check(audio::WwiseHash("PLAY_SE_BT_HIT_NORMAL") == 644100257u && audio::WwiseHash("PLAY_SE_BT_HIT_SUPER") == 609149001u &&
              audio::WwiseHash("PLAY_SE_BT_HIT_NOTVERY") == 4022121151u && audio::WwiseHash("PLAY_SE_BT_CRITICAL") == 3546451193u &&
              audio::WwiseHash("PLAY_BA_SYS_HIT_L") == 3311143776u,
          "WwiseHash dos eventos de SFX de combate");
    if (romfs.empty()) return;

    const std::filesystem::path dir = romfs / "audio" / "wwise" / "bin" / "Switch";
    size_t banks = 0, requested_found = 0;
    std::string error;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() != ".bnk") continue;
        audio::WwiseBank bank;
        if (!bank.Load(entry.path().string(), error)) continue;
        ++banks;
        for (const char* name : {"PLAY_SE_BT_HIT_NORMAL", "PLAY_SE_BT_HIT_SUPER", "PLAY_SE_BT_HIT_NOTVERY", "PLAY_SE_BT_CRITICAL"}) {
            requested_found += bank.FindObject(audio::WwiseHash(name)) ? 1 : 0;
        }
    }
    Check(banks == 16 && !std::filesystem::exists(dir / "SE.bnk") && requested_found == 0,
          "Os 16 SoundBanks (nao ha SE.bnk) nao contem PLAY_SE_BT_HIT_NORMAL/SUPER/NOTVERY nem PLAY_SE_BT_CRITICAL");

    audio::WwiseBank battle;
    audio::WwiseSoundSource hit_l, hit_m, down;
    const bool resolved = battle.Load((dir / "BATTLE_SYSTEM.bnk").string(), error) &&
                          audio::ResolveEventSound(battle, audio::WwiseHash("PLAY_BA_SYS_HIT_L"), hit_l, error) &&
                          audio::ResolveEventSound(battle, audio::WwiseHash("PLAY_BA_SYS_HIT_M"), hit_m, error) &&
                          audio::ResolveEventSound(battle, audio::WwiseHash("PLAY_BA_SYS_POKE_DOWN"), down, error);
    Check(resolved && hit_l.InStreamPackage() && hit_l.media_id == 656051336u && hit_m.media_id == 362307504u &&
              down.media_id == 723387808u, "BATTLE_SYSTEM.bnk: PLAY_BA_SYS_HIT_L/M e POKE_DOWN resolvem para faixas do Stream.pck");
}

} // namespace

int main(int argc, char** argv) {
    TestStatStages();
    TestAccuracy();
    TestCritical();
    TestSfxEvents(argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path());
    if (argc <= 1) std::puts("(sem <romfs_dir>: conferencia dos SoundBanks ignorada)");
    std::printf("%s (%d falhas)\n", g_failures == 0 ? "OK" : "FALHOU", g_failures);
    return g_failures == 0 ? 0 : 1;
}
