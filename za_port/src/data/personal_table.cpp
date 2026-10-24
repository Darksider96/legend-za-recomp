#include "data/personal_table.hpp"

#include "za/flat_reader.hpp"

#include <algorithm>
#include <array>

namespace za::data {

namespace {

using za::FlatReader;

constexpr std::array<std::string_view, 18> kTypeNames = {
    "Normal", "Luta", "Voador", "Veneno", "Terra", "Pedra", "Inseto", "Fantasma", "Aço",
    "Fogo", "Água", "Planta", "Elétrico", "Psíquico", "Gelo", "Dragão", "Noturno", "Fada",
};

struct SpeciesInfo {
    uint16_t species;
    uint16_t form;
    uint16_t national;
    uint8_t color;
    uint8_t unknown;
    uint16_t height_cm;
    uint16_t weight_hg;
};
static_assert(sizeof(SpeciesInfo) == 12);

bool ReadStats(const FlatReader& fb, const FlatReader::Table& table, unsigned field, PokemonStats& out) {
    std::array<uint8_t, 6> raw{};
    if (!fb.StructField(table, field, raw)) return false;
    out = {raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]};
    return true;
}

} // namespace

bool PersonalDatabase::Load(std::span<const uint8_t> data, std::string& error) {
    entries_.clear();
    national_index_.clear();
    const FlatReader fb(data);
    FlatReader::Table root;
    FlatReader::Vector records;
    if (!fb.Root(root) || !fb.VectorField(root, 0, 4, records)) {
        error = "personal_array: FlatBuffer sem vetor de registros";
        return false;
    }
    entries_.reserve(records.count);
    for (uint32_t i = 0; i < records.count; ++i) {
        FlatReader::Table record;
        SpeciesInfo info{};
        if (!fb.TableElement(records, i, record) || !fb.StructField(record, 0, info)) {
            error = "personal_array: registro " + std::to_string(i) + " invalido";
            return false;
        }
        PokemonPersonalData entry;
        entry.species_id = info.species;
        entry.form_id = info.form;
        entry.national_dex = info.national;
        entry.color = info.color;
        entry.height_cm = info.height_cm;
        entry.weight_hg = info.weight_hg;
        entry.type_1 = fb.Scalar<uint8_t>(record, 3, 0);
        entry.type_2 = fb.Scalar<uint8_t>(record, 4, 0);
        entry.growth_rate = fb.Scalar<uint8_t>(record, 8, 0);
        entry.catch_rate = fb.Scalar<uint8_t>(record, 9, 0);
        entry.egg_group_1 = fb.Scalar<uint8_t>(record, 11, 0);
        entry.egg_group_2 = fb.Scalar<uint8_t>(record, 12, 0);
        entry.hatch_cycles = fb.Scalar<uint8_t>(record, 14, 0);
        entry.base_friendship = fb.Scalar<uint8_t>(record, 15, 0);
        ReadStats(fb, record, 19, entry.ev_yield);
        ReadStats(fb, record, 20, entry.base_stats);

        const uint32_t key = MakeKey(entry.species_id, entry.form_id);
        if (!entries_.emplace(key, entry).second) {
            error = "personal_array: especie " + std::to_string(entry.species_id) + " forma " + std::to_string(entry.form_id) +
                    " duplicada";
            return false;
        }
        national_index_.emplace(MakeKey(entry.national_dex, entry.form_id), key);
    }
    return true;
}

const PokemonPersonalData* PersonalDatabase::Get(uint16_t species_id, uint16_t form_id) const {
    const auto it = entries_.find(MakeKey(species_id, form_id));
    return it == entries_.end() ? nullptr : &it->second;
}

const PokemonPersonalData* PersonalDatabase::FindByNationalDex(uint16_t national_dex, uint16_t form_id) const {
    const auto it = national_index_.find(MakeKey(national_dex, form_id));
    return it == national_index_.end() ? nullptr : &entries_.at(it->second);
}

std::vector<const PokemonPersonalData*> PersonalDatabase::Forms(uint16_t species_id) const {
    std::vector<const PokemonPersonalData*> forms;
    for (uint16_t form = 0; form < 10000; ++form) {
        const PokemonPersonalData* entry = Get(species_id, form);
        if (entry) {
            forms.push_back(entry);
        } else if (form > 64) {
            break;  // formas no jogo vao ate 27
        }
    }
    return forms;
}

std::string_view PersonalDatabase::GetTypeName(uint8_t type_id) const {
    return type_id < kTypeNames.size() ? kTypeNames[type_id] : std::string_view("?");
}

PersonalDatabase& PersonalDatabase::Instance() {
    static PersonalDatabase s_instance;
    return s_instance;
}

} // namespace za::data
