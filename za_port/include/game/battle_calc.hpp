#pragma once
#include <cstdint>
#include <array>
#include <string>
#include <cmath>
#include <algorithm>

namespace za::game {

// Tabela de 18 tipos elementares canônicos
constexpr size_t kTypeCount = 18;

// Matriz de Multiplicadores de Dano Elementar (Atacante x Defensor)
// 0 = 0.0x (Imune), 1 = 0.5x (Pouco efetivo), 2 = 1.0x (Neutro), 4 = 2.0x (Super efetivo)
inline constexpr uint8_t kTypeChart[kTypeCount][kTypeCount] = {
    // Nor Fig Fly Poi Gro Roc Bug Gho Ste Fir Wat Gra Ele Psy Ice Dra Dar Fai
    /*Normal  */ { 2,  2,  2,  2,  2,  1,  2,  0,  1,  2,  2,  2,  2,  2,  2,  2,  2,  2 },
    /*Fighting*/ { 4,  2,  1,  1,  2,  4,  1,  0,  4,  2,  2,  2,  2,  1,  4,  2,  4,  1 },
    /*Flying  */ { 2,  4,  2,  2,  2,  1,  4,  2,  1,  2,  2,  4,  1,  2,  2,  2,  2,  2 },
    /*Poison  */ { 2,  2,  2,  1,  1,  1,  2,  1,  0,  2,  2,  4,  2,  2,  2,  2,  2,  4 },
    /*Ground  */ { 2,  2,  0,  4,  2,  4,  1,  2,  4,  4,  2,  1,  4,  2,  2,  2,  2,  2 },
    /*Rock    */ { 2,  1,  4,  2,  1,  2,  4,  2,  1,  4,  2,  2,  2,  2,  4,  2,  2,  2 },
    /*Bug     */ { 2,  1,  1,  1,  2,  2,  2,  1,  1,  1,  2,  4,  2,  4,  2,  2,  4,  1 },
    /*Ghost   */ { 0,  2,  2,  2,  2,  2,  2,  4,  2,  2,  2,  2,  2,  4,  2,  2,  1,  2 },
    /*Steel   */ { 2,  2,  2,  2,  2,  4,  2,  2,  1,  1,  1,  2,  1,  2,  4,  2,  2,  4 },
    /*Fire    */ { 2,  2,  2,  2,  2,  1,  4,  2,  4,  1,  1,  4,  2,  2,  4,  1,  2,  2 },
    /*Water   */ { 2,  2,  2,  2,  4,  4,  2,  2,  2,  4,  1,  1,  2,  2,  2,  1,  2,  2 },
    /*Grass   */ { 2,  2,  1,  1,  4,  4,  1,  2,  1,  1,  4,  1,  2,  2,  2,  1,  2,  2 },
    /*Electric*/ { 2,  2,  4,  2,  0,  2,  2,  2,  2,  2,  4,  1,  1,  2,  2,  1,  2,  2 },
    /*Psychic */ { 2,  4,  2,  4,  2,  2,  2,  2,  1,  2,  2,  2,  2,  1,  2,  2,  0,  2 },
    /*Ice     */ { 2,  2,  4,  2,  4,  2,  2,  2,  1,  1,  1,  4,  2,  2,  1,  4,  2,  2 },
    /*Dragon  */ { 2,  2,  2,  2,  2,  2,  2,  2,  1,  2,  2,  2,  2,  2,  2,  4,  2,  0 },
    /*Dark    */ { 2,  1,  2,  2,  2,  2,  2,  4,  2,  2,  2,  2,  2,  4,  2,  2,  1,  1 },
    /*Fairy   */ { 2,  4,  2,  1,  2,  2,  2,  2,  1,  1,  2,  2,  2,  2,  2,  4,  4,  2 },
};

inline const char* GetTypeName(uint8_t type_id) {
    constexpr const char* kNames[kTypeCount] = {
        "Normal", "Luta", "Voador", "Veneno", "Terra", "Pedra",
        "Inseto", "Fantasma", "Aco", "Fogo", "Agua", "Planta",
        "Eletrico", "Psiquico", "Gelo", "Dragao", "Sombrio", "Fada"
    };
    if (type_id < kTypeCount) return kNames[type_id];
    return "Desconhecido";
}

inline float GetSingleTypeMultiplier(uint8_t attack_type, uint8_t defend_type) {
    if (attack_type >= kTypeCount || defend_type >= kTypeCount) return 1.0f;
    uint8_t val = kTypeChart[attack_type][defend_type];
    switch (val) {
        case 0: return 0.0f;
        case 1: return 0.5f;
        case 4: return 2.0f;
        case 2:
        default: return 1.0f;
    }
}

inline float GetTypeEffectiveness(uint8_t attack_type, uint8_t defend_type1, uint8_t defend_type2 = 255) {
    float mult = GetSingleTypeMultiplier(attack_type, defend_type1);
    if (defend_type2 < kTypeCount && defend_type2 != defend_type1) {
        mult *= GetSingleTypeMultiplier(attack_type, defend_type2);
    }
    return mult;
}

inline const char* GetEffectivenessText(float mult) {
    if (mult == 0.0f) return "Nao teve efeito!";
    if (mult >= 2.0f) return "Super efetivo!";
    if (mult <= 0.5f) return "Nao e muito efetivo...";
    return "Acertou o alvo!";
}

inline int32_t CalculateDamage(int level, int power, int attack_stat, int defense_stat,
                               float effectiveness, bool stab, float random_factor = 1.0f) {
    if (power == 0 || effectiveness == 0.0f) return 0;
    defense_stat = std::max(defense_stat, 1);
    attack_stat = std::max(attack_stat, 1);
    level = std::clamp(level, 1, 100);

    // Formula canônica: ((2 * Level / 5 + 2) * Power * Attack / Defense / 50 + 2) * Targets * Weather * Critical * Random * STAB * Type
    float base = (static_cast<float>(2 * level) / 5.0f + 2.0f);
    float damage = (base * power * static_cast<float>(attack_stat) / static_cast<float>(defense_stat)) / 50.0f + 2.0f;

    if (stab) {
        damage *= 1.5f;
    }
    damage *= effectiveness;
    damage *= std::clamp(random_factor, 0.85f, 1.00f);

    return static_cast<int32_t>(std::max(1.0f, std::round(damage)));
}

} // namespace za::game
