#pragma once
// Estagios de atributos (-6..+6), precisao/evasao e taxa de golpe critico
//
// Regras canonicas das geracoes principais (Gen 7+); NAO conferidas contra o executavel do Z-A, cujo combate
// em tempo real pode usar outra tabela.
//   Atributos (Ataque, Defesa, Atq. Esp, Def. Esp, Velocidade): max(2, 2 + k) / max(2, 2 - k)
//   Precisao e evasao:                                          max(3, 3 + k) / max(3, 3 - k)
//   Critico por estagio: 0 = 1/24, 1 = 1/8, 2 = 1/2, 3+ = garantido; dano critico 1,5x

#include <algorithm>
#include <cstdint>

namespace za::game {

inline constexpr int kMinStage = -6;
inline constexpr int kMaxStage = 6;
inline constexpr float kCriticalDamageMultiplier = 1.5f;

struct StageRatio {
    int numerator{1};
    int denominator{1};

    constexpr float Value() const { return static_cast<float>(numerator) / static_cast<float>(denominator); }
};

constexpr int ClampStage(int stage) { return std::clamp(stage, kMinStage, kMaxStage); }

constexpr StageRatio StatStageRatio(int stage) {
    const int k = ClampStage(stage);
    return {std::max(2, 2 + k), std::max(2, 2 - k)};
}

constexpr StageRatio AccuracyStageRatio(int stage) {
    const int k = ClampStage(stage);
    return {std::max(3, 3 + k), std::max(3, 3 - k)};
}

constexpr float StatStageMultiplier(int stage) { return StatStageRatio(stage).Value(); }
constexpr float AccuracyStageMultiplier(int stage) { return AccuracyStageRatio(stage).Value(); }

// Atributo efetivo com truncamento inteiro, como nos jogos: stat * numerador / denominador
constexpr uint32_t ApplyStatStage(uint32_t stat, int stage) {
    const StageRatio ratio = StatStageRatio(stage);
    return stat * static_cast<uint32_t>(ratio.numerator) / static_cast<uint32_t>(ratio.denominator);
}

// Estagio usado no teste de acerto: precisao do atacante menos evasao do alvo, limitado a -6..+6
constexpr int CombinedAccuracyStage(int accuracy_stage, int evasion_stage) {
    return ClampStage(ClampStage(accuracy_stage) - ClampStage(evasion_stage));
}

constexpr StageRatio CriticalHitChance(int stage) {
    if (stage <= 0) return {1, 24};
    if (stage == 1) return {1, 8};
    if (stage == 2) return {1, 2};
    return {1, 1};
}

// Sorteio deterministico: roll uniforme (qualquer u32); critico se roll % denominador < numerador
constexpr bool IsCriticalHit(int stage, uint32_t roll) {
    const StageRatio chance = CriticalHitChance(stage);
    return roll % static_cast<uint32_t>(chance.denominator) < static_cast<uint32_t>(chance.numerator);
}

} // namespace za::game
