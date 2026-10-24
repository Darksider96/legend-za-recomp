#include "data/message_text.hpp"

#include <cstring>

namespace za::data {

namespace {

template <typename T>
bool ReadAt(std::span<const uint8_t> data, size_t offset, T& out) {
    if (offset > data.size() || sizeof(T) > data.size() - offset) return false;
    std::memcpy(&out, data.data() + offset, sizeof(T));
    return true;
}

void AppendUtf8(std::string& out, uint32_t code) {
    if (code < 0x80) {
        out.push_back(static_cast<char>(code));
    } else if (code < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (code >> 6)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else if (code < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (code >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (code >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
}

} // namespace

bool ParseMessageText(std::span<const uint8_t> data, std::vector<std::string>& out_lines, std::string& error) {
    out_lines.clear();
    uint16_t languages = 0, lines = 0;
    uint32_t section = 0, section_size = 0;
    if (!ReadAt(data, 0, languages) || !ReadAt(data, 2, lines) || !ReadAt(data, 12, section) || languages == 0 ||
        !ReadAt(data, section, section_size) || section_size > data.size() - section ||
        size_t{lines} * 8 + 4 > section_size) {
        error = "texto .dat com cabecalho invalido";
        return false;
    }
    const std::span<const uint8_t> body = data.subspan(section, section_size);
    out_lines.resize(lines);
    for (uint16_t i = 0; i < lines; ++i) {
        uint32_t offset = 0;
        uint16_t length = 0;
        ReadAt(body, 4 + size_t{i} * 8, offset);
        ReadAt(body, 8 + size_t{i} * 8, length);
        if (offset > body.size() || size_t{length} * 2 > body.size() - offset) {
            error = "linha " + std::to_string(i) + " fora da secao";
            return false;
        }
        uint16_t key = static_cast<uint16_t>(0x7C89 + 0x2983 * i);
        std::vector<uint16_t> units(length);
        for (uint16_t k = 0; k < length; ++k) {
            uint16_t value = 0;
            ReadAt(body, offset + size_t{k} * 2, value);
            units[k] = static_cast<uint16_t>(value ^ key);
            key = static_cast<uint16_t>((key << 3) | (key >> 13));
        }
        std::string& text = out_lines[i];
        for (size_t k = 0; k < units.size() && units[k] != 0; ++k) {
            uint32_t code = units[k];
            if (code >= 0xD800 && code < 0xDC00 && k + 1 < units.size() && units[k + 1] >= 0xDC00 && units[k + 1] < 0xE000) {
                code = 0x10000 + ((code - 0xD800) << 10) + (units[++k] - 0xDC00);
            }
            AppendUtf8(text, code);
        }
    }
    return true;
}

} // namespace za::data
