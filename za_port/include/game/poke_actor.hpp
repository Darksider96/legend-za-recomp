#pragma once
#include "gfx/camera3d.hpp"
#include "gfx/trmdl.hpp"
#include "gfx/tranm.hpp"
#include "gfx/trcol.hpp"
#include "nn/hid.hpp"
#include <cstdint>
#include <string>
#include <memory>
#include <functional>
#include <array>

namespace za::game {

enum class ActorState : uint8_t {
    Idle = 0,
    Walk = 1,
    Run = 2,
    Attack = 3
};

struct ActorAnimationSet {
    std::shared_ptr<gfx::AnimationClip> idle;
    std::shared_ptr<gfx::AnimationClip> walk;
    std::shared_ptr<gfx::AnimationClip> run;
    std::shared_ptr<gfx::AnimationClip> attack;
};

class PokeActor {
public:
    uint16_t species_id{150};
    std::string name{"Mewtwo"};
    gfx::Model model;
    ActorAnimationSet anims;

    gfx::Vec3 position{0.0f, 0.0f, 0.0f};
    gfx::Vec3 velocity{0.0f, 0.0f, 0.0f};
    float yaw{0.0f};          // Rotação atual do modelo no eixo Y (radianos)
    float target_yaw{0.0f};   // Rotação desejada
    float scale{1.0f};
    float height{2.0f};       // Altura canônica da Pokédex em metros

    ActorState state{ActorState::Idle};
    float state_timer{0.0f};
    float anim_time{0.0f};

    float ground_height{0.0f};
    std::array<float, 3> ground_normal{0.0f, 1.0f, 0.0f};

    // Conjunto dos 4 golpes ativos
    std::array<uint16_t, 4> moves{0, 0, 0, 0};
    std::array<uint8_t, 4> move_pp{15, 10, 30, 10};
    std::array<uint8_t, 4> move_max_pp{15, 10, 30, 10};
    uint8_t selected_move_slot{0};

    // Atributos de Batalha e HP
    int level{50};
    int max_hp{180};
    int current_hp{180};
    int attack_stat{150};
    int defense_stat{100};
    int sp_attack_stat{180};
    int sp_defense_stat{100};
    int speed_stat{130};
    uint8_t type_1{13};
    uint8_t type_2{13};

    float hit_flash_timer{0.0f};
    bool is_fainted{false};
    PokeActor* target{nullptr};

    void SelectMove(uint8_t slot) { if (slot < 4) selected_move_slot = slot; }
    uint16_t GetActiveMoveId() const { return moves[selected_move_slot]; }
    uint8_t GetActiveMovePP() const { return move_pp[selected_move_slot]; }
    uint8_t GetActiveMoveMaxPP() const { return move_max_pp[selected_move_slot]; }

    void SetTarget(PokeActor* tgt) { target = tgt; }
    PokeActor* GetTarget() const { return target; }

    void InitializeCombat(uint16_t sp_id, const std::string& pkm_name, uint8_t t1, uint8_t t2,
                          int lvl, int hp, int atk, int def, int spa, int spd, int spe);
    void TakeDamage(int damage);
    void Heal(int amount);
    bool ConsumePP(uint8_t slot);
    float GetHpPercent() const { return max_hp > 0 ? static_cast<float>(current_hp) / static_cast<float>(max_hp) : 0.0f; }

    PokeActor() = default;

    void TriggerAttack(std::function<void(uint16_t, uint16_t)> on_attack = nullptr);
    void TriggerAttack(std::function<void(uint16_t)> on_attack) {
        TriggerAttack([on_attack](uint16_t sp, uint16_t) {
            if (on_attack) on_attack(sp);
        });
    }
    void Update(float dt, const nn::hid::NpadFullKeyState& pad, const gfx::Camera3D& cam, const gfx::CollisionWorld& world,
                std::function<void(uint16_t, uint16_t)> on_attack = nullptr);
    void Update(float dt, const nn::hid::NpadFullKeyState& pad, const gfx::Camera3D& cam, const gfx::CollisionWorld& world,
                std::function<void(uint16_t)> on_attack) {
        Update(dt, pad, cam, world, [on_attack](uint16_t sp, uint16_t) {
            if (on_attack) on_attack(sp);
        });
    }
    void EvaluatePose();

    const gfx::AnimationClip* GetActiveAnimation() const;
    const char* GetStateName() const;
    float GetCurrentSpeed() const;
};

} // namespace za::game
