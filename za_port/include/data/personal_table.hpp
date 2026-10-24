#pragma once
// Dados pessoais das especies (avalon/data/personal_array.bin) — FlatBuffer { 0: [Personal] } com 1422 registros
//
// Personal (26 campos; offsets conferidos com Bulbasaur, Wartortle, Pikachu, Magikarp, Gyarados, Mewtwo):
//   0: struct { u16 especie (id interno); u16 forma; u16 n. nacional; u8 cor; u8 ?; u16 altura (cm); u16 peso (hg) }
//   3/4: u8 tipo 1/2     8: u8 ritmo de crescimento     9: u8 taxa de captura     11/12: u8 grupo de ovo 1/2
//   14: u8 ciclos de ovo  15: u8 amizade base           19: u8[6] EVs concedidos  20: u8[6] atributos base
//   21..25: vetores (movimentos, evolucoes...) nao lidos aqui
// O id interno coincide com o n. nacional ate 916; a partir dai segue a ordem de desenvolvimento do jogo
// (1000 = Tinkatink, nacional 957). Os nomes (monsname.dat) e os cries (PLAY_PV_%04hu) usam o id interno.
// Altura e peso: o campo e em centimetros (u16), nao em decimetros — Mewtwo = 200 cm, 1220 hg.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace za::data {

struct PokemonStats {
    uint8_t hp{0};
    uint8_t attack{0};
    uint8_t defense{0};
    uint8_t sp_attack{0};
    uint8_t sp_defense{0};
    uint8_t speed{0};

    uint32_t Total() const { return uint32_t{hp} + attack + defense + sp_attack + sp_defense + speed; }
};

struct PokemonPersonalData {
    uint16_t species_id{0};
    uint16_t form_id{0};
    uint16_t national_dex{0};
    uint8_t type_1{0};
    uint8_t type_2{0};
    PokemonStats base_stats;
    PokemonStats ev_yield;
    uint16_t height_cm{0};
    uint16_t weight_hg{0};
    uint8_t color{0};
    uint8_t catch_rate{0};
    uint8_t growth_rate{0};
    uint8_t egg_group_1{0};
    uint8_t egg_group_2{0};
    uint8_t hatch_cycles{0};
    uint8_t base_friendship{0};

    float HeightMeters() const { return static_cast<float>(height_cm) / 100.0f; }
    float WeightKg() const { return static_cast<float>(weight_hg) / 10.0f; }
    bool SingleType() const { return type_1 == type_2; }
};

class PersonalDatabase {
public:
    static PersonalDatabase& Instance();
    static constexpr uint32_t MakeKey(uint16_t species_id, uint16_t form_id) { return uint32_t{species_id} * 10000u + form_id; }

    bool Load(std::span<const uint8_t> data, std::string& error);

    const PokemonPersonalData* Get(uint16_t species_id, uint16_t form_id = 0) const;
    const PokemonPersonalData* FindByNationalDex(uint16_t national_dex, uint16_t form_id = 0) const;
    // Formas da especie em ordem crescente de form_id
    std::vector<const PokemonPersonalData*> Forms(uint16_t species_id) const;

    size_t Size() const { return entries_.size(); }
    const std::unordered_map<uint32_t, PokemonPersonalData>& Entries() const { return entries_; }

    std::string_view GetTypeName(uint8_t type_id) const;

private:
    std::unordered_map<uint32_t, PokemonPersonalData> entries_;
    std::unordered_map<uint32_t, uint32_t> national_index_;   // MakeKey(nacional, forma) -> chave interna
};

} // namespace za::data
