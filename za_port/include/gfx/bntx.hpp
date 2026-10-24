#pragma once
// BNTX (contêiner de texturas NX) — parser, layout block-linear da Tegra X1 e conversao para DDS/ASTC
//
// Cabecalho (0x00): "BNTX\0\0\0\0", u32 versao, u16 BOM (0xFEFF), u8 alinhamento, u8 tam. endereco,
//                   u32 offset do nome, u16 flags, u16 offset do 1o bloco, u32 offset _RLT, u32 tamanho
// NX (0x20):        "NX  ", u32 qtd texturas, u64 -> [u64] BRTI, u64 -> BRTD, u64 -> _DIC
// BRTI:             +0x11 u8 dim, +0x12 u16 tile mode, +0x16 u16 mips, +0x1C u32 formato,
//                   +0x24 u32 largura/altura/profundidade/camadas, +0x34 u32 layout (bits 0-2: block
//                   height log2, bits 4-6: block depth log2), +0x50 u32 image size, +0x58 u32 canais,
//                   +0x5C u32 surface dim, +0x60 u64 -> nome, +0x70 u64 -> [u64] offsets dos mips

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace za::gfx {

struct BntxFormat {
    uint32_t raw{0};             // (tipo << 8) | variante (01 UNORM, 02 SNORM, 05 FLOAT, 06 SRGB, 0A UFLOAT)
    std::string name{"UNKNOWN"};
    uint8_t block_width{1};
    uint8_t block_height{1};
    uint8_t bytes_per_block{0};  // 0 = formato desconhecido
    uint32_t dxgi{0};            // DXGI_FORMAT equivalente; 0 = sem DDS
    bool astc{false};

    bool Known() const { return bytes_per_block != 0; }
    bool Compressed() const { return block_width > 1; }
};

BntxFormat DescribeFormat(uint32_t raw);

enum class SurfaceDim : uint32_t {
    Dim1D = 0, Dim2D = 1, Dim3D = 2, Cube = 3, Array1D = 4, Array2D = 5,
    Multisample2D = 6, MultisampleArray2D = 7, CubeArray = 8,
};

const char* SurfaceDimName(SurfaceDim dim);

struct BntxTexture {
    std::string name;
    BntxFormat format;
    uint32_t width{0};
    uint32_t height{0};
    uint32_t depth{1};
    uint32_t array_length{1};
    uint16_t mip_count{1};
    uint16_t tile_mode{0};          // 0 = block linear, 1 = pitch linear
    uint32_t block_height_log2{0};  // GOBs por bloco em Y
    uint32_t block_depth_log2{0};   // GOBs por bloco em Z
    SurfaceDim surface_dim{SurfaceDim::Dim2D};
    uint32_t image_size{0};
    uint32_t alignment{0};
    uint32_t channel_swizzle{0};    // byte por canal R,G,B,A: 0 = zero, 1 = um, 2..5 = R,G,B,A
    std::vector<uint64_t> mip_offsets;  // offsets absolutos no arquivo

    bool IsCube() const { return surface_dim == SurfaceDim::Cube || surface_dim == SurfaceDim::CubeArray; }
    bool Is3D() const { return surface_dim == SurfaceDim::Dim3D; }
};

struct BntxFile {
    uint32_t version{0};
    std::string name;
    std::vector<BntxTexture> textures;
};

bool IsBntx(std::span<const uint8_t> data);
bool ParseBntx(std::span<const uint8_t> data, BntxFile& out, std::string& error);

struct MipLayout {
    uint32_t width{0};
    uint32_t height{0};
    uint32_t depth{1};
    uint64_t offset{0};          // relativo ao inicio da camada
    uint64_t size{0};            // bytes com o padding do layout
    uint32_t pitch{0};           // apenas pitch linear
    uint32_t gob_height_log2{0}; // block height efetivo do mip
    uint32_t gob_depth_log2{0};
};

struct SurfaceLayout {
    std::vector<MipLayout> mips;
    uint64_t layer_size{0};      // soma dos mips de uma camada
    uint64_t layer_stride{0};    // distancia entre camadas (alinhada ao bloco de GOBs)
    uint64_t total_size{0};      // deve ser igual a BntxTexture::image_size
};

// Tamanhos e offsets de cada mip segundo as regras de layout da Tegra X1
SurfaceLayout ComputeLayout(const BntxTexture& texture);

// Desfaz o swizzle; saida linear na ordem DDS (camada -> mip -> fatia)
bool DeswizzleTexture(std::span<const uint8_t> file, const BntxTexture& texture,
                      std::vector<uint8_t>& out, std::string& error);

// Arquivo DDS (cabecalho DX10) a partir dos dados lineares
bool BuildDds(const BntxTexture& texture, std::span<const uint8_t> linear,
              std::vector<uint8_t>& out, std::string& error);

// Arquivo .astc (mip 0 da camada 0) a partir dos dados lineares
bool BuildAstc(const BntxTexture& texture, std::span<const uint8_t> linear,
               std::vector<uint8_t>& out, std::string& error);

// Deswizzle + DDS (BC/RGBA) ou .astc (ASTC); extension recebe ".dds" ou ".astc"
bool ConvertTexture(std::span<const uint8_t> file, const BntxTexture& texture,
                    std::vector<uint8_t>& out, std::string& extension, std::string& error);

} // namespace za::gfx
