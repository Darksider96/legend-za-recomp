#pragma once
// Golpes aprendidos por nivel
//
// Fonte real: avalon/data/personal_array.bin (nao ha waza_tbl_array.bin no RomFS do Z-A). Campo 25 de cada
// registro Personal = [{ u16 golpe; u8 nivel; u8 nivel de dominio }] em ordem de aprendizado; ex.: Bulbasaur
// Tackle 1, Growl 1, Vine Whip 3, Growth 6, Leech Seed 9, Razor Leaf 12... So a forma 0 de cada especie e indexada.
// Load com buffer vazio usa a tabela canonica embutida (Pikachu, Mewtwo, Bulbasaur, Wartortle), que difere do
// jogo: no Z-A o Mewtwo aprende Recover no nivel 80 e nao tem Shadow Ball; o Wartortle nao tem Rapid Spin.

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace za::data {

struct LevelMove {
    uint8_t level{1};
    uint16_t move_id{0};
    uint8_t mastery_level{0};   // 0 na tabela canonica
};

class LearnsetDatabase {
public:
    static LearnsetDatabase& Instance();

    // Carrega do personal_array.bin (ou inicializa a tabela canonica se o buffer estiver vazio)
    bool Load(std::span<const uint8_t> bytes, std::string& error);

    // Retorna os golpes aprendidos por nivel para a especie (id interno), em ordem de aprendizado
    std::vector<LevelMove> GetLevelMoves(uint16_t species_id) const;

    // Retorna os 4 golpes mais recentes aprendidos ate determinado nivel (sem repeticao, do mais antigo ao mais novo)
    std::vector<uint16_t> GetMovesAtLevel(uint16_t species_id, uint8_t level) const;

    size_t SpeciesCount() const { return learnsets_.size(); }
    bool IsCanonicalFallback() const { return canonical_; }

private:
    std::unordered_map<uint16_t, std::vector<LevelMove>> learnsets_;
    bool canonical_{false};
};

} // namespace za::data
