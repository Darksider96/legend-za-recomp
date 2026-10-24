#pragma once
// Leitor FlatBuffer minimo (somente leitura, sem flatc) com verificacao de limites em toda leitura

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>

namespace za {

static_assert(std::endian::native == std::endian::little, "FlatReader assume host little-endian");

class FlatReader {
public:
    struct Table {
        size_t pos{0};
        size_t vtable{0};
        uint16_t vtable_size{0};
    };

    struct Vector {
        size_t data{0};
        uint32_t count{0};
    };

    explicit FlatReader(std::span<const uint8_t> data) : data_(data) {}

    template <typename T>
    bool Read(size_t offset, T& out) const {
        if (offset > data_.size() || sizeof(T) > data_.size() - offset) return false;
        std::memcpy(&out, data_.data() + offset, sizeof(T));
        return true;
    }

    const uint8_t* At(size_t offset) const { return data_.data() + offset; }
    size_t Size() const { return data_.size(); }

    bool Root(Table& out) const {
        uint32_t root = 0;
        return Read(0, root) && TableAt(root, out);
    }

    bool TableAt(size_t pos, Table& out) const {
        int32_t soffset = 0;
        if (!Read(pos, soffset)) return false;
        const int64_t vtable = static_cast<int64_t>(pos) - soffset;
        if (vtable < 0) return false;
        uint16_t vtable_size = 0;
        if (!Read(static_cast<size_t>(vtable), vtable_size) || vtable_size < 4 || (vtable_size & 1)) return false;
        if (static_cast<size_t>(vtable) + vtable_size > data_.size()) return false;
        out = {pos, static_cast<size_t>(vtable), vtable_size};
        return true;
    }

    // Posicao absoluta do campo, ou 0 se ausente (campo nunca pode estar no offset 0)
    size_t Field(const Table& table, unsigned index) const {
        const size_t slot = 4 + 2 * static_cast<size_t>(index);
        if (slot + 2 > table.vtable_size) return 0;
        uint16_t offset = 0;
        Read(table.vtable + slot, offset);
        return offset ? table.pos + offset : 0;
    }

    template <typename T>
    T Scalar(const Table& table, unsigned index, T default_value) const {
        const size_t pos = Field(table, index);
        T value = default_value;
        if (pos == 0 || !Read(pos, value)) return default_value;
        return value;
    }

    // Struct gravado inline na tabela (ex.: vec3 de floats); false se ausente
    template <typename T>
    bool StructField(const Table& table, unsigned index, T& out) const {
        const size_t pos = Field(table, index);
        return pos != 0 && Read(pos, out);
    }

    bool TableField(const Table& table, unsigned index, Table& out) const {
        const size_t pos = Field(table, index);
        size_t target = 0;
        return pos != 0 && Deref(pos, target) && TableAt(target, out);
    }

    std::string StringField(const Table& table, unsigned index) const {
        const size_t pos = Field(table, index);
        size_t target = 0;
        uint32_t length = 0;
        if (pos == 0 || !Deref(pos, target) || !Read(target, length) || length > data_.size() - target - 4) return {};
        return std::string(reinterpret_cast<const char*>(At(target + 4)), length);
    }

    bool VectorField(const Table& table, unsigned index, size_t element_size, Vector& out) const {
        const size_t pos = Field(table, index);
        size_t vec = 0;
        uint32_t count = 0;
        if (pos == 0 || !Deref(pos, vec) || !Read(vec, count)) return false;
        const size_t data = vec + 4;
        if (data > data_.size() || static_cast<uint64_t>(count) * element_size > data_.size() - data) return false;
        out = {data, count};
        return true;
    }

    bool TableElement(const Vector& vec, uint32_t index, Table& out) const {
        size_t pos = 0;
        return Deref(vec.data + 4 * static_cast<size_t>(index), pos) && TableAt(pos, out);
    }

    bool StringElement(const Vector& vec, uint32_t index, std::string& out) const {
        size_t pos = 0;
        uint32_t length = 0;
        if (!Deref(vec.data + 4 * static_cast<size_t>(index), pos) || !Read(pos, length)) return false;
        if (length > data_.size() - pos - 4) return false;
        out.assign(reinterpret_cast<const char*>(At(pos + 4)), length);
        return true;
    }

    uint64_t U64Element(const Vector& vec, uint32_t index) const {
        uint64_t value = 0;
        Read(vec.data + 8 * static_cast<size_t>(index), value);
        return value;
    }

private:
    bool Deref(size_t at, size_t& out) const {
        uint32_t offset = 0;
        if (!Read(at, offset)) return false;
        out = at + offset;
        return out < data_.size();
    }

    std::span<const uint8_t> data_;
};

} // namespace za
