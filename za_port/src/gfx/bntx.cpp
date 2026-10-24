#include "gfx/bntx.hpp"

#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstring>

namespace za::gfx {

static_assert(std::endian::native == std::endian::little, "Leitor BNTX assume host little-endian");

namespace {

constexpr char kBntxMagic[8] = {'B', 'N', 'T', 'X', '\0', '\0', '\0', '\0'};
constexpr uint64_t kBrtiSize = 0xA0;
constexpr uint32_t kGobShift = 9;  // GOB = 64 bytes x 8 linhas = 512 bytes

template <typename T>
bool Read(std::span<const uint8_t> data, uint64_t offset, T& out) {
    if (offset > data.size() || sizeof(T) > data.size() - offset) return false;
    std::memcpy(&out, data.data() + offset, sizeof(T));
    return true;
}

constexpr uint64_t DivUp(uint64_t value, uint64_t divisor) { return (value + divisor - 1) / divisor; }
constexpr uint64_t AlignUp(uint64_t value, uint64_t alignment) { return DivUp(value, alignment) * alignment; }

struct TypeRow {
    uint8_t type;
    const char* name;
    uint8_t block_width;
    uint8_t block_height;
    uint8_t bytes_per_block;
};

// Tipos (byte alto do formato BNTX/NVN). 0x14, 0x15, 0x19 e os BCn foram conferidos contra os image_size do jogo.
constexpr TypeRow kTypes[] = {
    {0x02, "R8", 1, 1, 1},           {0x09, "R8G8", 1, 1, 2},         {0x0A, "R16", 1, 1, 2},
    {0x0B, "R8G8B8A8", 1, 1, 4},     {0x0C, "B8G8R8A8", 1, 1, 4},     {0x0E, "R10G10B10A2", 1, 1, 4},
    {0x0F, "R11G11B10", 1, 1, 4},    {0x14, "R32", 1, 1, 4},          {0x15, "R16G16B16A16", 1, 1, 8},
    {0x19, "R32G32B32A32", 1, 1, 16},
    {0x1A, "BC1", 4, 4, 8},          {0x1B, "BC2", 4, 4, 16},         {0x1C, "BC3", 4, 4, 16},
    {0x1D, "BC4", 4, 4, 8},          {0x1E, "BC5", 4, 4, 16},         {0x1F, "BC6H", 4, 4, 16},
    {0x20, "BC7", 4, 4, 16},
    {0x2D, "ASTC_4x4", 4, 4, 16},    {0x2E, "ASTC_5x4", 5, 4, 16},    {0x2F, "ASTC_5x5", 5, 5, 16},
    {0x30, "ASTC_6x5", 6, 5, 16},    {0x31, "ASTC_6x6", 6, 6, 16},    {0x32, "ASTC_8x5", 8, 5, 16},
    {0x33, "ASTC_8x6", 8, 6, 16},    {0x34, "ASTC_8x8", 8, 8, 16},    {0x35, "ASTC_10x5", 10, 5, 16},
    {0x36, "ASTC_10x6", 10, 6, 16},  {0x37, "ASTC_10x8", 10, 8, 16},  {0x38, "ASTC_10x10", 10, 10, 16},
    {0x39, "ASTC_12x10", 12, 10, 16}, {0x3A, "ASTC_12x12", 12, 12, 16},
};

struct DxgiRow {
    uint32_t raw;
    uint32_t dxgi;
};

constexpr DxgiRow kDxgi[] = {
    {0x0201, 61}, {0x0202, 63}, {0x0203, 62}, {0x0204, 64},
    {0x0901, 49}, {0x0902, 51}, {0x0903, 50}, {0x0904, 52},
    {0x0A01, 56}, {0x0A02, 58}, {0x0A03, 57}, {0x0A04, 59}, {0x0A05, 54},
    {0x0B01, 28}, {0x0B02, 31}, {0x0B03, 30}, {0x0B04, 32}, {0x0B06, 29},
    {0x0C01, 87}, {0x0C06, 91},
    {0x0E01, 24}, {0x0E03, 25},
    {0x0F05, 26},
    {0x1403, 42}, {0x1404, 43}, {0x1405, 41},
    {0x1501, 11}, {0x1502, 13}, {0x1503, 12}, {0x1504, 14}, {0x1505, 10},
    {0x1903, 3}, {0x1904, 4}, {0x1905, 2},
    {0x1A01, 71}, {0x1A06, 72}, {0x1B01, 74}, {0x1B06, 75}, {0x1C01, 77}, {0x1C06, 78},
    {0x1D01, 80}, {0x1D02, 81}, {0x1E01, 83}, {0x1E02, 84},
    {0x1F0A, 95}, {0x1F05, 96},
    {0x2001, 98}, {0x2006, 99},
};

const char* VariantName(uint32_t type, uint32_t variant) {
    if (type == 0x1F) return variant == 0x0A ? "UF16" : variant == 0x05 ? "SF16" : nullptr;
    switch (variant) {
    case 0x01: return "UNORM";
    case 0x02: return "SNORM";
    case 0x03: return "UINT";
    case 0x04: return "SINT";
    case 0x05: return "FLOAT";
    case 0x06: return "SRGB";
    case 0x0A: return "UFLOAT";
    default:   return nullptr;
    }
}

bool ReadLengthPrefixedString(std::span<const uint8_t> data, uint64_t offset, std::string& out) {
    uint16_t length = 0;
    if (!Read(data, offset, length) || offset + 2 + length > data.size()) return false;
    out.assign(reinterpret_cast<const char*>(data.data() + offset + 2), length);
    return true;
}

// Reduz o block height/depth de um mip enquanto ele couber na metade do bloco (regra do hardware)
void FitGobBlock(uint64_t height_in_blocks, uint64_t depth, uint32_t& gob_height_log2, uint32_t& gob_depth_log2) {
    while (gob_height_log2 > 0 && height_in_blocks <= (uint64_t{1} << (gob_height_log2 - 1)) * 8) --gob_height_log2;
    while (gob_depth_log2 > 0 && depth <= (uint64_t{1} << (gob_depth_log2 - 1))) --gob_depth_log2;
}

// Copia um mip block-linear para linear. Dentro do GOB cada trecho de 16 bytes alinhados e contiguo.
void DeswizzleBlockLinear(const uint8_t* src, uint64_t src_size, uint8_t* dst,
                          uint32_t width_blocks, uint32_t height_blocks, uint32_t depth, uint32_t bytes_per_block,
                          uint32_t gob_height_log2, uint32_t gob_depth_log2) {
    const uint64_t row_bytes = uint64_t{width_blocks} * bytes_per_block;
    const uint32_t x_shift = kGobShift + gob_height_log2 + gob_depth_log2;
    const uint64_t block_row_size = DivUp(row_bytes, 64) << x_shift;
    const uint64_t slice_size = DivUp(height_blocks, uint64_t{8} << gob_height_log2) * block_row_size;
    const uint32_t gob_y_mask = (1u << gob_height_log2) - 1;
    const uint32_t gob_z_mask = (1u << gob_depth_log2) - 1;

    for (uint32_t z = 0; z < depth; ++z) {
        const uint64_t offset_z = (z >> gob_depth_log2) * slice_size +
                                  (uint64_t{z & gob_z_mask} << (kGobShift + gob_height_log2));
        for (uint32_t y = 0; y < height_blocks; ++y) {
            const uint32_t gob_y = y >> 3;
            const uint64_t offset_y = (gob_y >> gob_height_log2) * block_row_size +
                                      (uint64_t{gob_y & gob_y_mask} << kGobShift);
            const uint64_t in_gob_y = ((y & 7u) >> 1) * 64 + (y & 1u) * 16;
            uint8_t* dst_row = dst + (uint64_t{z} * height_blocks + y) * row_bytes;

            for (uint64_t x = 0; x < row_bytes; x += 16) {
                const uint64_t offset_x = (x >> 6) << x_shift;
                const uint64_t in_gob = ((x & 63) >> 5) * 256 + ((x & 31) >> 4) * 32 + in_gob_y;
                const uint64_t pos = offset_z + offset_y + offset_x + in_gob;
                const size_t count = static_cast<size_t>(std::min<uint64_t>(16, row_bytes - x));
                if (pos <= src_size && count <= src_size - pos) {
                    std::memcpy(dst_row + x, src + pos, count);
                } else {
                    std::memset(dst_row + x, 0, count);
                }
            }
        }
    }
}

uint64_t LinearMipSize(const BntxTexture& texture, const MipLayout& mip) {
    return DivUp(mip.width, texture.format.block_width) * DivUp(mip.height, texture.format.block_height) *
           mip.depth * texture.format.bytes_per_block;
}

std::string HexFormat(uint32_t raw) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "0x%04x", raw);
    return buffer;
}

void PutU32(std::vector<uint8_t>& out, uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) out.push_back(static_cast<uint8_t>(value >> shift));
}

void PutU24(std::vector<uint8_t>& out, uint32_t value) {
    for (int shift = 0; shift < 24; shift += 8) out.push_back(static_cast<uint8_t>(value >> shift));
}

} // namespace

BntxFormat DescribeFormat(uint32_t raw) {
    BntxFormat format;
    format.raw = raw;

    const uint32_t type = (raw >> 8) & 0xFF;
    const char* variant = VariantName(type, raw & 0xFF);
    const auto row = std::find_if(std::begin(kTypes), std::end(kTypes), [type](const TypeRow& r) { return r.type == type; });
    if (row == std::end(kTypes) || !variant) return format;

    format.name = std::string(row->name) + "_" + variant;
    format.block_width = row->block_width;
    format.block_height = row->block_height;
    format.bytes_per_block = row->bytes_per_block;
    format.astc = type >= 0x2D && type <= 0x3A;
    for (const DxgiRow& dxgi : kDxgi) {
        if (dxgi.raw == raw) format.dxgi = dxgi.dxgi;
    }
    return format;
}

const char* SurfaceDimName(SurfaceDim dim) {
    switch (dim) {
    case SurfaceDim::Dim1D:              return "1D";
    case SurfaceDim::Dim2D:              return "2D";
    case SurfaceDim::Dim3D:              return "3D";
    case SurfaceDim::Cube:               return "Cube";
    case SurfaceDim::Array1D:            return "1D array";
    case SurfaceDim::Array2D:            return "2D array";
    case SurfaceDim::Multisample2D:      return "2D MSAA";
    case SurfaceDim::MultisampleArray2D: return "2D MSAA array";
    case SurfaceDim::CubeArray:          return "Cube array";
    }
    return "?";
}

bool IsBntx(std::span<const uint8_t> data) {
    return data.size() >= sizeof(kBntxMagic) && std::memcmp(data.data(), kBntxMagic, 4) == 0;
}

bool ParseBntx(std::span<const uint8_t> data, BntxFile& out, std::string& error) {
    out = {};
    if (!IsBntx(data) || data.size() < 0x40 || std::memcmp(data.data(), kBntxMagic, sizeof(kBntxMagic)) != 0) {
        error = "assinatura BNTX ausente";
        return false;
    }

    uint16_t bom = 0;
    Read(data, 0x0C, bom);
    if (bom != 0xFEFF) {
        error = "apenas BNTX little-endian e suportado";
        return false;
    }
    Read(data, 0x08, out.version);

    uint32_t name_offset = 0;
    if (Read(data, 0x10, name_offset) && name_offset >= 2) ReadLengthPrefixedString(data, name_offset - 2, out.name);

    uint32_t count = 0;
    uint64_t info_array = 0;
    if (std::memcmp(data.data() + 0x20, "NX  ", 4) != 0 || !Read(data, 0x24, count) || !Read(data, 0x28, info_array) ||
        count > 4096 || info_array > data.size() || uint64_t{count} * 8 > data.size() - info_array) {
        error = "bloco NX invalido";
        return false;
    }

    out.textures.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t brti = 0;
        Read(data, info_array + 8 * uint64_t{i}, brti);
        if (brti > data.size() || kBrtiSize > data.size() - brti || std::memcmp(data.data() + brti, "BRTI", 4) != 0) {
            error = "BRTI #" + std::to_string(i) + " invalido";
            return false;
        }

        BntxTexture texture;
        uint32_t format_raw = 0, layout = 0, surface_dim = 0;
        uint64_t texture_name = 0, mip_table = 0;
        Read(data, brti + 0x12, texture.tile_mode);
        Read(data, brti + 0x16, texture.mip_count);
        Read(data, brti + 0x1C, format_raw);
        Read(data, brti + 0x24, texture.width);
        Read(data, brti + 0x28, texture.height);
        Read(data, brti + 0x2C, texture.depth);
        Read(data, brti + 0x30, texture.array_length);
        Read(data, brti + 0x34, layout);
        Read(data, brti + 0x50, texture.image_size);
        Read(data, brti + 0x54, texture.alignment);
        Read(data, brti + 0x58, texture.channel_swizzle);
        Read(data, brti + 0x5C, surface_dim);
        Read(data, brti + 0x60, texture_name);
        Read(data, brti + 0x70, mip_table);

        texture.format = DescribeFormat(format_raw);
        texture.block_height_log2 = layout & 7;
        texture.block_depth_log2 = (layout >> 4) & 7;
        texture.surface_dim = static_cast<SurfaceDim>(surface_dim);
        texture.depth = std::max(texture.depth, 1u);
        texture.array_length = std::max(texture.array_length, 1u);

        if (texture.width == 0 || texture.height == 0 || texture.mip_count == 0 || texture.mip_count > 17 ||
            surface_dim > static_cast<uint32_t>(SurfaceDim::CubeArray) || texture.tile_mode > 1 ||
            mip_table > data.size() || uint64_t{texture.mip_count} * 8 > data.size() - mip_table) {
            error = "BRTI #" + std::to_string(i) + " com campos invalidos";
            return false;
        }
        if (!ReadLengthPrefixedString(data, texture_name, texture.name)) texture.name = "texture_" + std::to_string(i);

        texture.mip_offsets.resize(texture.mip_count);
        for (uint16_t level = 0; level < texture.mip_count; ++level) {
            Read(data, mip_table + 8 * uint64_t{level}, texture.mip_offsets[level]);
        }
        out.textures.push_back(std::move(texture));
    }
    return true;
}

SurfaceLayout ComputeLayout(const BntxTexture& texture) {
    SurfaceLayout layout;
    const BntxFormat& format = texture.format;
    if (!format.Known()) return layout;

    const uint32_t depth = texture.Is3D() ? texture.depth : 1;
    uint64_t offset = 0;
    for (uint32_t level = 0; level < texture.mip_count; ++level) {
        MipLayout mip;
        mip.width = std::max(1u, texture.width >> level);
        mip.height = std::max(1u, texture.height >> level);
        mip.depth = std::max(1u, depth >> level);
        const uint64_t width_blocks = DivUp(mip.width, format.block_width);
        const uint64_t height_blocks = DivUp(mip.height, format.block_height);
        const uint64_t row_bytes = width_blocks * format.bytes_per_block;

        if (texture.tile_mode == 1) {
            mip.pitch = static_cast<uint32_t>(AlignUp(row_bytes, 32));
            mip.size = uint64_t{mip.pitch} * height_blocks * mip.depth;
        } else {
            mip.gob_height_log2 = texture.block_height_log2;
            mip.gob_depth_log2 = texture.block_depth_log2;
            FitGobBlock(height_blocks, mip.depth, mip.gob_height_log2, mip.gob_depth_log2);
            const uint64_t blocks_y = DivUp(DivUp(height_blocks, 8), uint64_t{1} << mip.gob_height_log2);
            const uint64_t blocks_z = DivUp(mip.depth, uint64_t{1} << mip.gob_depth_log2);
            mip.size = (blocks_z * blocks_y * DivUp(row_bytes, 64)) << (kGobShift + mip.gob_height_log2 + mip.gob_depth_log2);
        }

        mip.offset = offset;
        offset += mip.size;
        layout.mips.push_back(mip);
    }

    layout.layer_size = offset;
    layout.layer_stride = offset;
    if (texture.tile_mode == 0) {
        // Camadas alinhadas ao bloco de GOBs do mip 0
        uint32_t gob_height_log2 = texture.block_height_log2;
        uint32_t gob_depth_log2 = texture.block_depth_log2;
        FitGobBlock(DivUp(texture.height, format.block_height), depth, gob_height_log2, gob_depth_log2);
        layout.layer_stride = AlignUp(offset, uint64_t{1} << (kGobShift + gob_height_log2 + gob_depth_log2));
    }
    // image_size guarda a ultima camada sem alinhamento apenas quando ha uma unica camada
    layout.total_size = texture.array_length > 1 ? layout.layer_stride * texture.array_length : layout.layer_size;
    return layout;
}

bool DeswizzleTexture(std::span<const uint8_t> file, const BntxTexture& texture,
                      std::vector<uint8_t>& out, std::string& error) {
    out.clear();
    if (!texture.format.Known()) {
        error = "formato desconhecido " + HexFormat(texture.format.raw);
        return false;
    }

    const SurfaceLayout layout = ComputeLayout(texture);
    if (layout.total_size != texture.image_size) {
        error = "layout calculado (" + std::to_string(layout.total_size) + " bytes) difere do image_size (" +
                std::to_string(texture.image_size) + " bytes)";
        return false;
    }
    const uint64_t base = texture.mip_offsets.front();
    for (size_t level = 0; level < layout.mips.size(); ++level) {
        if (texture.mip_offsets[level] - base != layout.mips[level].offset) {
            error = "offset do mip " + std::to_string(level) + " difere do layout calculado";
            return false;
        }
    }
    if (base > file.size() || layout.total_size > file.size() - base) {
        error = "dados da textura ultrapassam o arquivo";
        return false;
    }

    uint64_t linear_size = 0;
    for (const MipLayout& mip : layout.mips) linear_size += LinearMipSize(texture, mip);
    out.reserve(static_cast<size_t>(linear_size * texture.array_length));

    const BntxFormat& format = texture.format;
    for (uint32_t layer = 0; layer < texture.array_length; ++layer) {
        for (const MipLayout& mip : layout.mips) {
            const uint8_t* src = file.data() + base + layer * layout.layer_stride + mip.offset;
            const uint64_t src_size = std::min<uint64_t>(mip.size, file.size() - (base + layer * layout.layer_stride + mip.offset));
            const auto width_blocks = static_cast<uint32_t>(DivUp(mip.width, format.block_width));
            const auto height_blocks = static_cast<uint32_t>(DivUp(mip.height, format.block_height));
            const size_t dst_offset = out.size();
            out.resize(dst_offset + static_cast<size_t>(LinearMipSize(texture, mip)));

            if (texture.tile_mode == 1) {
                const uint64_t row_bytes = uint64_t{width_blocks} * format.bytes_per_block;
                for (uint64_t row = 0; row < uint64_t{height_blocks} * mip.depth; ++row) {
                    std::memcpy(out.data() + dst_offset + row * row_bytes, src + row * mip.pitch, static_cast<size_t>(row_bytes));
                }
            } else {
                DeswizzleBlockLinear(src, src_size, out.data() + dst_offset, width_blocks, height_blocks, mip.depth,
                                     format.bytes_per_block, mip.gob_height_log2, mip.gob_depth_log2);
            }
        }
    }
    return true;
}

bool BuildDds(const BntxTexture& texture, std::span<const uint8_t> linear,
              std::vector<uint8_t>& out, std::string& error) {
    out.clear();
    const BntxFormat& format = texture.format;
    if (format.dxgi == 0) {
        error = "formato " + format.name + " sem equivalente DXGI";
        return false;
    }
    if (texture.IsCube() && texture.array_length % 6 != 0) {
        error = "cubemap com quantidade de faces invalida";
        return false;
    }

    const SurfaceLayout layout = ComputeLayout(texture);
    uint64_t expected = 0;
    for (const MipLayout& mip : layout.mips) expected += LinearMipSize(texture, mip);
    if (expected * texture.array_length != linear.size()) {
        error = "tamanho dos dados lineares nao confere com a textura";
        return false;
    }

    constexpr uint32_t kCaps = 0x1, kHeight = 0x2, kWidth = 0x4, kPitch = 0x8, kPixelFormat = 0x1000;
    constexpr uint32_t kMipMapCount = 0x20000, kLinearSize = 0x80000, kDepth = 0x800000;
    constexpr uint32_t kCapsComplex = 0x8, kCapsTexture = 0x1000, kCapsMipmap = 0x400000;
    constexpr uint32_t kCaps2CubeAllFaces = 0xFE00, kCaps2Volume = 0x200000;

    const bool volume = texture.Is3D();
    const uint64_t top_size = LinearMipSize(texture, layout.mips.front()) / layout.mips.front().depth;
    uint32_t flags = kCaps | kHeight | kWidth | kPixelFormat | (format.Compressed() ? kLinearSize : kPitch);
    if (texture.mip_count > 1) flags |= kMipMapCount;
    if (volume) flags |= kDepth;
    uint32_t caps = kCapsTexture;
    if (texture.mip_count > 1) caps |= kCapsMipmap | kCapsComplex;
    if (texture.IsCube() || volume || texture.array_length > 1) caps |= kCapsComplex;

    out.reserve(148 + linear.size());
    out.insert(out.end(), {'D', 'D', 'S', ' '});
    PutU32(out, 124);
    PutU32(out, flags);
    PutU32(out, texture.height);
    PutU32(out, texture.width);
    PutU32(out, format.Compressed() ? static_cast<uint32_t>(top_size)
                                    : static_cast<uint32_t>(uint64_t{texture.width} * format.bytes_per_block));
    PutU32(out, volume ? texture.depth : 0);
    PutU32(out, texture.mip_count);
    for (int i = 0; i < 11; ++i) PutU32(out, 0);
    // DDS_PIXELFORMAT com FourCC "DX10"
    PutU32(out, 32);
    PutU32(out, 0x4);
    out.insert(out.end(), {'D', 'X', '1', '0'});
    for (int i = 0; i < 5; ++i) PutU32(out, 0);
    PutU32(out, caps);
    PutU32(out, texture.IsCube() ? kCaps2CubeAllFaces : volume ? kCaps2Volume : 0);
    for (int i = 0; i < 3; ++i) PutU32(out, 0);
    // DDS_HEADER_DXT10
    PutU32(out, format.dxgi);
    PutU32(out, volume ? 4 : 3);  // D3D10_RESOURCE_DIMENSION_TEXTURE3D / TEXTURE2D
    PutU32(out, texture.IsCube() ? 0x4 : 0);
    PutU32(out, texture.IsCube() ? texture.array_length / 6 : volume ? 1 : texture.array_length);
    PutU32(out, 0);

    out.insert(out.end(), linear.begin(), linear.end());
    return true;
}

bool BuildAstc(const BntxTexture& texture, std::span<const uint8_t> linear,
               std::vector<uint8_t>& out, std::string& error) {
    out.clear();
    const BntxFormat& format = texture.format;
    if (!format.astc) {
        error = "formato " + format.name + " nao e ASTC";
        return false;
    }
    const uint64_t mip0 = DivUp(texture.width, format.block_width) * DivUp(texture.height, format.block_height) * 16;
    if (linear.size() < mip0) {
        error = "dados ASTC insuficientes";
        return false;
    }

    out.reserve(16 + static_cast<size_t>(mip0));
    out.insert(out.end(), {0x13, 0xAB, 0xA1, 0x5C, format.block_width, format.block_height, 1});
    PutU24(out, texture.width);
    PutU24(out, texture.height);
    PutU24(out, 1);
    out.insert(out.end(), linear.begin(), linear.begin() + static_cast<std::ptrdiff_t>(mip0));
    return true;
}

bool ConvertTexture(std::span<const uint8_t> file, const BntxTexture& texture,
                    std::vector<uint8_t>& out, std::string& extension, std::string& error) {
    if (!texture.format.Known()) {
        error = "formato desconhecido " + HexFormat(texture.format.raw);
        return false;
    }
    if (!texture.format.astc && texture.format.dxgi == 0) {
        error = "formato " + texture.format.name + " sem equivalente DXGI";
        return false;
    }

    std::vector<uint8_t> linear;
    if (!DeswizzleTexture(file, texture, linear, error)) return false;
    extension = texture.format.astc ? ".astc" : ".dds";
    return texture.format.astc ? BuildAstc(texture, linear, out, error) : BuildDds(texture, linear, out, error);
}

} // namespace za::gfx
