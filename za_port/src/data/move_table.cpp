#include "data/move_table.hpp"
#include "data/message_text.hpp"
#include "za/flat_reader.hpp"
#include <iostream>

namespace za::data {

MoveDatabase& MoveDatabase::Instance() {
    static MoveDatabase instance;
    return instance;
}

bool MoveDatabase::Load(std::span<const uint8_t> waza_array_bytes,
                        std::span<const uint8_t> wazaname_dat_bytes,
                        std::string& error) {
    moves_by_id_.clear();
    moves_by_name_.clear();

    // 1. Carrega os nomes dos golpes se wazaname_dat_bytes foi fornecido
    std::vector<std::string> move_names;
    if (!wazaname_dat_bytes.empty()) {
        std::string text_err;
        if (!ParseMessageText(wazaname_dat_bytes, move_names, text_err)) {
            // Se falhar o parse de texto, não aborta, continua com fallback numérico
            std::cout << "[MoveDatabase] Aviso ao decodificar wazaname.dat: " << text_err << std::endl;
        }
    }

    // 2. Parser do FlatBuffer waza_array.bin
    const FlatReader fb(waza_array_bytes);
    FlatReader::Table root;
    FlatReader::Vector records;

    if (!fb.Root(root) || !fb.VectorField(root, 0, 4, records)) {
        error = "waza_array: FlatBuffer sem vetor de movimentos";
        return false;
    }

    moves_by_id_.reserve(records.count);

    for (uint32_t i = 0; i < records.count; ++i) {
        FlatReader::Table move_table;
        if (!fb.TableElement(records, i, move_table)) {
            continue;
        }

        uint16_t move_id = fb.Scalar<uint16_t>(move_table, 0, static_cast<uint16_t>(i));
        if (move_id == 0 && i != 0) {
            move_id = static_cast<uint16_t>(i);
        }

        MoveEntry entry{};
        entry.move_id = move_id;
        entry.type = fb.Scalar<uint8_t>(move_table, 2, 0);
        entry.category = static_cast<MoveCategory>(fb.Scalar<uint8_t>(move_table, 4, 0));
        entry.base_power = fb.Scalar<uint8_t>(move_table, 5, 0);
        entry.accuracy = fb.Scalar<uint8_t>(move_table, 6, 100);
        entry.pp = fb.Scalar<uint8_t>(move_table, 7, 0);
        entry.priority = fb.Scalar<int8_t>(move_table, 8, 0);

        if (move_id < move_names.size() && !move_names[move_id].empty()) {
            entry.name = move_names[move_id];
        } else {
            entry.name = "Move #" + std::to_string(move_id);
        }

        moves_by_id_[move_id] = entry;
        moves_by_name_[entry.name] = move_id;
    }

    return !moves_by_id_.empty();
}

const MoveEntry* MoveDatabase::Get(uint16_t move_id) const {
    auto it = moves_by_id_.find(move_id);
    if (it != moves_by_id_.end()) {
        return &it->second;
    }
    return nullptr;
}

const MoveEntry* MoveDatabase::FindByName(const std::string& name) const {
    auto it = moves_by_name_.find(name);
    if (it != moves_by_name_.end()) {
        return Get(it->second);
    }
    return nullptr;
}

} // namespace za::data
