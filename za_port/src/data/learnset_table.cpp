#include "data/learnset_table.hpp"

#include "za/flat_reader.hpp"

#include <algorithm>
#include <array>

namespace za::data {

namespace {

using za::FlatReader;

constexpr unsigned kLevelMovesField = 25;

struct CanonicalEntry {
    uint16_t species;
    std::vector<LevelMove> moves;
};

std::vector<CanonicalEntry> CanonicalTable() {
    return {
        {25, {{1, 39}, {1, 84}, {4, 45}, {8, 98}, {12, 486}, {20, 209}, {28, 231}, {32, 85}}},
        {150, {{1, 93}, {1, 129}, {1, 50}, {1, 791}, {16, 246}, {32, 105}, {48, 247}, {56, 94}, {64, 540}}},
        {1, {{1, 33}, {1, 45}, {3, 22}, {9, 73}, {12, 75}}},
        {8, {{1, 33}, {1, 39}, {1, 55}, {9, 229}, {12, 44}, {24, 401}, {35, 56}}},
    };
}

} // namespace

LearnsetDatabase& LearnsetDatabase::Instance() {
    static LearnsetDatabase instance;
    return instance;
}

bool LearnsetDatabase::Load(std::span<const uint8_t> bytes, std::string& error) {
    learnsets_.clear();
    canonical_ = bytes.empty();
    if (canonical_) {
        for (CanonicalEntry& entry : CanonicalTable()) learnsets_[entry.species] = std::move(entry.moves);
        return true;
    }

    const FlatReader fb(bytes);
    FlatReader::Table root;
    FlatReader::Vector records;
    if (!fb.Root(root) || !fb.VectorField(root, 0, 4, records)) {
        error = "learnset: FlatBuffer sem vetor de registros Personal";
        return false;
    }
    for (uint32_t i = 0; i < records.count; ++i) {
        FlatReader::Table record;
        std::array<uint16_t, 2> key{};   // especie, forma (inicio do struct do campo 0)
        if (!fb.TableElement(records, i, record) || !fb.StructField(record, 0, key)) {
            error = "learnset: registro " + std::to_string(i) + " invalido";
            return false;
        }
        FlatReader::Vector moves;
        if (key[1] != 0 || !fb.VectorField(record, kLevelMovesField, 4, moves)) continue;
        std::vector<LevelMove>& list = learnsets_[key[0]];
        list.reserve(moves.count);
        for (uint32_t m = 0; m < moves.count; ++m) {
            uint16_t move = 0;
            std::array<uint8_t, 2> levels{};
            fb.Read(moves.data + 4 * size_t{m}, move);
            fb.Read(moves.data + 4 * size_t{m} + 2, levels);
            list.push_back({levels[0], move, levels[1]});
        }
    }
    return true;
}

std::vector<LevelMove> LearnsetDatabase::GetLevelMoves(uint16_t species_id) const {
    const auto it = learnsets_.find(species_id);
    return it == learnsets_.end() ? std::vector<LevelMove>{} : it->second;
}

std::vector<uint16_t> LearnsetDatabase::GetMovesAtLevel(uint16_t species_id, uint8_t level) const {
    std::vector<uint16_t> moves;
    const auto it = learnsets_.find(species_id);
    if (it == learnsets_.end()) return moves;
    // Do mais recente para o mais antigo, ignorando golpes repetidos, ate juntar 4
    for (auto entry = it->second.rbegin(); entry != it->second.rend() && moves.size() < 4; ++entry) {
        if (entry->level <= level && std::find(moves.begin(), moves.end(), entry->move_id) == moves.end()) {
            moves.push_back(entry->move_id);
        }
    }
    std::reverse(moves.begin(), moves.end());
    return moves;
}

} // namespace za::data
