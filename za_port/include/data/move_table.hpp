#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <span>

namespace za::data {

enum class MoveCategory : uint8_t {
    Status = 0,
    Physical = 1,
    Special = 2
};

struct MoveEntry {
    uint16_t move_id{0};
    std::string name;
    uint8_t type{0};            // 0..17 (Normal, Fighting, etc.)
    MoveCategory category{MoveCategory::Status};
    uint8_t base_power{0};
    uint8_t accuracy{0};
    uint8_t pp{0};
    int8_t priority{0};

    const char* CategoryName() const {
        switch (category) {
            case MoveCategory::Physical: return "Fisico";
            case MoveCategory::Special:  return "Especial";
            case MoveCategory::Status:
            default:                     return "Status";
        }
    }
};

class MoveDatabase {
public:
    static MoveDatabase& Instance();

    bool Load(std::span<const uint8_t> waza_array_bytes,
              std::span<const uint8_t> wazaname_dat_bytes,
              std::string& error);

    const MoveEntry* Get(uint16_t move_id) const;
    const MoveEntry* FindByName(const std::string& name) const;
    size_t Count() const { return moves_by_id_.size(); }

    const std::unordered_map<uint16_t, MoveEntry>& AllMoves() const { return moves_by_id_; }

private:
    MoveDatabase() = default;
    std::unordered_map<uint16_t, MoveEntry> moves_by_id_;
    std::unordered_map<std::string, uint16_t> moves_by_name_;
};

} // namespace za::data
