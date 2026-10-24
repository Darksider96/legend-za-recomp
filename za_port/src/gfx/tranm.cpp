#include "gfx/tranm.hpp"

#include "za/flat_reader.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_map>

namespace za::gfx {

namespace {

using za::FlatReader;

bool ReadVec3(const FlatReader& fb, size_t pos, Vec3f& out) {
    return fb.Read(pos, out);
}

bool ReadQuat48(const FlatReader& fb, size_t pos, Quat& out) {
    uint16_t packed[3];
    if (!fb.Read(pos, packed)) return false;
    out = UnpackQuaternion48(packed[0], packed[1], packed[2]);
    return true;
}

// Canal union: campo de tipo (u8) + tabela Fixed/Dynamic/Framed16/Framed8
template <typename T, typename ReadValue>
bool ReadChannel(const FlatReader& fb, const FlatReader::Table& track, unsigned type_field, unsigned data_field,
                 size_t value_size, ReadValue read_value, AnimationChannel<T>& out, std::string& error) {
    const auto kind = static_cast<AnimationChannelKind>(fb.Scalar<uint8_t>(track, type_field, 0));
    FlatReader::Table table;
    if (kind == AnimationChannelKind::None || !fb.TableField(track, data_field, table)) return true;
    out.kind = kind;

    T value{};
    switch (kind) {
    case AnimationChannelKind::Fixed: {
        const size_t pos = fb.Field(table, 0);
        if (pos == 0 || !read_value(fb, pos, value)) break;
        out.keys.push_back({0.0f, value});
        return true;
    }
    case AnimationChannelKind::Dynamic: {
        FlatReader::Vector values;
        if (!fb.VectorField(table, 0, value_size, values)) break;
        for (uint32_t i = 0; i < values.count; ++i) {
            if (!read_value(fb, values.data + i * value_size, value)) return false;
            out.keys.push_back({static_cast<float>(i), value});
        }
        return true;
    }
    case AnimationChannelKind::Framed16:
    case AnimationChannelKind::Framed8: {
        const size_t frame_size = kind == AnimationChannelKind::Framed16 ? 2 : 1;
        FlatReader::Vector frames, values;
        if (!fb.VectorField(table, 0, frame_size, frames) || !fb.VectorField(table, 1, value_size, values) ||
            frames.count != values.count) {
            break;
        }
        for (uint32_t i = 0; i < values.count; ++i) {
            uint16_t frame = 0;
            if (frame_size == 2) {
                fb.Read(frames.data + 2 * size_t{i}, frame);
            } else {
                uint8_t small = 0;
                fb.Read(frames.data + i, small);
                frame = small;
            }
            if (!read_value(fb, values.data + i * value_size, value)) return false;
            if (!out.keys.empty() && static_cast<float>(frame) <= out.keys.back().frame) continue;  // quadros crescentes
            out.keys.push_back({static_cast<float>(frame), value});
        }
        return true;
    }
    case AnimationChannelKind::None:
        return true;
    }
    error = "canal de animacao do tipo " + std::to_string(static_cast<int>(kind)) + " invalido";
    return false;
}

template <typename T, typename Blend>
T SampleChannel(const AnimationChannel<T>& channel, float frame, Blend blend) {
    const auto& keys = channel.keys;
    if (keys.size() == 1 || frame <= keys.front().frame) return keys.front().value;
    if (frame >= keys.back().frame) return keys.back().value;
    const auto next = std::upper_bound(keys.begin(), keys.end(), frame,
                                       [](float f, const AnimationKey<T>& key) { return f < key.frame; });
    const auto previous = next - 1;
    const float t = (frame - previous->frame) / (next->frame - previous->frame);
    return blend(previous->value, next->value, t);
}

Vec3f Lerp(const Vec3f& a, const Vec3f& b, float t) {
    return {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t};
}

Quat Slerp(const Quat& a, Quat b, float t) {
    float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (dot < 0) {  // caminho mais curto
        for (float& c : b) c = -c;
        dot = -dot;
    }
    float wa = 1 - t, wb = t;
    if (dot < 0.9995f) {
        const float theta = std::acos(std::min(dot, 1.0f));
        const float sin_theta = std::sin(theta);
        wa = std::sin((1 - t) * theta) / sin_theta;
        wb = std::sin(t * theta) / sin_theta;
    }
    Quat q{a[0] * wa + b[0] * wb, a[1] * wa + b[1] * wb, a[2] * wa + b[2] * wb, a[3] * wa + b[3] * wb};
    const float length = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    for (float& c : q) c /= length > 0 ? length : 1.0f;
    return q;
}

} // namespace

Quat UnpackQuaternion48(uint16_t x, uint16_t y, uint16_t z) {
    const uint64_t packed = (uint64_t{z} << 32) | (uint64_t{y} << 16) | x;
    constexpr double kScale = std::numbers::pi / 2.0 / 0x7FFF;
    constexpr double kOffset = std::numbers::pi / 4.0;
    const double small[3] = {static_cast<double>((packed >> 3) & 0x7FFF) * kScale - kOffset,
                             static_cast<double>((packed >> 18) & 0x7FFF) * kScale - kOffset,
                             static_cast<double>((packed >> 33) & 0x7FFF) * kScale - kOffset};
    const double largest = std::sqrt(std::max(0.0, 1.0 - small[0] * small[0] - small[1] * small[1] - small[2] * small[2]));

    // Maior componente omitido (positivo); o bit de sinal nega o quaternion inteiro — mesma rotacao, mas
    // preserva o hemisferio escolhido pelo exportador para a interpolacao entre chaves
    const size_t missing = packed & 3;
    const double sign = (packed & 4) ? -1.0 : 1.0;
    Quat q{};
    for (size_t i = 0, k = 0; i < 4; ++i) q[i] = static_cast<float>(sign * (i == missing ? largest : small[k++]));
    return q;
}

bool ParseTranm(std::span<const uint8_t> data, AnimationClip& out, std::string& error) {
    out = {};
    const FlatReader fb(data);
    FlatReader::Table root, info, skeleton;
    if (!fb.Root(root) || !fb.TableField(root, 0, info)) {
        error = "tranm: FlatBuffer invalido";
        return false;
    }
    out.loop = fb.Scalar<uint32_t>(info, 0, 0) != 0;
    out.frame_count = fb.Scalar<uint32_t>(info, 1, 0);
    out.fps = fb.Scalar<uint32_t>(info, 2, 30);

    FlatReader::Vector tracks;
    if (!fb.TableField(root, 1, skeleton) || !fb.VectorField(skeleton, 0, 4, tracks)) return true;  // sem trilhas de osso

    out.tracks.reserve(tracks.count);
    for (uint32_t i = 0; i < tracks.count; ++i) {
        FlatReader::Table table;
        if (!fb.TableElement(tracks, i, table)) {
            error = "tranm: trilha #" + std::to_string(i) + " invalida";
            return false;
        }
        AnimationTrack track;
        track.bone = fb.StringField(table, 0);
        if (!ReadChannel(fb, table, 1, 2, 12, ReadVec3, track.scale, error) ||
            !ReadChannel(fb, table, 3, 4, 6, ReadQuat48, track.rotation, error) ||
            !ReadChannel(fb, table, 5, 6, 12, ReadVec3, track.translation, error)) {
            error = "tranm: osso '" + track.bone + "': " + error;
            return false;
        }
        out.tracks.push_back(std::move(track));
    }
    return true;
}

bool ParseTracn(std::span<const uint8_t> data, std::vector<AnimationSetEntry>& out, std::string& error) {
    out.clear();
    const FlatReader fb(data);
    FlatReader::Table root;
    FlatReader::Vector entries;
    if (!fb.Root(root) || !fb.VectorField(root, 0, 4, entries)) {
        error = "tracn: FlatBuffer invalido";
        return false;
    }
    for (uint32_t i = 0; i < entries.count; ++i) {
        FlatReader::Table entry;
        if (fb.TableElement(entries, i, entry)) out.push_back({fb.StringField(entry, 0), fb.StringField(entry, 1)});
    }
    return true;
}

void EvaluateAnimation(const AnimationClip& clip, float time_seconds, Skeleton& inout_skeleton) {
    float frame = time_seconds * static_cast<float>(clip.fps);
    const float last = clip.frame_count > 1 ? static_cast<float>(clip.frame_count - 1) : 0.0f;
    if (last > 0) frame = clip.loop ? frame - std::floor(frame / last) * last : std::clamp(frame, 0.0f, last);

    std::unordered_map<std::string_view, size_t> node_by_name;
    for (size_t i = 0; i < inout_skeleton.nodes.size(); ++i) node_by_name.emplace(inout_skeleton.nodes[i].name, i);

    for (const AnimationTrack& track : clip.tracks) {
        const auto node = node_by_name.find(track.bone);
        if (node == node_by_name.end()) continue;
        SkeletonNode& target = inout_skeleton.nodes[node->second];
        if (!track.scale.Empty()) target.scale = SampleChannel(track.scale, frame, Lerp);
        if (!track.rotation.Empty()) target.orientation = SampleChannel(track.rotation, frame, Slerp);
        if (!track.translation.Empty()) target.translation = SampleChannel(track.translation, frame, Lerp);
    }
    UpdateSkeletonMatrices(inout_skeleton);
}

} // namespace za::gfx
