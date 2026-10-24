#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

// Recovered from exefs/main and all 394 local .trpokecfg assets.
// The original field names were not retained. fNN means wire field index NN.
// Wire tables use FlatBuffers vtables: these are NOT fixed-offset disk PODs.
namespace za::re::pokemon::config {
static_assert(std::endian::native == std::endian::little);

struct Float3 { float x, y, z; };
struct alignas(16) Float3Padded { float x, y, z, padding; };

// Storage of the 0x220-byte component created at VA 0x7101163578 and
// initialized by 0x7100302958. Unknown base-class members remain opaque.
// Addresses are target 64-bit addresses, not native host ownership pointers.
struct alignas(16) PokeConfigRuntimeStorage {
    std::byte opaque_000[0x168];
    std::uint64_t flatbuffer_root_address; // 0x168
    std::byte opaque_170[0x0c];
    float f03; // 0x17c, default 1.0
    float f04; // 0x180, default 0.0
    float f05; // 0x184, default 0.0
    float f06; // 0x188, default 0.0
    float f07; // 0x18c, default 0.0; converted degrees->radians by 104d1d0
    float f08; // 0x190, default 0.0; converted degrees->radians by 104d1d0
    std::byte opaque_194[0x0c];
    std::uint8_t f20; // 0x1a0; boolean storage
    std::uint8_t f21; // 0x1a1; default 1, enum meaning unresolved
    std::uint8_t f22; // 0x1a2; boolean storage
    std::byte opaque_1a3[0x0d];
    Float3Padded f23; // 0x1b0; wire has only x/y/z (12 bytes)
    std::uint8_t f13; // 0x1c0; boolean storage
    std::byte opaque_1c1[0x0f];
    Float3Padded f14_f15_f16; // 0x1d0; defaults (1, 0.1, 1), pad 0
    Float3Padded f17_f18_f19; // 0x1e0; defaults (0, -0.05, 0), pad 0
    std::byte opaque_1f0[0x20];
    float interpolated_factor_210; // f1 + easing(clamp(input,-1,1)*.5+.5)*(f2-f1)
    std::byte opaque_214[0x0c];
};

static_assert(sizeof(Float3) == 12);
static_assert(sizeof(Float3Padded) == 16);
static_assert(sizeof(PokeConfigRuntimeStorage) == 0x220);
static_assert(offsetof(PokeConfigRuntimeStorage, flatbuffer_root_address) == 0x168);
static_assert(offsetof(PokeConfigRuntimeStorage, f03) == 0x17c);
static_assert(offsetof(PokeConfigRuntimeStorage, f08) == 0x190);
static_assert(offsetof(PokeConfigRuntimeStorage, f20) == 0x1a0);
static_assert(offsetof(PokeConfigRuntimeStorage, f21) == 0x1a1);
static_assert(offsetof(PokeConfigRuntimeStorage, f22) == 0x1a2);
static_assert(offsetof(PokeConfigRuntimeStorage, f23) == 0x1b0);
static_assert(offsetof(PokeConfigRuntimeStorage, f13) == 0x1c0);
static_assert(offsetof(PokeConfigRuntimeStorage, f14_f15_f16) == 0x1d0);
static_assert(offsetof(PokeConfigRuntimeStorage, f17_f18_f19) == 0x1e0);
static_assert(offsetof(PokeConfigRuntimeStorage, interpolated_factor_210) == 0x210);

enum class WireKind : std::uint8_t { U8, Bool8, Float32, Float3Inline };
inline constexpr std::array<WireKind,24> kWireKinds = {
    WireKind::U8, WireKind::Float32, WireKind::Float32,
    WireKind::Float32, WireKind::Float32, WireKind::Float32,
    WireKind::Float32, WireKind::Float32, WireKind::Float32,
    WireKind::Bool8, WireKind::Float32, WireKind::Float32,
    WireKind::Float32, WireKind::Bool8, WireKind::Float32,
    WireKind::Float32, WireKind::Float32, WireKind::Float32,
    WireKind::Float32, WireKind::Float32, WireKind::Bool8,
    WireKind::U8, WireKind::Bool8, WireKind::Float3Inline
};

// Scalar defaults recovered from code, not inferred from the most common asset.
inline constexpr std::array<float,24> kScalarDefaults = {
    1.0f, 0.8f, 1.2f, 1.0f, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1.0f, 0.1f, 1.0f, 0, -0.05f, 0, 0, 1.0f, 0, 0
};

// Bounds-checked little-endian view for the observed FlatBuffers table.
// Does not invent absent original field names or nested attachment records.
class TrPokeCfgView {
    std::span<const std::byte> bytes_;
    std::size_t root_ = 0, vtable_ = 0;
    std::uint16_t vtable_size_ = 0, object_size_ = 0;
    bool valid_ = false;
    template<class T> bool read(std::size_t at, T& out) const {
        if (at > bytes_.size() || sizeof(T) > bytes_.size()-at) return false;
        std::memcpy(&out, bytes_.data()+at, sizeof(T)); return true;
    }
    std::size_t field(unsigned index, std::size_t width) const {
        if (!valid_ || index >= 24 || 4+2*index+2 > vtable_size_) return 0;
        std::uint16_t offset=0;
        if (!read(vtable_+4+2*index, offset) || !offset || offset > object_size_
            || width > std::size_t(object_size_-offset)) return 0;
        return root_+offset;
    }
public:
    explicit TrPokeCfgView(std::span<const std::byte> bytes) : bytes_(bytes) {
        std::uint32_t root=0; std::int32_t displacement=0;
        if (!read(0, root) || !read(root, displacement)) return;
        const auto vt=std::int64_t(root)-displacement;
        if (vt < 0 || std::uint64_t(vt) > bytes.size()) return;
        root_=root; vtable_=std::size_t(vt);
        if (!read(vtable_,vtable_size_) || !read(vtable_+2,object_size_)
            || vtable_size_<4 || (vtable_size_&1) || vtable_>bytes.size()
            || vtable_size_>bytes.size()-vtable_ || root_>bytes.size()
            || object_size_>bytes.size()-root_) return;
        valid_=true;
    }
    bool valid() const { return valid_; }
    std::uint8_t byte(unsigned index) const {
        if (index>=24 || (kWireKinds[index]!=WireKind::U8 && kWireKinds[index]!=WireKind::Bool8)) return 0;
        std::uint8_t value=(index==0 || index==21) ? 1 : 0;
        if (auto pos=field(index,1)) read(pos,value);
        return value;
    }
    float scalar(unsigned index) const {
        if (index>=24 || kWireKinds[index]!=WireKind::Float32) return 0;
        float value=kScalarDefaults[index];
        if (auto pos=field(index,4)) read(pos,value);
        return value;
    }
    Float3 vector23() const {
        Float3 value{};
        if (auto pos=field(23,sizeof(value))) read(pos,value);
        return value;
    }
};
} // namespace za::re::pokemon::config
