#include "gfx/trmdl.hpp"

#include "gfx/tranm.hpp"
#include "za/flat_reader.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace za::gfx {

namespace {

namespace fs_std = std::filesystem;
using za::FlatReader;

enum class ComponentKind : uint8_t { Float32, Float16, Unorm16, Unorm8, Uint8, Uint32 };

struct VertexFormatInfo {
    uint32_t id;
    uint8_t components;
    uint8_t size;
    ComponentKind kind;
};

constexpr VertexFormatInfo kVertexFormats[] = {
    {20, 4, 4, ComponentKind::Unorm8},   // RGBA8_UNORM
    {22, 4, 4, ComponentKind::Uint8},    // RGBA8_UINT
    {36, 1, 4, ComponentKind::Uint32},   // R32_UINT
    {39, 4, 8, ComponentKind::Unorm16},  // RGBA16_UNORM
    {43, 4, 8, ComponentKind::Float16},  // RGBA16_FLOAT
    {48, 2, 8, ComponentKind::Float32},  // RG32_FLOAT
    {51, 3, 12, ComponentKind::Float32}, // RGB32_FLOAT
    {54, 4, 16, ComponentKind::Float32}, // RGBA32_FLOAT
};

const VertexFormatInfo* FindVertexFormat(uint32_t id) {
    for (const VertexFormatInfo& format : kVertexFormats) {
        if (format.id == id) return &format;
    }
    return nullptr;
}

float HalfToFloat(uint16_t half) {
    const uint32_t sign = static_cast<uint32_t>(half & 0x8000u) << 16;
    uint32_t exponent = (half >> 10) & 0x1Fu;
    uint32_t mantissa = half & 0x3FFu;
    if (exponent == 0) {
        if (mantissa == 0) return std::bit_cast<float>(sign);
        exponent = 127 - 15 + 1;  // subnormal: normaliza a mantissa
        while ((mantissa & 0x400u) == 0) {
            mantissa <<= 1;
            --exponent;
        }
        return std::bit_cast<float>(sign | (exponent << 23) | ((mantissa & 0x3FFu) << 13));
    }
    if (exponent == 31) return std::bit_cast<float>(sign | 0x7F800000u | (mantissa << 13));
    return std::bit_cast<float>(sign | ((exponent + 127 - 15) << 23) | (mantissa << 13));
}

// Ate 4 componentes como float; inteiros sem normalizacao. Componentes ausentes: (0, 0, 0, 1)
void DecodeComponents(const uint8_t* src, const VertexFormatInfo& format, float out[4]) {
    out[0] = out[1] = out[2] = 0.0f;
    out[3] = 1.0f;
    for (uint8_t c = 0; c < format.components; ++c) {
        switch (format.kind) {
        case ComponentKind::Float32: std::memcpy(&out[c], src + 4 * c, 4); break;
        case ComponentKind::Float16: {
            uint16_t value = 0;
            std::memcpy(&value, src + 2 * c, 2);
            out[c] = HalfToFloat(value);
            break;
        }
        case ComponentKind::Unorm16: {
            uint16_t value = 0;
            std::memcpy(&value, src + 2 * c, 2);
            out[c] = static_cast<float>(value) / 65535.0f;
            break;
        }
        case ComponentKind::Unorm8: out[c] = static_cast<float>(src[c]) / 255.0f; break;
        case ComponentKind::Uint8:  out[c] = static_cast<float>(src[c]); break;
        case ComponentKind::Uint32: {
            uint32_t value = 0;
            std::memcpy(&value, src + 4 * c, 4);
            out[c] = static_cast<float>(value);
            break;
        }
        }
    }
}

bool BytesOf(const FlatReader& fb, const FlatReader::Vector& list, uint32_t index, std::span<const uint8_t>& out) {
    FlatReader::Table holder;
    FlatReader::Vector bytes;
    if (index >= list.count || !fb.TableElement(list, index, holder) || !fb.VectorField(holder, 0, 1, bytes)) return false;
    out = std::span<const uint8_t>(fb.At(bytes.data), bytes.count);
    return true;
}

// ---- glTF ----

std::string JsonEscape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
            out.push_back(c);
        } else if (static_cast<unsigned char>(c) < 0x20) {
            char buffer[8];
            std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned>(c));
            out += buffer;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string Num(float value) {
    if (!std::isfinite(value)) value = 0.0f;
    char buffer[32];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, result.ptr);
}

std::string Num(size_t value) { return std::to_string(value); }

template <typename Container>
std::string JoinJson(const Container& items) {
    std::string out = "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) out += ",";
        out += items[i];
    }
    return out + "]";
}

std::string Vec(std::span<const float> values) {
    std::string out = "[";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i) out += ",";
        out += Num(values[i]);
    }
    return out + "]";
}

constexpr int kFloat = 5126, kUnsignedShort = 5123, kUnsignedInt = 5125;
constexpr int kArrayBuffer = 34962, kElementArrayBuffer = 34963;

class GltfBuffers {
public:
    size_t AddView(const void* data, size_t size, int target) {
        while (bytes_.size() % 4) bytes_.push_back(0);
        const size_t offset = bytes_.size();
        const auto* begin = static_cast<const uint8_t*>(data);
        bytes_.insert(bytes_.end(), begin, begin + size);
        std::string view = "{\"buffer\":0,\"byteOffset\":" + Num(offset) + ",\"byteLength\":" + Num(size);
        if (target) view += ",\"target\":" + std::to_string(target);
        views_.push_back(view + "}");
        return views_.size() - 1;
    }

    size_t AddAccessor(size_t view, size_t byte_offset, int component_type, size_t count, const char* type,
                       const std::string& extra = {}) {
        accessors_.push_back("{\"bufferView\":" + Num(view) + ",\"byteOffset\":" + Num(byte_offset) +
                             ",\"componentType\":" + std::to_string(component_type) + ",\"count\":" + Num(count) +
                             ",\"type\":\"" + type + "\"" + extra + "}");
        return accessors_.size() - 1;
    }

    template <typename T>
    size_t AddArray(const std::vector<T>& values, int component_type, size_t count, const char* type,
                    int target = kArrayBuffer, const std::string& extra = {}) {
        const size_t view = AddView(values.data(), values.size() * sizeof(T), target);
        return AddAccessor(view, 0, component_type, count, type, extra);
    }

    const std::vector<uint8_t>& Bytes() const { return bytes_; }
    const std::vector<std::string>& Views() const { return views_; }
    const std::vector<std::string>& Accessors() const { return accessors_; }

private:
    std::vector<uint8_t> bytes_;
    std::vector<std::string> views_;
    std::vector<std::string> accessors_;
};

using QuatD = std::array<double, 4>;  // x, y, z, w
using Matrix = std::array<float, 16>;  // coluna-major

QuatD Multiply(const QuatD& a, const QuatD& b) {
    return {a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
            a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
            a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
            a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
}

Matrix Multiply(const Matrix& a, const Matrix& b) {
    Matrix out{};
    for (size_t column = 0; column < 4; ++column) {
        for (size_t row = 0; row < 4; ++row) {
            float sum = 0;
            for (size_t k = 0; k < 4; ++k) sum += a[k * 4 + row] * b[column * 4 + k];
            out[column * 4 + row] = sum;
        }
    }
    return out;
}

// T * R * S do no (pose atual)
Matrix LocalMatrix(const SkeletonNode& node) {
    const float x = node.orientation[0], y = node.orientation[1], z = node.orientation[2], w = node.orientation[3];
    const auto& s = node.scale;
    const auto& t = node.translation;
    return {(1 - 2 * (y * y + z * z)) * s[0], 2 * (x * y + z * w) * s[0], 2 * (x * z - y * w) * s[0], 0,
            2 * (x * y - z * w) * s[1], (1 - 2 * (x * x + z * z)) * s[1], 2 * (y * z + x * w) * s[1], 0,
            2 * (x * z + y * w) * s[2], 2 * (y * z - x * w) * s[2], (1 - 2 * (x * x + y * y)) * s[2], 0,
            t[0], t[1], t[2], 1};
}

bool IsPng(std::string_view uri) {
    return uri.size() >= 4 && uri.substr(uri.size() - 4) == ".png";
}

bool WriteFile(const fs_std::path& path, const void* data, size_t size, std::string& error) {
    std::error_code ec;
    if (path.has_parent_path()) fs_std::create_directories(path.parent_path(), ec);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream || !stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(size))) {
        error = "falha ao gravar " + path.string();
        return false;
    }
    return true;
}

// Junta de ossos validas: todo osso tem um no e todo indice de vertice cabe na lista
bool BuildJointList(const Model& model, std::vector<size_t>& joints) {
    const Skeleton& skeleton = model.skeleton;
    if (skeleton.Empty()) return false;
    joints.assign(skeleton.inverse_bind.size(), SIZE_MAX);
    for (size_t i = 0; i < skeleton.nodes.size(); ++i) {
        const int32_t bone = skeleton.nodes[i].bone;
        if (bone >= 0 && static_cast<size_t>(bone) < joints.size()) joints[static_cast<size_t>(bone)] = i;
    }
    if (std::find(joints.begin(), joints.end(), SIZE_MAX) != joints.end()) return false;
    for (const MeshShape& mesh : model.meshes) {
        for (uint16_t joint : mesh.joints) {
            if (joint >= joints.size()) return false;
        }
    }
    return true;
}

} // namespace

// Rotacao Euler XYZ extrinseca (R = Rz * Ry * Rx), conferida contra as inverse bind matrices do .trskl
std::array<float, 4> EulerXyzToQuaternion(const std::array<float, 3>& euler) {
    const auto axis = [](int index, float angle) {
        QuatD q{0, 0, 0, std::cos(angle * 0.5)};
        q[static_cast<size_t>(index)] = std::sin(angle * 0.5);
        return q;
    };
    const QuatD q = Multiply(axis(2, euler[2]), Multiply(axis(1, euler[1]), axis(0, euler[0])));
    return {static_cast<float>(q[0]), static_cast<float>(q[1]), static_cast<float>(q[2]), static_cast<float>(q[3])};
}

void UpdateSkeletonMatrices(Skeleton& skeleton) {
    const size_t count = skeleton.nodes.size();
    skeleton.world.assign(count, Matrix{});
    std::vector<uint8_t> done(count, 0);
    std::vector<size_t> chain;
    for (size_t i = 0; i < count; ++i) {
        // Sobe ate um ancestral ja calculado e desce aplicando as transformacoes (pais podem vir depois dos filhos)
        chain.clear();
        for (size_t node = i; !done[node];) {
            if (std::find(chain.begin(), chain.end(), node) != chain.end()) break;  // ciclo: trata como raiz
            chain.push_back(node);
            const int32_t parent = skeleton.nodes[node].parent;
            if (parent < 0 || static_cast<size_t>(parent) >= count) break;
            node = static_cast<size_t>(parent);
        }
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            const int32_t parent = skeleton.nodes[*it].parent;
            const Matrix local = LocalMatrix(skeleton.nodes[*it]);
            const bool has_parent = parent >= 0 && static_cast<size_t>(parent) < count && done[static_cast<size_t>(parent)];
            skeleton.world[*it] = has_parent ? Multiply(skeleton.world[static_cast<size_t>(parent)], local) : local;
            done[*it] = 1;
        }
    }

    skeleton.skinning.assign(skeleton.inverse_bind.size(), Matrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
    for (size_t i = 0; i < count; ++i) {
        const int32_t bone = skeleton.nodes[i].bone;
        if (bone >= 0 && static_cast<size_t>(bone) < skeleton.inverse_bind.size()) {
            skeleton.skinning[static_cast<size_t>(bone)] = Multiply(skeleton.world[i], skeleton.inverse_bind[static_cast<size_t>(bone)]);
        }
    }
}

const MaterialTexture* Material::Find(std::string_view slot) const {
    for (const MaterialTexture& texture : textures) {
        if (texture.slot == slot) return &texture;
    }
    return nullptr;
}

bool ParseTrmdl(std::span<const uint8_t> data, TrmdlFile& out, std::string& error) {
    out = {};
    const FlatReader fb(data);
    FlatReader::Table root;
    if (!fb.Root(root)) {
        error = "trmdl: FlatBuffer invalido";
        return false;
    }

    FlatReader::Vector meshes, materials;
    if (fb.VectorField(root, 1, 4, meshes)) {
        for (uint32_t i = 0; i < meshes.count; ++i) {
            FlatReader::Table mesh;
            if (fb.TableElement(meshes, i, mesh)) out.mesh_files.push_back(fb.StringField(mesh, 0));
        }
    }
    FlatReader::Table skeleton;
    if (fb.TableField(root, 2, skeleton)) out.skeleton_file = fb.StringField(skeleton, 0);
    if (fb.VectorField(root, 3, 4, materials)) {
        for (uint32_t i = 0; i < materials.count; ++i) {
            std::string name;
            if (fb.StringElement(materials, i, name)) out.material_files.push_back(std::move(name));
        }
    }
    FlatReader::Table bounds;
    if (fb.TableField(root, 5, bounds)) {
        fb.StructField(bounds, 0, out.bounds_min);
        fb.StructField(bounds, 1, out.bounds_max);
    }

    // Modelos-base de personagem trazem so o esqueleto (sem malhas)
    if (out.mesh_files.empty() && out.skeleton_file.empty()) {
        error = "trmdl sem malhas nem esqueleto";
        return false;
    }
    return true;
}

bool ParseTrmsh(std::span<const uint8_t> data, std::vector<MeshShape>& out, std::string& buffer_file, std::string& error) {
    out.clear();
    const FlatReader fb(data);
    FlatReader::Table root;
    FlatReader::Vector shapes;
    if (!fb.Root(root) || !fb.VectorField(root, 1, 4, shapes)) {
        error = "trmsh: FlatBuffer invalido";
        return false;
    }
    buffer_file = fb.StringField(root, 2);

    for (uint32_t i = 0; i < shapes.count; ++i) {
        FlatReader::Table shape;
        if (!fb.TableElement(shapes, i, shape)) {
            error = "trmsh: MeshShape #" + std::to_string(i) + " invalido";
            return false;
        }
        MeshShape mesh;
        mesh.name = fb.StringField(shape, 0);
        mesh.index_format = fb.Scalar<uint32_t>(shape, 2, 0);

        FlatReader::Vector accessors, submeshes;
        if (fb.VectorField(shape, 3, 4, accessors)) {
            for (uint32_t s = 0; s < accessors.count; ++s) {
                FlatReader::Table accessor;
                FlatReader::Vector attributes, sizes;
                if (!fb.TableElement(accessors, s, accessor)) continue;
                if (fb.VectorField(accessor, 0, 4, attributes)) {
                    for (uint32_t a = 0; a < attributes.count; ++a) {
                        FlatReader::Table attribute;
                        if (!fb.TableElement(attributes, a, attribute)) continue;
                        mesh.attributes.push_back({fb.Scalar<uint32_t>(attribute, 1, 0), fb.Scalar<uint32_t>(attribute, 2, 0),
                                                   fb.Scalar<uint32_t>(attribute, 3, 0), fb.Scalar<uint32_t>(attribute, 4, 0), s});
                    }
                }
                uint32_t stride = 0;
                FlatReader::Table size;
                if (fb.VectorField(accessor, 1, 4, sizes) && sizes.count > 0 && fb.TableElement(sizes, 0, size)) {
                    stride = fb.Scalar<uint32_t>(size, 0, 0);
                }
                mesh.strides.push_back(stride);
            }
        }
        if (fb.VectorField(shape, 4, 4, submeshes)) {
            for (uint32_t m = 0; m < submeshes.count; ++m) {
                FlatReader::Table submesh;
                if (!fb.TableElement(submeshes, m, submesh)) continue;
                mesh.submeshes.push_back({fb.Scalar<uint32_t>(submesh, 0, 0), fb.Scalar<uint32_t>(submesh, 1, 0),
                                          fb.StringField(submesh, 3)});
            }
        }
        out.push_back(std::move(mesh));
    }
    return true;
}

bool LoadTrmbf(std::span<const uint8_t> data, std::vector<MeshShape>& meshes, std::string& error) {
    const FlatReader fb(data);
    FlatReader::Table root;
    FlatReader::Vector buffers;
    if (!fb.Root(root) || !fb.VectorField(root, 1, 4, buffers)) {
        error = "trmbf: FlatBuffer invalido";
        return false;
    }
    if (buffers.count < meshes.size()) {
        error = "trmbf com " + std::to_string(buffers.count) + " buffers para " + std::to_string(meshes.size()) + " malhas";
        return false;
    }

    for (size_t m = 0; m < meshes.size(); ++m) {
        MeshShape& mesh = meshes[m];
        const std::string where = "malha '" + mesh.name + "': ";
        FlatReader::Table buffer;
        FlatReader::Vector index_list, vertex_list;
        std::span<const uint8_t> index_bytes;
        if (!fb.TableElement(buffers, static_cast<uint32_t>(m), buffer) || !fb.VectorField(buffer, 0, 4, index_list) ||
            !fb.VectorField(buffer, 1, 4, vertex_list) || !BytesOf(fb, index_list, 0, index_bytes)) {
            error = where + "buffer ausente no trmbf";
            return false;
        }

        // Indices
        const uint32_t index_size = mesh.index_format == 0 ? 1 : mesh.index_format == 1 ? 2 : mesh.index_format == 2 ? 4 : 0;
        if (index_size == 0) {
            error = where + "formato de indice " + std::to_string(mesh.index_format) + " nao suportado";
            return false;
        }
        mesh.indices.resize(index_bytes.size() / index_size);
        for (size_t i = 0; i < mesh.indices.size(); ++i) {
            uint32_t value = 0;
            std::memcpy(&value, index_bytes.data() + i * index_size, index_size);
            mesh.indices[i] = value;
        }

        // Streams de vertices
        std::vector<std::span<const uint8_t>> streams(mesh.strides.size());
        for (size_t s = 0; s < streams.size(); ++s) {
            if (mesh.strides[s] == 0 || !BytesOf(fb, vertex_list, static_cast<uint32_t>(s), streams[s])) {
                error = where + "stream de vertices " + std::to_string(s) + " ausente";
                return false;
            }
            const size_t count = streams[s].size() / mesh.strides[s];
            if (s == 0) mesh.vertex_count = static_cast<uint32_t>(count);
            if (count != mesh.vertex_count || streams[s].size() % mesh.strides[s] != 0) {
                error = where + "tamanho do stream " + std::to_string(s) + " nao e multiplo do stride";
                return false;
            }
        }

        bool has_position = false;
        for (const VertexAttribute& attribute : mesh.attributes) {
            const VertexFormatInfo* format = FindVertexFormat(attribute.format);
            const auto semantic = static_cast<VertexSemantic>(attribute.semantic);
            if (!format || attribute.stream >= streams.size() ||
                attribute.offset + format->size > mesh.strides[attribute.stream]) {
                if (semantic == VertexSemantic::Position) {
                    error = where + "POSITION com formato " + std::to_string(attribute.format) + " nao suportado";
                    return false;
                }
                continue;  // atributo auxiliar desconhecido: ignorado
            }

            std::vector<float>* target = nullptr;
            size_t components = 0;
            switch (semantic) {
            case VertexSemantic::Position:     target = &mesh.positions; components = 3; has_position = true; break;
            case VertexSemantic::Normal:       target = &mesh.normals; components = 3; break;
            case VertexSemantic::Tangent:      target = &mesh.tangents; components = 4; break;
            case VertexSemantic::Color:        target = attribute.layer == 0 ? &mesh.colors : nullptr; components = 4; break;
            case VertexSemantic::TexCoord:     target = attribute.layer < 2 ? &mesh.texcoords[attribute.layer] : nullptr; components = 2; break;
            case VertexSemantic::BlendWeights: target = attribute.layer == 0 ? &mesh.weights : nullptr; components = 4; break;
            case VertexSemantic::BlendIndices: break;
            case VertexSemantic::Binormal:     break;
            }

            const std::span<const uint8_t> stream = streams[attribute.stream];
            const uint32_t stride = mesh.strides[attribute.stream];
            if (semantic == VertexSemantic::BlendIndices && attribute.layer == 0) {
                // 4 indices de osso de 8 bits (RGBA8 ou R32 empacotado)
                mesh.joints.resize(size_t{mesh.vertex_count} * 4);
                for (uint32_t v = 0; v < mesh.vertex_count; ++v) {
                    const uint8_t* src = stream.data() + size_t{v} * stride + attribute.offset;
                    for (size_t c = 0; c < 4; ++c) mesh.joints[size_t{v} * 4 + c] = src[c];
                }
                continue;
            }
            if (!target) continue;

            target->resize(size_t{mesh.vertex_count} * components);
            for (uint32_t v = 0; v < mesh.vertex_count; ++v) {
                float values[4];
                DecodeComponents(stream.data() + size_t{v} * stride + attribute.offset, *format, values);
                std::copy_n(values, components, target->data() + size_t{v} * components);
            }
        }
        if (!has_position) {
            error = where + "sem atributo POSITION";
            return false;
        }

        for (const Submesh& submesh : mesh.submeshes) {
            if (uint64_t{submesh.index_offset} + submesh.index_count > mesh.indices.size()) {
                error = where + "submalha '" + submesh.material + "' fora do buffer de indices";
                return false;
            }
        }
        for (uint32_t index : mesh.indices) {
            if (index >= mesh.vertex_count) {
                error = where + "indice " + std::to_string(index) + " fora dos " + std::to_string(mesh.vertex_count) + " vertices";
                return false;
            }
        }
    }
    return true;
}

bool ParseTrmtr(std::span<const uint8_t> data, std::vector<Material>& out, std::string& error) {
    out.clear();
    const FlatReader fb(data);
    FlatReader::Table root;
    FlatReader::Vector materials;
    if (!fb.Root(root) || !fb.VectorField(root, 1, 4, materials)) {
        error = "trmtr: FlatBuffer invalido";
        return false;
    }
    for (uint32_t i = 0; i < materials.count; ++i) {
        FlatReader::Table table;
        if (!fb.TableElement(materials, i, table)) continue;
        Material material;
        material.name = fb.StringField(table, 0);

        FlatReader::Vector shaders, list;
        FlatReader::Table shader, entry;
        if (fb.VectorField(table, 1, 4, shaders) && shaders.count > 0 && fb.TableElement(shaders, 0, shader)) {
            material.shader = fb.StringField(shader, 0);
            if (fb.VectorField(shader, 1, 4, list)) {
                for (uint32_t k = 0; k < list.count; ++k) {
                    if (fb.TableElement(list, k, entry)) material.options.emplace_back(fb.StringField(entry, 0), fb.StringField(entry, 1));
                }
            }
        }
        if (fb.VectorField(table, 2, 4, list)) {
            for (uint32_t k = 0; k < list.count; ++k) {
                if (fb.TableElement(list, k, entry)) {
                    material.textures.push_back({fb.StringField(entry, 0), fb.StringField(entry, 1), fb.Scalar<uint32_t>(entry, 2, 0)});
                }
            }
        }
        if (fb.VectorField(table, 3, 4, list)) {
            for (uint32_t k = 0; k < list.count; ++k) {
                if (fb.TableElement(list, k, entry)) material.samplers.push_back({fb.Scalar<uint32_t>(entry, 9, 0), fb.Scalar<uint32_t>(entry, 10, 0)});
            }
        }
        if (fb.VectorField(table, 4, 4, list)) {
            for (uint32_t k = 0; k < list.count; ++k) {
                if (fb.TableElement(list, k, entry)) material.floats.emplace_back(fb.StringField(entry, 0), fb.Scalar<float>(entry, 1, 0.0f));
            }
        }
        if (fb.VectorField(table, 7, 4, list)) {
            for (uint32_t k = 0; k < list.count; ++k) {
                std::array<float, 4> value{0, 0, 0, 0};
                if (fb.TableElement(list, k, entry)) {
                    fb.StructField(entry, 1, value);
                    material.vectors.emplace_back(fb.StringField(entry, 0), value);
                }
            }
        }
        if (fb.VectorField(table, 9, 4, list)) {
            for (uint32_t k = 0; k < list.count; ++k) {
                if (fb.TableElement(list, k, entry)) material.ints.emplace_back(fb.StringField(entry, 0), fb.Scalar<uint32_t>(entry, 1, 0));
            }
        }
        out.push_back(std::move(material));
    }
    return true;
}

bool ParseTrskl(std::span<const uint8_t> data, Skeleton& out, std::string& error) {
    out = {};
    const FlatReader fb(data);
    FlatReader::Table root;
    FlatReader::Vector nodes, bones;
    if (!fb.Root(root) || !fb.VectorField(root, 1, 4, nodes)) {
        error = "trskl: FlatBuffer invalido";
        return false;
    }

    for (uint32_t i = 0; i < nodes.count; ++i) {
        FlatReader::Table table, transform;
        if (!fb.TableElement(nodes, i, table)) continue;
        SkeletonNode node;
        node.name = fb.StringField(table, 0);
        if (fb.TableField(table, 1, transform)) {
            fb.StructField(transform, 0, node.scale);
            fb.StructField(transform, 1, node.rotation);
            fb.StructField(transform, 2, node.translation);
        }
        node.parent = fb.Scalar<int32_t>(table, 4, -1);
        node.bone = fb.Scalar<int32_t>(table, 5, -1);
        if (node.parent >= static_cast<int32_t>(nodes.count) || node.parent == static_cast<int32_t>(i)) node.parent = -1;
        node.orientation = EulerXyzToQuaternion(node.rotation);
        out.nodes.push_back(std::move(node));
    }

    if (fb.VectorField(root, 2, 4, bones)) {
        for (uint32_t i = 0; i < bones.count; ++i) {
            FlatReader::Table bone, matrix;
            std::array<float, 16> inverse_bind{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
            if (fb.TableElement(bones, i, bone) && fb.TableField(bone, 2, matrix)) {
                for (unsigned column = 0; column < 4; ++column) {
                    std::array<float, 3> values{};
                    if (fb.StructField(matrix, column, values)) std::copy(values.begin(), values.end(), inverse_bind.begin() + 4 * column);
                }
            }
            out.inverse_bind.push_back(inverse_bind);
        }
    }
    UpdateSkeletonMatrices(out);
    return true;
}

bool ExportToGltf(const Model& model, const std::string& out_path, const TextureUris& texture_uris, std::string& error,
                  const ModelExportOptions& options) {
    const fs_std::path gltf_path(out_path);
    fs_std::path bin_path = gltf_path;
    bin_path.replace_extension(".bin");

    GltfBuffers buffers;
    std::vector<std::string> images, textures, materials, meshes, nodes, skins, animations;
    std::map<std::string, size_t> texture_index, material_index;
    bool uses_dds = false;

    // PNG entra no nucleo do glTF; DDS via MSFT_texture_dds
    const auto texture_ref = [&](const std::string& uri) -> std::string {
        const auto [entry, inserted] = texture_index.try_emplace(uri, textures.size());
        if (inserted) {
            const bool png = IsPng(uri);
            images.push_back("{\"uri\":\"" + JsonEscape(uri) + "\",\"mimeType\":\"" + (png ? "image/png" : "image/vnd-ms.dds") + "\"}");
            const std::string source = Num(images.size() - 1);
            textures.push_back(png ? "{\"sampler\":0,\"source\":" + source + "}"
                                   : "{\"sampler\":0,\"extensions\":{\"MSFT_texture_dds\":{\"source\":" + source + "}}}");
            uses_dds = uses_dds || !png;
        }
        return "{\"index\":" + Num(entry->second) + "}";
    };
    const auto texture_for = [&](const Material& material, std::string_view slot) -> std::string {
        const MaterialTexture* texture = material.Find(slot);
        if (!texture) return {};
        const auto uri = texture_uris.find(texture->file);
        return uri == texture_uris.end() ? std::string() : texture_ref(uri->second);
    };

    for (const Material& material : model.materials) {
        if (material_index.count(material.name)) continue;
        std::string json = "{\"name\":\"" + JsonEscape(material.name) + "\",\"pbrMetallicRoughness\":{\"metallicFactor\":0,\"roughnessFactor\":1";
        const auto baked = options.base_color_uris.find(material.name);
        if (baked != options.base_color_uris.end()) {
            json += ",\"baseColorTexture\":" + texture_ref(baked->second);
        } else if (const std::string base = texture_for(material, "BaseColorMap"); !base.empty()) {
            json += ",\"baseColorTexture\":" + base;
        }
        json += "}";
        if (const std::string normal = texture_for(material, "NormalMap"); !normal.empty()) json += ",\"normalTexture\":" + normal;
        if (const std::string occlusion = texture_for(material, "OcclusionMap"); !occlusion.empty()) json += ",\"occlusionTexture\":" + occlusion;

        // Parametros completos do shader Trinity, para reimplementacao fora do glTF padrao
        const auto object = [](const auto& pairs, const auto& format) {
            std::string out = "{";
            for (size_t i = 0; i < pairs.size(); ++i) {
                out += std::string(i ? "," : "") + "\"" + JsonEscape(pairs[i].first) + "\":" + format(pairs[i].second);
            }
            return out + "}";
        };
        std::vector<std::pair<std::string, std::string>> slots;
        for (const MaterialTexture& texture : material.textures) {
            const auto uri = texture_uris.find(texture.file);
            slots.emplace_back(texture.slot, uri != texture_uris.end() ? uri->second : texture.file);
        }
        const auto quoted = [](const std::string& value) { return "\"" + JsonEscape(value) + "\""; };
        json += ",\"extras\":{\"trinity\":{\"shader\":\"" + JsonEscape(material.shader) + "\"" +
                ",\"options\":" + object(material.options, quoted) + ",\"textures\":" + object(slots, quoted) +
                ",\"floats\":" + object(material.floats, [](float value) { return Num(value); }) +
                ",\"vectors\":" + object(material.vectors, [](const std::array<float, 4>& value) { return Vec(value); }) +
                ",\"ints\":" + object(material.ints, [](uint32_t value) { return std::to_string(value); }) + "}}";
        material_index[material.name] = materials.size();
        materials.push_back(json + "}");
    }

    // Esqueleto: um no glTF por no do .trskl (mesmos indices); skin apenas se houver malhas com pesos
    std::vector<size_t> joints;
    const bool has_weights = std::any_of(model.meshes.begin(), model.meshes.end(),
                                         [](const MeshShape& mesh) { return !mesh.joints.empty() && !mesh.weights.empty(); });
    const bool skinned = has_weights && BuildJointList(model, joints);
    std::vector<size_t> scene_roots;
    if (!model.skeleton.nodes.empty()) {
        const Skeleton& skeleton = model.skeleton;
        std::vector<std::vector<size_t>> children(skeleton.nodes.size());
        for (size_t i = 0; i < skeleton.nodes.size(); ++i) {
            const int32_t parent = skeleton.nodes[i].parent;
            if (parent >= 0) children[static_cast<size_t>(parent)].push_back(i);
            else scene_roots.push_back(i);
        }
        for (size_t i = 0; i < skeleton.nodes.size(); ++i) {
            const SkeletonNode& node = skeleton.nodes[i];
            std::string json = "{\"name\":\"" + JsonEscape(node.name) + "\",\"translation\":" + Vec(node.translation) +
                               ",\"rotation\":" + Vec(node.orientation) + ",\"scale\":" + Vec(node.scale);
            if (!children[i].empty()) {
                std::vector<std::string> list;
                for (size_t child : children[i]) list.push_back(Num(child));
                json += ",\"children\":" + JoinJson(list);
            }
            nodes.push_back(json + "}");
        }
    }
    if (skinned) {
        const Skeleton& skeleton = model.skeleton;
        std::vector<float> inverse_bind;
        for (const auto& matrix : skeleton.inverse_bind) inverse_bind.insert(inverse_bind.end(), matrix.begin(), matrix.end());
        const size_t ibm = buffers.AddArray(inverse_bind, kFloat, skeleton.inverse_bind.size(), "MAT4", 0);
        std::vector<std::string> joint_list;
        for (size_t joint : joints) joint_list.push_back(Num(joint));
        skins.push_back("{\"name\":\"" + JsonEscape(model.name) + "\",\"inverseBindMatrices\":" + Num(ibm) +
                        ",\"joints\":" + JoinJson(joint_list) + "}");
    }

    // Animacoes: um sampler LINEAR por canal animado, ligado ao no de mesmo nome (indices = nos do esqueleto)
    if (!model.skeleton.nodes.empty()) {
        std::map<std::string_view, size_t> node_by_name;
        for (size_t i = 0; i < model.skeleton.nodes.size(); ++i) node_by_name.emplace(model.skeleton.nodes[i].name, i);

        for (const AnimationClip* clip : options.animations) {
            if (!clip) continue;
            const float fps = clip->fps ? static_cast<float>(clip->fps) : 30.0f;
            std::vector<std::string> samplers, channels;
            const auto add_channel = [&](size_t node, const char* path, const auto& channel, size_t components) {
                if (channel.Empty()) return;
                std::vector<float> times, values;
                for (const auto& key : channel.keys) {
                    times.push_back(key.frame / fps);
                    values.insert(values.end(), key.value.begin(), key.value.begin() + static_cast<std::ptrdiff_t>(components));
                }
                if (components == 4) {  // quaternions unitarios no mesmo hemisferio do anterior
                    for (size_t k = 0; k < times.size(); ++k) {
                        float* q = values.data() + 4 * k;
                        const float length = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
                        for (size_t c = 0; c < 4; ++c) q[c] /= length > 0 ? length : 1.0f;
                        if (k > 0 && q[0] * q[-4] + q[1] * q[-3] + q[2] * q[-2] + q[3] * q[-1] < 0) {
                            for (size_t c = 0; c < 4; ++c) q[c] = -q[c];
                        }
                    }
                }
                const size_t input = buffers.AddArray(times, kFloat, times.size(), "SCALAR", 0,
                                                      ",\"min\":[" + Num(times.front()) + "],\"max\":[" + Num(times.back()) + "]");
                const size_t output = buffers.AddArray(values, kFloat, times.size(), components == 4 ? "VEC4" : "VEC3", 0);
                samplers.push_back("{\"input\":" + Num(input) + ",\"output\":" + Num(output) + ",\"interpolation\":\"LINEAR\"}");
                channels.push_back("{\"sampler\":" + Num(samplers.size() - 1) + ",\"target\":{\"node\":" + Num(node) +
                                   ",\"path\":\"" + path + "\"}}");
            };
            for (const AnimationTrack& track : clip->tracks) {
                const auto node = node_by_name.find(track.bone);
                if (node == node_by_name.end()) continue;
                add_channel(node->second, "translation", track.translation, 3);
                add_channel(node->second, "rotation", track.rotation, 4);
                add_channel(node->second, "scale", track.scale, 3);
            }
            if (!channels.empty()) {
                animations.push_back("{\"name\":\"" + JsonEscape(clip->name) + "\",\"samplers\":" + JoinJson(samplers) +
                                     ",\"channels\":" + JoinJson(channels) + "}");
            }
        }
    }

    for (const MeshShape& mesh : model.meshes) {
        const size_t count = mesh.vertex_count;
        std::string attributes;
        const auto add_attribute = [&](const char* name, size_t accessor) {
            attributes += std::string(attributes.empty() ? "" : ",") + "\"" + name + "\":" + Num(accessor);
        };

        std::array<float, 3> low{INFINITY, INFINITY, INFINITY}, high{-INFINITY, -INFINITY, -INFINITY};
        for (size_t v = 0; v < count; ++v) {
            for (size_t c = 0; c < 3; ++c) {
                low[c] = std::min(low[c], mesh.positions[v * 3 + c]);
                high[c] = std::max(high[c], mesh.positions[v * 3 + c]);
            }
        }
        add_attribute("POSITION", buffers.AddArray(mesh.positions, kFloat, count, "VEC3", kArrayBuffer,
                                                   ",\"min\":" + Vec(low) + ",\"max\":" + Vec(high)));

        if (mesh.normals.size() == count * 3) {
            std::vector<float> normals(mesh.normals);
            for (size_t v = 0; v < count; ++v) {
                float* n = normals.data() + v * 3;
                const float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                if (length > 1e-8f) {
                    for (size_t c = 0; c < 3; ++c) n[c] /= length;
                } else {
                    n[0] = 0, n[1] = 1, n[2] = 0;
                }
            }
            add_attribute("NORMAL", buffers.AddArray(normals, kFloat, count, "VEC3"));
        }
        if (mesh.tangents.size() == count * 4 && mesh.normals.size() == count * 3) {
            std::vector<float> tangents(mesh.tangents);
            for (size_t v = 0; v < count; ++v) {
                float* t = tangents.data() + v * 4;
                const float length = std::sqrt(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);
                if (length > 1e-8f) {
                    for (size_t c = 0; c < 3; ++c) t[c] /= length;
                } else {
                    t[0] = 1, t[1] = 0, t[2] = 0;
                }
                t[3] = t[3] < 0 ? -1.0f : 1.0f;
            }
            add_attribute("TANGENT", buffers.AddArray(tangents, kFloat, count, "VEC4"));
        }
        for (size_t layer = 0; layer < 2; ++layer) {
            if (mesh.texcoords[layer].size() == count * 2) {
                add_attribute(layer == 0 ? "TEXCOORD_0" : "TEXCOORD_1", buffers.AddArray(mesh.texcoords[layer], kFloat, count, "VEC2"));
            }
        }
        // Cor de vertice na Trinity costuma ser mascara: exportada como atributo de aplicacao para nao tingir o modelo
        if (mesh.colors.size() == count * 4) add_attribute("_COLOR_0", buffers.AddArray(mesh.colors, kFloat, count, "VEC4"));

        if (mesh.joints.size() == count * 4 && mesh.weights.size() == count * 4) {
            std::vector<float> weights(mesh.weights);
            std::vector<uint16_t> joint_indices(mesh.joints);
            for (size_t v = 0; v < count; ++v) {
                float* w = weights.data() + v * 4;
                const float sum = w[0] + w[1] + w[2] + w[3];
                if (sum > 1e-6f) {
                    for (size_t c = 0; c < 4; ++c) w[c] /= sum;
                } else {
                    w[0] = 1, w[1] = w[2] = w[3] = 0;
                }
            }
            add_attribute(skinned ? "JOINTS_0" : "_JOINTS_0", buffers.AddArray(joint_indices, kUnsignedShort, count, "VEC4"));
            add_attribute(skinned ? "WEIGHTS_0" : "_WEIGHTS_0", buffers.AddArray(weights, kFloat, count, "VEC4"));
        }

        // Indices: um bufferView por malha, um accessor por submalha
        const bool wide = count > 65535;
        size_t index_view = 0;
        if (wide) {
            index_view = buffers.AddView(mesh.indices.data(), mesh.indices.size() * 4, kElementArrayBuffer);
        } else {
            std::vector<uint16_t> narrow(mesh.indices.begin(), mesh.indices.end());
            index_view = buffers.AddView(narrow.data(), narrow.size() * 2, kElementArrayBuffer);
        }

        std::vector<std::string> primitives;
        std::vector<Submesh> submeshes = mesh.submeshes;
        if (submeshes.empty()) submeshes.push_back({static_cast<uint32_t>(mesh.indices.size()), 0, std::string()});
        for (const Submesh& submesh : submeshes) {
            if (submesh.index_count == 0) continue;
            const size_t indices = buffers.AddAccessor(index_view, size_t{submesh.index_offset} * (wide ? 4 : 2),
                                                       wide ? kUnsignedInt : kUnsignedShort, submesh.index_count, "SCALAR");
            std::string primitive = "{\"attributes\":{" + attributes + "},\"indices\":" + Num(indices);
            if (!submesh.material.empty()) {
                auto material = material_index.find(submesh.material);
                if (material == material_index.end()) {
                    material = material_index.emplace(submesh.material, materials.size()).first;
                    materials.push_back("{\"name\":\"" + JsonEscape(submesh.material) + "\"}");
                }
                primitive += ",\"material\":" + Num(material->second);
            }
            primitives.push_back(primitive + "}");
        }

        meshes.push_back("{\"name\":\"" + JsonEscape(mesh.name) + "\",\"primitives\":" + JoinJson(primitives) + "}");
        scene_roots.push_back(nodes.size());
        nodes.push_back("{\"name\":\"" + JsonEscape(mesh.name) + "\",\"mesh\":" + Num(meshes.size() - 1) +
                        (skinned ? ",\"skin\":0" : "") + "}");
    }

    std::vector<std::string> roots;
    for (size_t root : scene_roots) roots.push_back(Num(root));

    std::string json = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"za_port trpfs_tool\"}";
    if (uses_dds) json += ",\"extensionsUsed\":[\"MSFT_texture_dds\"]";
    json += ",\"scene\":0,\"scenes\":[{\"name\":\"" + JsonEscape(model.name) + "\",\"nodes\":" + JoinJson(roots) + "}]";
    json += ",\"nodes\":" + JoinJson(nodes);
    if (!meshes.empty()) json += ",\"meshes\":" + JoinJson(meshes);
    if (!materials.empty()) json += ",\"materials\":" + JoinJson(materials);
    if (!textures.empty()) {
        json += ",\"textures\":" + JoinJson(textures) + ",\"images\":" + JoinJson(images) +
                ",\"samplers\":[{\"wrapS\":10497,\"wrapT\":10497}]";
    }
    if (!skins.empty()) json += ",\"skins\":" + JoinJson(skins);
    if (!animations.empty()) json += ",\"animations\":" + JoinJson(animations);
    json += ",\"accessors\":" + JoinJson(buffers.Accessors()) + ",\"bufferViews\":" + JoinJson(buffers.Views()) +
            ",\"buffers\":[{\"uri\":\"" + JsonEscape(bin_path.filename().string()) + "\",\"byteLength\":" +
            Num(buffers.Bytes().size()) + "}]}";

    return WriteFile(bin_path, buffers.Bytes().data(), buffers.Bytes().size(), error) &&
           WriteFile(gltf_path, json.data(), json.size(), error);
}

bool ExportToObj(const Model& model, const std::string& out_path, const TextureUris& texture_uris, std::string& error,
                 const ModelExportOptions& options) {
    const fs_std::path obj_path(out_path);
    fs_std::path mtl_path = obj_path;
    mtl_path.replace_extension(".mtl");

    std::string obj = "# za_port trpfs_tool: " + model.name + "\nmtllib " + mtl_path.filename().string() + "\n";
    size_t base = 1;
    for (const MeshShape& mesh : model.meshes) {
        const size_t count = mesh.vertex_count;
        const bool has_uv = mesh.texcoords[0].size() == count * 2;
        const bool has_normal = mesh.normals.size() == count * 3;
        obj += "o " + mesh.name + "\n";
        for (size_t v = 0; v < count; ++v) {
            const float* p = mesh.positions.data() + v * 3;
            obj += "v " + Num(p[0]) + " " + Num(p[1]) + " " + Num(p[2]) + "\n";
        }
        if (has_uv) {
            for (size_t v = 0; v < count; ++v) {  // OBJ tem origem do V embaixo
                obj += "vt " + Num(mesh.texcoords[0][v * 2]) + " " + Num(1.0f - mesh.texcoords[0][v * 2 + 1]) + "\n";
            }
        }
        if (has_normal) {
            for (size_t v = 0; v < count; ++v) {
                const float* n = mesh.normals.data() + v * 3;
                obj += "vn " + Num(n[0]) + " " + Num(n[1]) + " " + Num(n[2]) + "\n";
            }
        }
        for (const Submesh& submesh : mesh.submeshes) {
            obj += "g " + mesh.name + "_" + submesh.material + "\nusemtl " + submesh.material + "\n";
            for (size_t i = submesh.index_offset; i + 2 < size_t{submesh.index_offset} + submesh.index_count; i += 3) {
                obj += "f";
                for (size_t k = 0; k < 3; ++k) {
                    const std::string index = Num(base + mesh.indices[i + k]);
                    obj += " " + index;
                    if (has_uv || has_normal) obj += "/" + (has_uv ? index : std::string()) + (has_normal ? "/" + index : std::string());
                }
                obj += "\n";
            }
        }
        base += count;
    }

    std::string mtl = "# za_port trpfs_tool: " + model.name + "\n";
    for (const Material& material : model.materials) {
        mtl += "newmtl " + material.name + "\nKd 1 1 1\n";
        const auto map = [&](const char* key, std::string_view slot) {
            const MaterialTexture* texture = material.Find(slot);
            if (!texture) return;
            if (const auto uri = texture_uris.find(texture->file); uri != texture_uris.end()) mtl += std::string(key) + " " + uri->second + "\n";
        };
        if (const auto baked = options.base_color_uris.find(material.name); baked != options.base_color_uris.end()) {
            mtl += "map_Kd " + baked->second + "\n";
        } else {
            map("map_Kd", "BaseColorMap");
        }
        map("map_Bump", "NormalMap");
        map("map_Ka", "OcclusionMap");
        mtl += "\n";
    }

    return WriteFile(mtl_path, mtl.data(), mtl.size(), error) && WriteFile(obj_path, obj.data(), obj.size(), error);
}

} // namespace za::gfx
