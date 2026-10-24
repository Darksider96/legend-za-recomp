#include "game/poke_actor.hpp"
#include "data/move_table.hpp"
#include "game/battle_calc.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace za::game {

void PokeActor::InitializeCombat(uint16_t sp_id, const std::string& pkm_name, uint8_t t1, uint8_t t2,
                                int lvl, int hp, int atk, int def, int spa, int spd, int spe) {
    species_id = sp_id;
    name = pkm_name;
    type_1 = t1;
    type_2 = t2;
    level = lvl;
    max_hp = hp;
    current_hp = hp;
    attack_stat = atk;
    defense_stat = def;
    sp_attack_stat = spa;
    sp_defense_stat = spd;
    speed_stat = spe;
    is_fainted = false;
    hit_flash_timer = 0.0f;
}

void PokeActor::TakeDamage(int damage) {
    if (is_fainted) return;
    current_hp -= damage;
    hit_flash_timer = 0.35f;
    if (current_hp <= 0) {
        current_hp = 0;
        is_fainted = true;
    }
}

void PokeActor::Heal(int amount) {
    if (is_fainted) return;
    current_hp = std::min(max_hp, current_hp + amount);
}

bool PokeActor::ConsumePP(uint8_t slot) {
    if (slot >= 4) return false;
    if (move_pp[slot] > 0) {
        move_pp[slot]--;
        return true;
    }
    return false;
}

void PokeActor::TriggerAttack(std::function<void(uint16_t, uint16_t)> on_attack) {
    if (state == ActorState::Attack || is_fainted) return;

    uint16_t mid = GetActiveMoveId();
    ConsumePP(selected_move_slot);

    state = ActorState::Attack;
    state_timer = 0.0f;
    anim_time = 0.0f;

    // Se houver alvo na arena, calcula o combate em tempo real
    if (target && !target->is_fainted) {
        float dist = (position - target->position).Length();
        if (dist <= 35.0f) {
            const auto* mentry = za::data::MoveDatabase::Instance().Get(mid);
            if (mentry && mentry->base_power > 0) {
                float mult = za::game::GetTypeEffectiveness(mentry->type, target->type_1, target->type_2);
                bool stab = (mentry->type == type_1 || mentry->type == type_2);

                int atk_val = (mentry->category == za::data::MoveCategory::Special) ? sp_attack_stat : attack_stat;
                int def_val = (mentry->category == za::data::MoveCategory::Special) ? target->sp_defense_stat : target->defense_stat;

                int dmg = za::game::CalculateDamage(level, mentry->base_power, atk_val, def_val, mult, stab, 0.95f);
                target->TakeDamage(dmg);
            } else if (mentry && mentry->base_power == 0) {
                // Golpe de recuperação (ex: Recover)
                Heal(max_hp / 2);
            }
        }
    }

    if (on_attack) {
        on_attack(species_id, mid);
    }
}

void PokeActor::Update(float dt, const nn::hid::NpadFullKeyState& pad, const gfx::Camera3D& cam,
                       const gfx::CollisionWorld& world, std::function<void(uint16_t, uint16_t)> on_attack) {
    if (dt <= 0.0f) dt = 0.0166f;
    if (dt > 0.1f) dt = 0.1f; // Clamp delta time para evitar teletransporte em quedas de framerate

    state_timer += dt;
    anim_time += dt;
    if (hit_flash_timer > 0.0f) {
        hit_flash_timer -= dt;
        if (hit_flash_timer < 0.0f) hit_flash_timer = 0.0f;
    }

    // Checagem de disparo de ataque: Botão Esquerdo do Mouse, Espaço, F, E ou botão ZR/A do gamepad
    auto is_key_down = [](int vk) -> bool {
        return (GetAsyncKeyState(vk) & 0x8000) != 0;
    };

    // Seleção de Golpes: Teclas Z (slot 0), X (slot 1), C (slot 2), V (slot 3) ou D-Pad
    if (is_key_down('Z') || (pad.buttons & nn::hid::NpadButton_Up) != 0) SelectMove(0);
    else if (is_key_down('X') || (pad.buttons & nn::hid::NpadButton_Right) != 0) SelectMove(1);
    else if (is_key_down('C') || (pad.buttons & nn::hid::NpadButton_Down) != 0) SelectMove(2);
    else if (is_key_down('V') || (pad.buttons & nn::hid::NpadButton_Left) != 0) SelectMove(3);

    bool attack_triggered = is_key_down(VK_LBUTTON) || is_key_down('F') || is_key_down(VK_SPACE) ||
                           ((pad.buttons & (nn::hid::NpadButton_ZR | nn::hid::NpadButton_A)) != 0);

    if (attack_triggered && state != ActorState::Attack) {
        TriggerAttack(on_attack);
    }

    if (state == ActorState::Attack) {
        float attack_duration = anims.attack ? anims.attack->DurationSeconds() : 1.8f;
        if (state_timer >= attack_duration) {
            state = ActorState::Idle;
            state_timer = 0.0f;
            anim_time = 0.0f;
        } else {
            // Em ataque, o personagem fica firme no chão com leve desaceleração de inércia
            velocity = velocity * 0.85f;
            position = position + velocity * dt;
        }
    } else {
        // Leitura dos eixos analógicos / teclas WASD
        float input_x = static_cast<float>(pad.analog_stick_l.x) / 32767.0f;
        float input_y = static_cast<float>(pad.analog_stick_l.y) / 32767.0f;
        float mag = std::sqrt(input_x * input_x + input_y * input_y);

        if (mag > 0.15f) {
            // Direção de movimento relativa à câmera de terceira pessoa
            float cam_yaw = cam.yaw;
            gfx::Vec3 cam_fwd{ std::sin(cam_yaw), 0.0f, std::cos(cam_yaw) };
            gfx::Vec3 cam_right{ std::cos(cam_yaw), 0.0f, -std::sin(cam_yaw) };

            gfx::Vec3 move_dir = cam_fwd * input_y + cam_right * input_x;
            float move_len = move_dir.Length();
            if (move_len > 1e-4f) {
                move_dir = move_dir * (1.0f / move_len);

                // Ângulo alvo de rotação (Yaw) para onde o Pokémon está se movendo
                target_yaw = std::atan2(move_dir.x, move_dir.z);

                // Interpolação angular suave pelo menor arco
                float diff = target_yaw - yaw;
                constexpr float kPi = 3.14159265f;
                constexpr float kTwoPi = 6.283185307f;
                while (diff > kPi)  diff -= kTwoPi;
                while (diff < -kPi) diff += kTwoPi;

                constexpr float kRotSpeed = 12.0f; // Radianos por segundo
                yaw += std::clamp(diff, -kRotSpeed * dt, kRotSpeed * dt);

                // Determinação da velocidade e estado: Corrida (Shift / analógico no máximo) ou Caminhada
                bool is_sprinting = is_key_down(VK_SHIFT) || (pad.buttons & nn::hid::NpadButton_ZL) != 0 || mag > 0.88f;
                float move_speed = is_sprinting ? 6.4f : 2.7f;
                state = is_sprinting ? ActorState::Run : ActorState::Walk;

                velocity = move_dir * move_speed;
                position = position + velocity * dt;
            } else {
                state = ActorState::Idle;
                velocity = gfx::Vec3{0, 0, 0};
            }
        } else {
            state = ActorState::Idle;
            velocity = gfx::Vec3{0, 0, 0};
        }
    }

    // Clamping de Terreno e Colisão de Solo (.trcol Havok)
    float ground_y = 0.0f;
    std::array<float, 3> normal{0.0f, 1.0f, 0.0f};
    if (world.GetGroundHeight(position.x, position.z, ground_y, normal)) {
        ground_height = ground_y;
        ground_normal = normal;
        // Suavização da altura do solo para passos macios
        position.y = position.y * 0.65f + ground_height * 0.35f;
        if (std::abs(position.y - ground_height) < 0.04f) {
            position.y = ground_height;
        }
    } else {
        ground_height = 0.0f;
        ground_normal = {0.0f, 1.0f, 0.0f};
        position.y = position.y * 0.70f;
        if (std::abs(position.y) < 0.01f) position.y = 0.0f;
    }

    // Avaliação da deformação esquelética da animação ativa
    EvaluatePose();
}

const gfx::AnimationClip* PokeActor::GetActiveAnimation() const {
    switch (state) {
        case ActorState::Attack:
            if (anims.attack) return anims.attack.get();
            break;
        case ActorState::Run:
            if (anims.run) return anims.run.get();
            if (anims.walk) return anims.walk.get();
            break;
        case ActorState::Walk:
            if (anims.walk) return anims.walk.get();
            break;
        case ActorState::Idle:
        default:
            if (anims.idle) return anims.idle.get();
            break;
    }
    return anims.idle ? anims.idle.get() : nullptr;
}

void PokeActor::EvaluatePose() {
    const gfx::AnimationClip* clip = GetActiveAnimation();
    if (clip && !model.skeleton.Empty()) {
        gfx::EvaluateAnimation(*clip, anim_time, model.skeleton);
    }
}

const char* PokeActor::GetStateName() const {
    switch (state) {
        case ActorState::Idle:   return "REPOUSO (IDLE)";
        case ActorState::Walk:   return "CAMINHANDO (WALK)";
        case ActorState::Run:    return "CORRENDO (RUN)";
        case ActorState::Attack: return "ATACANDO (ATTACK)";
        default:                 return "DESCONHECIDO";
    }
}

float PokeActor::GetCurrentSpeed() const {
    return velocity.Length();
}

} // namespace za::game
