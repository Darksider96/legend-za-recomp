#include "gfx/trcol.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <string_view>

namespace za::gfx {

namespace {

using Vec3 = std::array<float, 3>;

template <typename T>
bool ReadAt(std::span<const uint8_t> data, size_t offset, T& out) {
    if (offset > data.size() || sizeof(T) > data.size() - offset) return false;
    std::memcpy(&out, data.data() + offset, sizeof(T));
    return true;
}

struct Section {
    std::string_view tag;
    size_t payload{0};
    size_t end{0};
};

// Secoes irmas em [begin, end): u32 big-endian (flags nos 2 bits altos) + tag
bool ReadSections(std::span<const uint8_t> data, size_t begin, size_t end, std::vector<Section>& out) {
    out.clear();
    for (size_t pos = begin; pos < end;) {
        if (pos + 8 > end) return false;
        const uint32_t header = (uint32_t{data[pos]} << 24) | (uint32_t{data[pos + 1]} << 16) |
                                (uint32_t{data[pos + 2]} << 8) | data[pos + 3];
        const size_t size = header & 0x3FFFFFFFu;
        if (size < 8 || size > end - pos) return false;
        out.push_back({std::string_view(reinterpret_cast<const char*>(data.data() + pos + 4), 4), pos + 8, pos + size});
        pos += size;
    }
    return true;
}

const Section* FindSection(const std::vector<Section>& sections, std::string_view tag) {
    for (const Section& section : sections) {
        if (section.tag == tag) return &section;
    }
    return nullptr;
}

// Inteiro comprimido do Havok: prefixo de bits altos define 1..5 bytes, big-endian
class VarintReader {
public:
    VarintReader(std::span<const uint8_t> data, size_t pos, size_t end) : data_(data), pos_(pos), end_(end) {}

    bool Next(uint64_t& out) {
        if (pos_ >= end_) return false;
        const uint8_t b = data_[pos_];
        size_t extra = 0;
        uint64_t value = 0;
        if (b < 0x80) {
            value = b;
        } else if ((b & 0xC0) == 0x80) {
            value = b & 0x3F, extra = 1;
        } else if ((b & 0xE0) == 0xC0) {
            value = b & 0x1F, extra = 2;
        } else if ((b & 0xF8) == 0xE0) {
            value = b & 0x07, extra = 3;
        } else if ((b & 0xF8) == 0xE8) {
            value = b & 0x07, extra = 4;
        } else {
            return false;
        }
        if (extra >= end_ - pos_) return false;
        for (size_t i = 1; i <= extra; ++i) value = (value << 8) | data_[pos_ + i];
        pos_ += 1 + extra;
        out = value;
        return true;
    }

    bool Skip(unsigned count) {
        uint64_t ignored = 0;
        for (unsigned i = 0; i < count; ++i) {
            if (!Next(ignored)) return false;
        }
        return true;
    }

    bool AtEnd() const { return pos_ >= end_; }

private:
    std::span<const uint8_t> data_;
    size_t pos_;
    size_t end_;
};

std::vector<std::string_view> SplitStrings(std::span<const uint8_t> data, const Section& section) {
    std::vector<std::string_view> out;
    size_t start = section.payload;
    for (size_t p = section.payload; p < section.end; ++p) {
        if (data[p] != 0) continue;
        out.emplace_back(reinterpret_cast<const char*>(data.data() + start), p - start);
        start = p + 1;
    }
    return out;
}

struct TagField {
    std::string_view name;
    uint32_t offset{0};
    uint32_t type{0};
};

struct TagType {
    std::string_view name;
    uint32_t parent{0};
    uint32_t size{0};
    std::vector<TagField> fields;
};

struct TagItem {
    uint32_t type{0};
    uint8_t flags{0};
    uint32_t offset{0};
    uint32_t count{0};
};

class Tagfile {
public:
    bool Parse(std::span<const uint8_t> data, std::string& error) {
        data_ = data;
        std::vector<Section> top, root, type_sections, index_sections;
        if (!ReadSections(data, 0, data.size(), top) || top.empty() || top.front().tag != "TAG0") {
            error = "nao e um tagfile Havok (TAG0)";
            return false;
        }
        if (!ReadSections(data, top.front().payload, top.front().end, root)) {
            error = "secoes do TAG0 corrompidas";
            return false;
        }
        const Section* sdk = FindSection(root, "SDKV");
        const Section* body = FindSection(root, "DATA");
        const Section* types = FindSection(root, "TYPE");
        const Section* index = FindSection(root, "INDX");
        if (!sdk || !body || !types || !index || !ReadSections(data, types->payload, types->end, type_sections) ||
            !ReadSections(data, index->payload, index->end, index_sections)) {
            error = "tagfile sem SDKV/DATA/TYPE/INDX";
            return false;
        }
        sdk_version_.assign(reinterpret_cast<const char*>(data.data() + sdk->payload), sdk->end - sdk->payload);
        body_ = data.subspan(body->payload, body->end - body->payload);

        const Section* tst = FindSection(type_sections, "TST1");
        const Section* tna = FindSection(type_sections, "TNA1");
        const Section* fst = FindSection(type_sections, "FST1");
        const Section* tbdy = FindSection(type_sections, "TBDY");
        const Section* item = FindSection(index_sections, "ITEM");
        if (!tst || !tna || !fst || !tbdy || !item) {
            error = "tagfile sem TST1/TNA1/FST1/TBDY/ITEM";
            return false;
        }
        return ParseTypes(*tst, *tna, *fst, *tbdy, error) && ParseItems(*item, error);
    }

    const std::string& SdkVersion() const { return sdk_version_; }
    const std::vector<TagItem>& Items() const { return items_; }

    std::optional<uint32_t> FindType(std::string_view name) const {
        for (uint32_t i = 1; i < types_.size(); ++i) {
            if (types_[i].name == name) return i;
        }
        return std::nullopt;
    }

    uint32_t TypeSize(uint32_t type) const { return type < types_.size() ? types_[type].size : 0; }

    // Offset do campo no tipo ou em um de seus pais
    std::optional<uint32_t> FieldOffset(uint32_t type, std::string_view field) const {
        for (unsigned depth = 0; type != 0 && type < types_.size() && depth < 32; ++depth) {
            for (const TagField& f : types_[type].fields) {
                if (f.name == field) return f.offset;
            }
            type = types_[type].parent;
        }
        return std::nullopt;
    }

    // Primeiro item (objeto apontado) do tipo dado
    const TagItem* FindObject(uint32_t type) const {
        for (const TagItem& entry : items_) {
            if (entry.type == type && entry.count > 0) return &entry;
        }
        return nullptr;
    }

    // Le o indice de ITEM gravado em body_offset e devolve o item, se valido e com elementos de element_size
    const TagItem* ItemRef(size_t body_offset, size_t element_size) const {
        uint32_t index = 0;
        if (!ReadAt(body_, body_offset, index) || index == 0 || index >= items_.size()) return nullptr;
        const TagItem& entry = items_[index];
        if (entry.offset > body_.size() || uint64_t{entry.count} * element_size > body_.size() - entry.offset) return nullptr;
        return &entry;
    }

    std::span<const uint8_t> Body() const { return body_; }

private:
    bool ParseTypes(const Section& tst, const Section& tna, const Section& fst, const Section& tbdy, std::string& error) {
        const std::vector<std::string_view> type_names = SplitStrings(data_, tst);
        const std::vector<std::string_view> field_names = SplitStrings(data_, fst);

        VarintReader names(data_, tna.payload, tna.end);
        uint64_t count = 0;
        if (!names.Next(count) || count == 0 || count > 1u << 20) {
            error = "TNA1 invalido";
            return false;
        }
        types_.assign(count, {});
        for (uint64_t i = 1; i < count; ++i) {
            uint64_t name = 0, template_count = 0;
            if (!names.Next(name) || !names.Next(template_count) || name >= type_names.size() ||
                !names.Skip(static_cast<unsigned>(2 * template_count))) {
                error = "TNA1 truncado";
                return false;
            }
            types_[i].name = type_names[name];
        }

        // TBDY: tipo, pai, opcoes (1 formato, 2 subtipo, 4 versao, 8 tamanho/alinhamento, 16 flags,
        // 32 campos, 64 interfaces, 128 atributo). A contagem de campos usa so os 16 bits baixos.
        VarintReader body(data_, tbdy.payload, tbdy.end);
        while (!body.AtEnd()) {
            uint64_t index = 0, parent = 0, options = 0;
            if (!body.Next(index) || index == 0) break;
            if (index >= types_.size() || !body.Next(parent) || !body.Next(options)) {
                error = "TBDY invalido";
                return false;
            }
            TagType& type = types_[index];
            type.parent = static_cast<uint32_t>(parent);
            bool ok = body.Skip(((options & 1) ? 1 : 0) + ((options & 2) ? 1 : 0) + ((options & 4) ? 1 : 0));
            uint64_t value = 0;
            if (ok && (options & 8)) {
                ok = body.Next(value) && body.Skip(1);
                type.size = static_cast<uint32_t>(value);
            }
            if (ok && (options & 16)) ok = body.Skip(1);
            if (ok && (options & 32)) {
                ok = body.Next(value);
                const uint64_t field_count = value & 0xFFFF;
                for (uint64_t f = 0; ok && f < field_count; ++f) {
                    uint64_t name = 0, offset = 0, field_type = 0;
                    ok = body.Next(name) && body.Skip(1) && body.Next(offset) && body.Next(field_type) &&
                         name < field_names.size();
                    if (ok) type.fields.push_back({field_names[name], static_cast<uint32_t>(offset), static_cast<uint32_t>(field_type)});
                }
            }
            if (ok && (options & 64)) ok = body.Next(value) && body.Skip(static_cast<unsigned>(2 * value));
            if (ok && (options & 128)) ok = body.Skip(1);
            if (!ok) {
                error = "TBDY truncado";
                return false;
            }
        }
        return true;
    }

    bool ParseItems(const Section& item, std::string& error) {
        for (size_t pos = item.payload; pos + 12 <= item.end; pos += 12) {
            uint32_t packed = 0;
            TagItem entry;
            ReadAt(data_, pos, packed);
            ReadAt(data_, pos + 4, entry.offset);
            ReadAt(data_, pos + 8, entry.count);
            entry.type = packed & 0xFFFFFF;
            entry.flags = static_cast<uint8_t>(packed >> 24);
            if (entry.type >= types_.size()) {
                error = "ITEM com tipo invalido";
                return false;
            }
            items_.push_back(entry);
        }
        return true;
    }

    std::span<const uint8_t> data_;
    std::span<const uint8_t> body_;
    std::string sdk_version_;
    std::vector<TagType> types_;
    std::vector<TagItem> items_;
};

Vec3 Sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }

} // namespace

bool ParseTrcol(std::span<const uint8_t> data, CollisionMesh& out, std::string& error) {
    out = {};
    Tagfile tag;
    if (!tag.Parse(data, error)) return false;

    const auto mesh_type = tag.FindType("hknpMeshShape");
    const auto util_type = tag.FindType("hknpMeshShapeVertexConversionUtil");
    const auto section_type = tag.FindType("hknpMeshShape::GeometrySection");
    const auto primitive_type = tag.FindType("hknpMeshShape::GeometrySection::Primitive");
    const TagItem* mesh = mesh_type ? tag.FindObject(*mesh_type) : nullptr;
    if (!mesh || !util_type) {
        error = "tagfile sem hknpMeshShape";
        return false;
    }
    const auto util_offset = tag.FieldOffset(*mesh_type, "vertexConversionUtil");
    const auto inv_offset = tag.FieldOffset(*util_type, "bitScale16Inv");
    const auto sections_offset = tag.FieldOffset(*mesh_type, "geometrySections");
    Vec3 inv_scale{};
    if (!util_offset || !inv_offset || !sections_offset || !ReadAt(tag.Body(), mesh->offset + *util_offset + *inv_offset, inv_scale)) {
        error = "hknpMeshShape sem vertexConversionUtil/geometrySections";
        return false;
    }

    // Materiais: hknpMaterialPalette.entries[].name (hkStringPtr -> item de chars)
    if (const auto palette_type = tag.FindType("hknpMaterialPalette"), descriptor_type = tag.FindType("hknpMaterialDescriptor");
        palette_type && descriptor_type) {
        const TagItem* palette = tag.FindObject(*palette_type);
        const auto entries_offset = tag.FieldOffset(*palette_type, "entries");
        const auto name_offset = tag.FieldOffset(*descriptor_type, "name");
        const uint32_t descriptor_size = tag.TypeSize(*descriptor_type);
        const TagItem* entries = palette && entries_offset ? tag.ItemRef(palette->offset + *entries_offset, descriptor_size) : nullptr;
        for (uint32_t i = 0; entries && name_offset && i < entries->count; ++i) {
            const TagItem* chars = tag.ItemRef(entries->offset + i * descriptor_size + *name_offset, 1);
            if (!chars) continue;
            const auto* text = reinterpret_cast<const char*>(tag.Body().data() + chars->offset);
            out.materials.emplace_back(text, strnlen(text, chars->count));
        }
    }

    const uint32_t section_size = section_type ? tag.TypeSize(*section_type) : 0;
    const TagItem* sections = section_size ? tag.ItemRef(mesh->offset + *sections_offset, section_size) : nullptr;
    if (!sections) return true;  // malha vazia (18 arquivos do RomFS)
    const auto primitives_offset = tag.FieldOffset(*section_type, "primitives");
    const auto vertices_offset = tag.FieldOffset(*section_type, "vertexBuffer");
    const auto origin_offset = tag.FieldOffset(*section_type, "sectionOffset");
    const uint32_t primitive_size = primitive_type ? tag.TypeSize(*primitive_type) : 0;
    if (!primitives_offset || !vertices_offset || !origin_offset || primitive_size < 4) {
        error = "GeometrySection com layout inesperado";
        return false;
    }

    const std::span<const uint8_t> body = tag.Body();
    out.section_count = sections->count;
    for (uint32_t s = 0; s < sections->count; ++s) {
        const size_t section = sections->offset + size_t{s} * section_size;
        std::array<int32_t, 3> origin{};
        const TagItem* primitives = tag.ItemRef(section + *primitives_offset, primitive_size);
        const TagItem* vertices = tag.ItemRef(section + *vertices_offset, 1);
        if (!ReadAt(body, section + *origin_offset, origin) || !primitives || !vertices) {
            error = "GeometrySection " + std::to_string(s) + " com referencias invalidas";
            return false;
        }
        const uint32_t base = static_cast<uint32_t>(out.vertices.size());
        // sectionOffset = INT32_MAX: secao sem quantizacao, vertexBuffer com float32 xyz (12 bytes por vertice)
        const bool unquantized = origin[0] == INT32_MAX && origin[1] == INT32_MAX && origin[2] == INT32_MAX;
        const uint32_t vertex_count = vertices->count / (unquantized ? 12 : 6);
        for (uint32_t v = 0; v < vertex_count; ++v) {
            if (unquantized) {
                Vec3 position{};
                ReadAt(body, vertices->offset + size_t{v} * 12, position);
                out.vertices.push_back(position);
                continue;
            }
            std::array<uint16_t, 3> q{};
            ReadAt(body, vertices->offset + size_t{v} * 6, q);
            out.vertices.push_back({(static_cast<float>(origin[0]) + q[0]) * inv_scale[0],
                                    (static_cast<float>(origin[1]) + q[1]) * inv_scale[1],
                                    (static_cast<float>(origin[2]) + q[2]) * inv_scale[2]});
        }
        out.unquantized_section_count += unquantized ? 1 : 0;
        for (uint32_t p = 0; p < primitives->count; ++p) {
            const uint8_t* id = body.data() + primitives->offset + size_t{p} * primitive_size;
            if (std::max({id[0], id[1], id[2], id[3]}) >= vertex_count) {
                error = "primitiva com indice de vertice fora da secao " + std::to_string(s);
                return false;
            }
            out.triangles.push_back({base + id[0], base + id[1], base + id[2]});
            if (id[2] != id[3]) {
                out.triangles.push_back({base + id[0], base + id[2], base + id[3]});
                ++out.quad_count;
            }
        }
    }

    if (!out.vertices.empty()) {
        out.aabb_min = out.aabb_max = out.vertices.front();
        for (const Vec3& v : out.vertices) {
            for (int a = 0; a < 3; ++a) {
                out.aabb_min[a] = std::min(out.aabb_min[a], v[a]);
                out.aabb_max[a] = std::max(out.aabb_max[a], v[a]);
            }
        }
    }
    BuildGroundGrid(out);
    return true;
}

void BuildGroundGrid(CollisionMesh& mesh) {
    mesh.grid_x = mesh.grid_z = 0;
    mesh.cell_start.clear();
    mesh.cell_triangles.clear();
    if (mesh.triangles.empty()) return;

    // ~4 triangulos por celula, ate 256 x 256, proporcional a extensao em X e Z
    const float extent_x = std::max(mesh.aabb_max[0] - mesh.aabb_min[0], 1e-3f);
    const float extent_z = std::max(mesh.aabb_max[2] - mesh.aabb_min[2], 1e-3f);
    const double cells = std::max(1.0, static_cast<double>(mesh.triangles.size()) / 4.0);
    const double aspect = extent_x / extent_z;
    mesh.grid_x = static_cast<uint32_t>(std::clamp(std::sqrt(cells * aspect), 1.0, 256.0));
    mesh.grid_z = static_cast<uint32_t>(std::clamp(std::sqrt(cells / aspect), 1.0, 256.0));
    mesh.cell_size_x = extent_x / static_cast<float>(mesh.grid_x);
    mesh.cell_size_z = extent_z / static_cast<float>(mesh.grid_z);

    const auto cell_range = [&](const std::array<uint32_t, 3>& t, uint32_t& x0, uint32_t& x1, uint32_t& z0, uint32_t& z1) {
        float min_x = mesh.vertices[t[0]][0], max_x = min_x, min_z = mesh.vertices[t[0]][2], max_z = min_z;
        for (int k = 1; k < 3; ++k) {
            min_x = std::min(min_x, mesh.vertices[t[k]][0]), max_x = std::max(max_x, mesh.vertices[t[k]][0]);
            min_z = std::min(min_z, mesh.vertices[t[k]][2]), max_z = std::max(max_z, mesh.vertices[t[k]][2]);
        }
        const auto cell = [](float value, float origin, float size, uint32_t count) {
            const float c = std::floor((value - origin) / size);
            return static_cast<uint32_t>(std::clamp(c, 0.0f, static_cast<float>(count - 1)));
        };
        x0 = cell(min_x, mesh.aabb_min[0], mesh.cell_size_x, mesh.grid_x);
        x1 = cell(max_x, mesh.aabb_min[0], mesh.cell_size_x, mesh.grid_x);
        z0 = cell(min_z, mesh.aabb_min[2], mesh.cell_size_z, mesh.grid_z);
        z1 = cell(max_z, mesh.aabb_min[2], mesh.cell_size_z, mesh.grid_z);
    };

    // Duas passadas (contagem + preenchimento) num CSR; paredes verticais (area XZ nula) ficam de fora
    std::vector<uint32_t> counts(size_t{mesh.grid_x} * mesh.grid_z + 1, 0);
    std::vector<uint8_t> skip(mesh.triangles.size(), 0);
    for (size_t i = 0; i < mesh.triangles.size(); ++i) {
        const auto& t = mesh.triangles[i];
        const Vec3 e1 = Sub(mesh.vertices[t[1]], mesh.vertices[t[0]]), e2 = Sub(mesh.vertices[t[2]], mesh.vertices[t[0]]);
        if (std::abs(e1[0] * e2[2] - e1[2] * e2[0]) < 1e-10f) {
            skip[i] = 1;
            continue;
        }
        uint32_t x0, x1, z0, z1;
        cell_range(t, x0, x1, z0, z1);
        for (uint32_t z = z0; z <= z1; ++z) {
            for (uint32_t x = x0; x <= x1; ++x) ++counts[size_t{z} * mesh.grid_x + x + 1];
        }
    }
    for (size_t c = 1; c < counts.size(); ++c) counts[c] += counts[c - 1];
    mesh.cell_start = counts;
    mesh.cell_triangles.resize(counts.back());
    for (size_t i = 0; i < mesh.triangles.size(); ++i) {
        if (skip[i]) continue;
        uint32_t x0, x1, z0, z1;
        cell_range(mesh.triangles[i], x0, x1, z0, z1);
        for (uint32_t z = z0; z <= z1; ++z) {
            for (uint32_t x = x0; x <= x1; ++x) mesh.cell_triangles[counts[size_t{z} * mesh.grid_x + x]++] = static_cast<uint32_t>(i);
        }
    }
}

bool RaycastGround(const CollisionMesh& col, float x, float z, float& out_ground_y, std::array<float, 3>& out_normal) {
    return RaycastGround(col, x, std::numeric_limits<float>::infinity(), z, out_ground_y, out_normal);
}

bool RaycastGround(const CollisionMesh& col, float x, float from_y, float z, float& out_ground_y,
                   std::array<float, 3>& out_normal) {
    constexpr float kEdge = 1e-6f;
    if (col.triangles.empty() || col.grid_x == 0 || x < col.aabb_min[0] - kEdge || x > col.aabb_max[0] + kEdge ||
        z < col.aabb_min[2] - kEdge || z > col.aabb_max[2] + kEdge) {
        return false;
    }
    const auto cell = [](float value, float origin, float size, uint32_t count) {
        return static_cast<uint32_t>(std::clamp(std::floor((value - origin) / size), 0.0f, static_cast<float>(count - 1)));
    };
    const size_t c = size_t{cell(z, col.aabb_min[2], col.cell_size_z, col.grid_z)} * col.grid_x +
                     cell(x, col.aabb_min[0], col.cell_size_x, col.grid_x);

    bool hit = false;
    float best = -std::numeric_limits<float>::infinity();
    const std::array<uint32_t, 3>* best_triangle = nullptr;
    for (uint32_t k = col.cell_start[c]; k < col.cell_start[c + 1]; ++k) {
        const auto& t = col.triangles[col.cell_triangles[k]];
        const Vec3& a = col.vertices[t[0]];
        const Vec3& b = col.vertices[t[1]];
        const Vec3& d = col.vertices[t[2]];
        // Baricentricas no plano XZ
        const float det = (b[0] - a[0]) * (d[2] - a[2]) - (d[0] - a[0]) * (b[2] - a[2]);
        if (std::abs(det) < 1e-12f) continue;
        const float u = ((x - a[0]) * (d[2] - a[2]) - (d[0] - a[0]) * (z - a[2])) / det;
        const float v = ((b[0] - a[0]) * (z - a[2]) - (x - a[0]) * (b[2] - a[2])) / det;
        if (u < -kEdge || v < -kEdge || u + v > 1.0f + kEdge) continue;
        const float y = a[1] + u * (b[1] - a[1]) + v * (d[1] - a[1]);
        if (y > from_y || y <= best) continue;
        best = y;
        best_triangle = &t;
        hit = true;
    }
    if (!hit) return false;

    const auto& t = *best_triangle;
    const Vec3 e1 = Sub(col.vertices[t[1]], col.vertices[t[0]]), e2 = Sub(col.vertices[t[2]], col.vertices[t[0]]);
    Vec3 n{e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
    const float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    const float sign = n[1] < 0 ? -1.0f : 1.0f;
    for (float& component : n) component = length > 0 ? sign * component / length : 0.0f;
    out_ground_y = best;
    out_normal = n;
    return true;
}

void TransformMesh(CollisionMesh& mesh, const std::array<float, 16>& m) {
    for (Vec3& v : mesh.vertices) {
        const Vec3 p = v;
        for (int r = 0; r < 3; ++r) v[r] = m[r] * p[0] + m[4 + r] * p[1] + m[8 + r] * p[2] + m[12 + r];
    }
    // Matriz com determinante negativo (espelhamento) inverte o winding; restaura o anti-horario
    const float det = m[0] * (m[5] * m[10] - m[9] * m[6]) - m[4] * (m[1] * m[10] - m[9] * m[2]) + m[8] * (m[1] * m[6] - m[5] * m[2]);
    if (det < 0) {
        for (auto& t : mesh.triangles) std::swap(t[1], t[2]);
    }
    if (!mesh.vertices.empty()) {
        mesh.aabb_min = mesh.aabb_max = mesh.vertices.front();
        for (const Vec3& v : mesh.vertices) {
            for (int a = 0; a < 3; ++a) {
                mesh.aabb_min[a] = std::min(mesh.aabb_min[a], v[a]);
                mesh.aabb_max[a] = std::max(mesh.aabb_max[a], v[a]);
            }
        }
    }
    BuildGroundGrid(mesh);
}

void CollisionWorld::Clear() {
    meshes_.clear();
    bounds_min_ = bounds_max_ = {0, 0, 0};
    has_bounds_ = false;
}

size_t CollisionWorld::Add(CollisionMesh mesh) {
    if (!mesh.Empty()) {
        for (int a = 0; a < 3; ++a) {
            bounds_min_[a] = has_bounds_ ? std::min(bounds_min_[a], mesh.aabb_min[a]) : mesh.aabb_min[a];
            bounds_max_[a] = has_bounds_ ? std::max(bounds_max_[a], mesh.aabb_max[a]) : mesh.aabb_max[a];
        }
        has_bounds_ = true;
    }
    meshes_.push_back(std::move(mesh));
    return meshes_.size() - 1;
}

bool CollisionWorld::GetGroundHeight(float x, float z, float& out_y, std::array<float, 3>& out_normal) const {
    return GetGroundHeight(x, std::numeric_limits<float>::infinity(), z, out_y, out_normal);
}

bool CollisionWorld::GetGroundHeight(float x, float from_y, float z, float& out_y, std::array<float, 3>& out_normal) const {
    bool hit = false;
    for (const CollisionMesh& mesh : meshes_) {
        float y = 0;
        Vec3 normal{};
        if (RaycastGround(mesh, x, from_y, z, y, normal) && (!hit || y > out_y)) {
            out_y = y;
            out_normal = normal;
            hit = true;
        }
    }
    return hit;
}

Vec3 CollisionWorld::ClampCameraAboveGround(const Vec3& target_camera_pos, float min_height_above_ground) const {
    const float radius = std::max(min_height_above_ground, 0.0f);
    const float from_y = target_camera_pos[1] + radius + kCameraProbeAbove;
    float required = -std::numeric_limits<float>::infinity();
    bool any = false;

    float ground = 0;
    Vec3 normal{};
    if (GetGroundHeight(target_camera_pos[0], from_y, target_camera_pos[2], ground, normal)) {
        // Folga perpendicular ao declive: num plano de normal n, altura vertical h / n.y; limitado a ~78 graus
        required = ground + radius / std::max(normal[1], 0.2f);
        any = true;
    }
    if (radius > 0) {
        constexpr float kOffsets[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& offset : kOffsets) {
            if (GetGroundHeight(target_camera_pos[0] + offset[0] * radius, from_y, target_camera_pos[2] + offset[1] * radius,
                                ground, normal)) {
                required = std::max(required, ground);  // equador da esfera acima do terreno vizinho
                any = true;
            }
        }
    }
    if (!any || target_camera_pos[1] >= required) return target_camera_pos;
    return {target_camera_pos[0], required, target_camera_pos[2]};
}

} // namespace za::gfx
