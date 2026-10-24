#include "gfx/trscn.hpp"

#include "za/flat_reader.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string_view>

namespace za::gfx {

namespace {

using za::FlatReader;

constexpr int kMaxDepth = 64;

// Strings FlatBuffer (u32 tamanho + bytes + '\0') que parecem caminhos de arquivo: sem espacos e com extensao
void ScanAssetPaths(std::span<const uint8_t> data, std::vector<std::string>& out) {
    for (size_t pos = 0; pos + 8 <= data.size(); pos += 4) {
        uint32_t length = 0;
        std::memcpy(&length, data.data() + pos, 4);
        if (length < 4 || length > 512 || length > data.size() - pos - 5 || data[pos + 4 + length] != 0) continue;
        const std::string_view text(reinterpret_cast<const char*>(data.data() + pos + 4), length);
        const size_t dot = text.rfind('.');
        if (dot == std::string_view::npos || dot == 0 || text.size() - dot < 3 || text.size() - dot > 12) continue;
        const bool printable = std::all_of(text.begin(), text.end(), [](char c) { return c > ' ' && c < 0x7F; });
        const bool extension = std::all_of(text.begin() + static_cast<std::ptrdiff_t>(dot) + 1, text.end(), [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
        });
        if (!printable || !extension) continue;
        if (std::find(out.begin(), out.end(), text) == out.end()) out.emplace_back(text);
        pos += (length + 3) & ~size_t{3};
    }
}

// SceneObject { 0: nome, 1: SRT {0: escala, 1: rotacao, 2: translacao} }
bool ReadSceneObject(const FlatReader& fb, const FlatReader::Table& root, SceneEntity& entity) {
    entity.name = fb.StringField(root, 0);
    FlatReader::Table srt;
    if (!fb.TableField(root, 1, srt)) return false;
    entity.has_transform = true;
    fb.StructField(srt, 0, entity.transform.scale);
    fb.StructField(srt, 1, entity.transform.rotation);
    fb.StructField(srt, 2, entity.transform.translation);
    return true;
}

void DecodeEntityData(std::span<const uint8_t> nested, SceneEntity& entity) {
    ScanAssetPaths(nested, entity.assets);
    const FlatReader fb(nested);
    FlatReader::Table root;
    if (!fb.Root(root)) return;

    if (entity.type == "trinity_SceneObject") {
        ReadSceneObject(fb, root, entity);
    } else if (entity.type == "trinity_ObjectTemplate") {
        // { 0: nome, 1: template, 2: .trsot, 3: u8, 4: "trinity_SceneObject", 5: [u8] SceneObject aninhado com o SRT }
        FlatReader::Vector object;
        if (fb.VectorField(root, 5, 1, object)) {
            const FlatReader inner(std::span<const uint8_t>(fb.At(object.data), object.count));
            FlatReader::Table inner_root;
            if (inner.Root(inner_root)) ReadSceneObject(inner, inner_root, entity);
        }
        entity.name = fb.StringField(root, 0);
    } else if (entity.type == "trinity_CollisionComponent") {
        // union { 0: u8 tipo; 1: tabela } -> 1: forma { 0: .trcol (vazio = caixa), 1: translacao, 2: rotacao, 3: escala }
        // (ordem de 1/2 suposta: nenhum componente do RomFS conhecido tem valor nao nulo neles)
        FlatReader::Table holder, shape;
        if (fb.Scalar<uint8_t>(root, 0, 0) == 1 && fb.TableField(root, 1, holder) && fb.TableField(holder, 1, shape)) {
            entity.has_transform = fb.StructField(shape, 1, entity.transform.translation);
            fb.StructField(shape, 2, entity.transform.rotation);
            fb.StructField(shape, 3, entity.transform.scale);
        }
    } else if (entity.type == "trinity_ScenePoint") {
        entity.name = fb.StringField(root, 0);
        entity.has_transform = fb.StructField(root, 1, entity.transform.translation);
    } else if (entity.type == "trinity_ModelComponent") {
        entity.name = fb.StringField(root, 1);
    }
}

bool ParseEntity(const FlatReader& fb, const FlatReader::Table& table, int depth, SceneEntity& out, std::string& error) {
    if (depth > kMaxDepth) {
        error = "hierarquia de cena profunda demais";
        return false;
    }
    out.type = fb.StringField(table, 0);
    FlatReader::Vector nested;
    if (fb.VectorField(table, 1, 1, nested)) {
        out.data_size = nested.count;
        DecodeEntityData(std::span<const uint8_t>(fb.At(nested.data), nested.count), out);
    }
    FlatReader::Vector children;
    if (!fb.VectorField(table, 2, 4, children)) return true;
    out.children.resize(children.count);
    for (uint32_t i = 0; i < children.count; ++i) {
        FlatReader::Table child;
        if (!fb.TableElement(children, i, child) || !ParseEntity(fb, child, depth + 1, out.children[i], error)) {
            if (error.empty()) error = "entidade filha " + std::to_string(i) + " de " + out.type + " invalida";
            return false;
        }
    }
    return true;
}

} // namespace

bool ParseTrscn(std::span<const uint8_t> data, Scene& out, std::string& error) {
    out = {};
    const FlatReader fb(data);
    FlatReader::Table root;
    if (!fb.Root(root)) {
        error = "FlatBuffer de cena invalido";
        return false;
    }
    out.name = fb.StringField(root, 0);
    out.layer = fb.StringField(root, 1);

    FlatReader::Vector entities;
    if (fb.VectorField(root, 4, 4, entities)) {
        out.entities.resize(entities.count);
        for (uint32_t i = 0; i < entities.count; ++i) {
            FlatReader::Table entity;
            if (!fb.TableElement(entities, i, entity) || !ParseEntity(fb, entity, 0, out.entities[i], error)) {
                if (error.empty()) error = "entidade " + std::to_string(i) + " invalida";
                return false;
            }
        }
    }
    FlatReader::Vector sub_scenes;
    if (fb.VectorField(root, 5, 4, sub_scenes)) {
        out.sub_scenes.resize(sub_scenes.count);
        for (uint32_t i = 0; i < sub_scenes.count; ++i) {
            if (!fb.StringElement(sub_scenes, i, out.sub_scenes[i])) {
                error = "sub-cena " + std::to_string(i) + " invalida";
                return false;
            }
        }
    }
    return true;
}

bool ParseTrins(std::span<const uint8_t> data, InstanceBuffer& out, std::string& error) {
    out = {};
    const FlatReader fb(data);
    FlatReader::Table root, buffer;
    FlatReader::Vector bytes;
    if (!fb.Root(root) || !fb.TableField(root, 4, buffer) || !fb.VectorField(buffer, 0, 1, bytes)) {
        error = "FlatBuffer de instancias invalido";
        return false;
    }
    out.type = fb.StringField(root, 0);
    const uint32_t count = fb.Scalar<uint32_t>(root, 2, 0);
    if (bytes.count != uint64_t{count} * 64) {
        error = "buffer com " + std::to_string(bytes.count) + " bytes para " + std::to_string(count) + " matrizes 4x4";
        return false;
    }
    out.matrices.resize(count);
    for (uint32_t i = 0; i < count; ++i) fb.Read(bytes.data + size_t{i} * 64, out.matrices[i]);
    return true;
}

std::string SceneChunkPath(std::string_view scene_path, unsigned chunk) {
    for (const std::string_view extension : {std::string_view(".trscn"), std::string_view(".trsot")}) {
        if (!scene_path.ends_with(extension)) continue;
        std::string out(scene_path.substr(0, scene_path.size() - extension.size()));
        return out + "_" + std::to_string(chunk) + std::string(extension);
    }
    return std::string(scene_path);
}

std::array<float, 16> ComposeTransform(const SceneTransform& srt) {
    const auto& r = srt.rotation;
    const float cx = std::cos(r[0]), sx = std::sin(r[0]);
    const float cy = std::cos(r[1]), sy = std::sin(r[1]);
    const float cz = std::cos(r[2]), sz = std::sin(r[2]);
    // R = Rz * Ry * Rx (colunas)
    const float rot[3][3] = {
        {cz * cy, sz * cy, -sy},
        {cz * sy * sx - sz * cx, sz * sy * sx + cz * cx, cy * sx},
        {cz * sy * cx + sz * sx, sz * sy * cx - cz * sx, cy * cx},
    };
    std::array<float, 16> m{};
    for (int c = 0; c < 3; ++c) {
        for (int row = 0; row < 3; ++row) m[c * 4 + row] = rot[c][row] * srt.scale[c];
    }
    m[12] = srt.translation[0];
    m[13] = srt.translation[1];
    m[14] = srt.translation[2];
    m[15] = 1;
    return m;
}

std::array<float, 16> MultiplyTransforms(const std::array<float, 16>& a, const std::array<float, 16>& b) {
    std::array<float, 16> out{};
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0;
            for (int k = 0; k < 4; ++k) sum += a[k * 4 + row] * b[c * 4 + k];
            out[c * 4 + row] = sum;
        }
    }
    return out;
}

} // namespace za::gfx
