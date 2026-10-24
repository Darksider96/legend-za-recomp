#include "gfx/texture_codec.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace za::gfx {

namespace {

// ---- BC1 / BC3 / BC4 / BC5 (aritmetica inteira identica aos decodificadores de referencia) ----

void Decode565(uint16_t color, uint8_t out[4]) {
    const uint32_t r = (color >> 11) & 31, g = (color >> 5) & 63, b = color & 31;
    out[0] = static_cast<uint8_t>((r << 3) | (r >> 2));
    out[1] = static_cast<uint8_t>((g << 2) | (g >> 4));
    out[2] = static_cast<uint8_t>((b << 3) | (b >> 2));
    out[3] = 255;
}

// Bloco de cor BC1 em RGBA (stride 4 por pixel, 16 pixels); four_color forca o modo de 4 cores (BC3)
void DecodeBc1Color(const uint8_t* block, uint8_t out[64], bool four_color) {
    uint16_t c0 = 0, c1 = 0;
    uint32_t indices = 0;
    std::memcpy(&c0, block, 2);
    std::memcpy(&c1, block + 2, 2);
    std::memcpy(&indices, block + 4, 4);

    uint8_t palette[4][4];
    Decode565(c0, palette[0]);
    Decode565(c1, palette[1]);
    for (int c = 0; c < 3; ++c) {
        const int a = palette[0][c], b = palette[1][c];
        if (c0 > c1 || four_color) {
            palette[2][c] = static_cast<uint8_t>((2 * a + b) / 3);
            palette[3][c] = static_cast<uint8_t>((a + 2 * b) / 3);
        } else {
            palette[2][c] = static_cast<uint8_t>((a + b) / 2);
            palette[3][c] = 0;
        }
    }
    palette[2][3] = 255;
    palette[3][3] = (c0 > c1 || four_color) ? 255 : 0;

    for (int i = 0; i < 16; ++i) std::memcpy(out + 4 * i, palette[(indices >> (2 * i)) & 3], 4);
}

// Bloco de 8 bytes estilo BC4: escreve 16 valores em out[i * stride]
void DecodeBc4Channel(const uint8_t* block, uint8_t* out, size_t stride) {
    const int a0 = block[0], a1 = block[1];
    uint8_t values[8];
    values[0] = static_cast<uint8_t>(a0);
    values[1] = static_cast<uint8_t>(a1);
    if (a0 > a1) {
        for (int i = 1; i < 7; ++i) values[i + 1] = static_cast<uint8_t>(((7 - i) * a0 + i * a1) / 7);
    } else {
        for (int i = 1; i < 5; ++i) values[i + 1] = static_cast<uint8_t>(((5 - i) * a0 + i * a1) / 5);
        values[6] = 0;
        values[7] = 255;
    }
    for (int half = 0; half < 2; ++half) {
        const uint32_t bits = block[2 + 3 * half] | (uint32_t{block[3 + 3 * half]} << 8) | (uint32_t{block[4 + 3 * half]} << 16);
        for (int i = 0; i < 8; ++i) out[static_cast<size_t>(8 * half + i) * stride] = values[(bits >> (3 * i)) & 7];
    }
}

// ---- BC7 ----

struct Bc7Mode {
    uint8_t subsets, partition_bits, rotation_bits, selector_bits, color_bits, alpha_bits;
    uint8_t endpoint_pbits, shared_pbits, index_bits, index2_bits;
};

constexpr Bc7Mode kBc7Modes[8] = {
    {3, 4, 0, 0, 4, 0, 1, 0, 3, 0}, {2, 6, 0, 0, 6, 0, 0, 1, 3, 0}, {3, 6, 0, 0, 5, 0, 0, 0, 2, 0},
    {2, 6, 0, 0, 7, 0, 1, 0, 2, 0}, {1, 0, 2, 1, 5, 6, 0, 0, 2, 3}, {1, 0, 2, 0, 7, 8, 0, 0, 2, 2},
    {1, 0, 0, 0, 7, 7, 1, 0, 4, 0}, {2, 6, 0, 0, 5, 5, 1, 0, 2, 0},
};

// Bit do pixel i (0..15) = subconjunto 1
constexpr uint16_t kPartition2[64] = {
    0xCCCC, 0x8888, 0xEEEE, 0xECC8, 0xC880, 0xFEEC, 0xFEC8, 0xEC80, 0xC800, 0xFFEC, 0xFE80, 0xE800, 0xFFE8, 0xFF00, 0xFFF0, 0xF000,
    0xF710, 0x008E, 0x7100, 0x08CE, 0x008C, 0x7310, 0x3100, 0x8CCE, 0x088C, 0x3110, 0x6666, 0x366C, 0x17E8, 0x0FF0, 0x718E, 0x399C,
    0xAAAA, 0xF0F0, 0x5A5A, 0x33CC, 0x3C3C, 0x55AA, 0x9696, 0xA55A, 0x73CE, 0x13C8, 0x324C, 0x3BDC, 0x6996, 0xC33C, 0x9966, 0x0660,
    0x0272, 0x04E4, 0x4E40, 0x2720, 0xC936, 0x936C, 0x39C6, 0x639C, 0x9336, 0x9CC6, 0x817E, 0xE718, 0xCCF0, 0x0FCC, 0x7744, 0xEE22,
};

// 2 bits por pixel = subconjunto (0..2)
constexpr uint32_t kPartition3[64] = {
    0xAA685050, 0x6A5A5040, 0x5A5A4200, 0x5450A0A8, 0xA5A50000, 0xA0A05050, 0x5555A0A0, 0x5A5A5050,
    0xAA550000, 0xAA555500, 0xAAAA5500, 0x90909090, 0x94949494, 0xA4A4A4A4, 0xA9A59450, 0x2A0A4250,
    0xA5945040, 0x0A425054, 0xA5A5A500, 0x55A0A0A0, 0xA8A85454, 0x6A6A4040, 0xA4A45000, 0x1A1A0500,
    0x0050A4A4, 0xAAA59090, 0x14696914, 0x69691400, 0xA08585A0, 0xAA821414, 0x50A4A450, 0x6A5A0200,
    0xA9A58000, 0x5090A0A8, 0xA8A09050, 0x24242424, 0x00AA5500, 0x24924924, 0x24499224, 0x50A50A50,
    0x500AA550, 0xAAAA4444, 0x66660000, 0xA5A0A5A0, 0x50A050A0, 0x69286928, 0x44AAAA44, 0x66666600,
    0xAA444444, 0x54A854A8, 0x95809580, 0x96969600, 0xA85454A8, 0x80959580, 0xAA141414, 0x96960000,
    0xAAAA1414, 0xA05050A0, 0xA0A5A5A0, 0x96000000, 0x40804080, 0xA9A8A9A8, 0xAAAAAA44, 0x2A4A5254,
};

constexpr uint8_t kAnchor2[64] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 2, 8, 2, 2, 8, 8, 15, 2, 8, 2, 2, 8, 8, 2, 2,
    15, 15, 6, 8, 2, 8, 15, 15, 2, 8, 2, 2, 2, 15, 15, 6, 6, 2, 6, 8, 15, 15, 2, 2, 15, 15, 15, 15, 15, 2, 2, 15,
};
constexpr uint8_t kAnchor3a[64] = {
    3, 3, 15, 15, 8, 3, 15, 15, 8, 8, 6, 6, 6, 5, 3, 3, 3, 3, 8, 15, 3, 3, 6, 10, 5, 8, 8, 6, 8, 5, 15, 15,
    8, 15, 3, 5, 6, 10, 8, 15, 15, 3, 15, 5, 15, 15, 15, 15, 3, 15, 5, 5, 5, 8, 5, 10, 5, 10, 8, 13, 15, 12, 3, 3,
};
constexpr uint8_t kAnchor3b[64] = {
    15, 8, 8, 3, 15, 15, 3, 8, 15, 15, 15, 15, 15, 15, 15, 8, 15, 8, 15, 3, 15, 8, 15, 8, 3, 15, 6, 10, 15, 15, 10, 8,
    15, 3, 15, 10, 10, 8, 9, 10, 6, 15, 8, 15, 3, 6, 6, 8, 15, 3, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 3, 15, 15, 8,
};

constexpr uint8_t kWeights2[4] = {0, 21, 43, 64};
constexpr uint8_t kWeights3[8] = {0, 9, 18, 27, 37, 46, 55, 64};
constexpr uint8_t kWeights4[16] = {0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64};

class BitReader {
public:
    explicit BitReader(const uint8_t* data) : data_(data) {}
    uint32_t Read(uint32_t count) {
        uint32_t value = 0;
        for (uint32_t i = 0; i < count; ++i, ++position_) value |= ((data_[position_ >> 3] >> (position_ & 7)) & 1u) << i;
        return value;
    }

private:
    const uint8_t* data_;
    uint32_t position_{0};
};

uint8_t Expand(uint32_t value, uint32_t bits) {
    value <<= 8 - bits;
    return static_cast<uint8_t>(value | (value >> bits));
}

uint8_t Interpolate(uint8_t e0, uint8_t e1, uint32_t index, uint32_t bits) {
    const uint32_t weight = bits == 2 ? kWeights2[index] : bits == 3 ? kWeights3[index] : kWeights4[index];
    return static_cast<uint8_t>(((64 - weight) * e0 + weight * e1 + 32) >> 6);
}

void DecodeBc7Block(const uint8_t* block, uint8_t out[64]) {
    uint32_t mode_index = 0;
    while (mode_index < 8 && !((block[0] >> mode_index) & 1)) ++mode_index;
    if (mode_index == 8) {  // bloco reservado
        std::memset(out, 0, 64);
        return;
    }

    const Bc7Mode& mode = kBc7Modes[mode_index];
    BitReader bits(block);
    bits.Read(mode_index + 1);
    const uint32_t partition = bits.Read(mode.partition_bits);
    const uint32_t rotation = bits.Read(mode.rotation_bits);
    const uint32_t selector = bits.Read(mode.selector_bits);

    const uint32_t endpoints = 2u * mode.subsets;
    uint32_t color[6][4] = {};
    for (uint32_t c = 0; c < 3; ++c) {
        for (uint32_t e = 0; e < endpoints; ++e) color[e][c] = bits.Read(mode.color_bits);
    }
    for (uint32_t e = 0; e < endpoints; ++e) color[e][3] = mode.alpha_bits ? bits.Read(mode.alpha_bits) : 255;

    uint32_t pbits[6] = {};
    if (mode.endpoint_pbits) {
        for (uint32_t e = 0; e < endpoints; ++e) pbits[e] = bits.Read(1);
    } else if (mode.shared_pbits) {
        for (uint32_t s = 0; s < mode.subsets; ++s) pbits[2 * s] = pbits[2 * s + 1] = bits.Read(1);
    }
    const bool has_pbit = mode.endpoint_pbits || mode.shared_pbits;

    uint8_t endpoint[6][4];
    for (uint32_t e = 0; e < endpoints; ++e) {
        for (uint32_t c = 0; c < 4; ++c) {
            if (c == 3 && !mode.alpha_bits) {
                endpoint[e][c] = 255;
                continue;
            }
            const uint32_t width = c == 3 ? mode.alpha_bits : mode.color_bits;
            endpoint[e][c] = has_pbit ? Expand((color[e][c] << 1) | pbits[e], width + 1) : Expand(color[e][c], width);
        }
    }

    const auto subset_of = [&](uint32_t pixel) -> uint32_t {
        if (mode.subsets == 2) return (kPartition2[partition] >> pixel) & 1;
        if (mode.subsets == 3) return (kPartition3[partition] >> (2 * pixel)) & 3;
        return 0;
    };
    const auto is_anchor = [&](uint32_t pixel) {
        if (pixel == 0) return true;
        if (mode.subsets == 2) return pixel == kAnchor2[partition];
        if (mode.subsets == 3) return pixel == kAnchor3a[partition] || pixel == kAnchor3b[partition];
        return false;
    };

    uint32_t index1[16], index2[16] = {};
    for (uint32_t i = 0; i < 16; ++i) index1[i] = bits.Read(mode.index_bits - (is_anchor(i) ? 1 : 0));
    if (mode.index2_bits) {
        for (uint32_t i = 0; i < 16; ++i) index2[i] = bits.Read(mode.index2_bits - (i == 0 ? 1 : 0));
    }

    for (uint32_t i = 0; i < 16; ++i) {
        const uint32_t subset = subset_of(i);
        const uint8_t* e0 = endpoint[2 * subset];
        const uint8_t* e1 = endpoint[2 * subset + 1];
        uint8_t* pixel = out + 4 * i;
        if (mode.index2_bits) {
            // Modos 4/5: um conjunto de indices para a cor e outro para o alfa (trocados pelo seletor)
            const bool swap = selector != 0;
            const uint32_t color_index = swap ? index2[i] : index1[i];
            const uint32_t alpha_index = swap ? index1[i] : index2[i];
            const uint32_t color_bits = swap ? mode.index2_bits : mode.index_bits;
            const uint32_t alpha_bits = swap ? mode.index_bits : mode.index2_bits;
            for (int c = 0; c < 3; ++c) pixel[c] = Interpolate(e0[c], e1[c], color_index, color_bits);
            pixel[3] = Interpolate(e0[3], e1[3], alpha_index, alpha_bits);
        } else {
            for (int c = 0; c < 4; ++c) pixel[c] = Interpolate(e0[c], e1[c], index1[i], mode.index_bits);
        }
        if (rotation) std::swap(pixel[3], pixel[rotation - 1]);
    }
}

// ---- PNG ----

constexpr std::array<uint32_t, 256> MakeCrcTable() {
    std::array<uint32_t, 256> table{};
    for (uint32_t n = 0; n < 256; ++n) {
        uint32_t c = n;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        table[n] = c;
    }
    return table;
}
constexpr std::array<uint32_t, 256> kCrcTable = MakeCrcTable();

uint32_t Crc32(const uint8_t* data, size_t size, uint32_t crc = 0xFFFFFFFFu) {
    for (size_t i = 0; i < size; ++i) crc = kCrcTable[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}

void PutBigEndian(std::vector<uint8_t>& out, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<uint8_t>(value >> shift));
}

void PutChunk(std::vector<uint8_t>& out, const char type[4], const std::vector<uint8_t>& data) {
    PutBigEndian(out, static_cast<uint32_t>(data.size()));
    const size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    PutBigEndian(out, Crc32(out.data() + start, out.size() - start) ^ 0xFFFFFFFFu);
}

} // namespace

bool IsSrgb(const BntxFormat& format) {
    return (format.raw & 0xFF) == 0x06;
}

bool DecodeTexture(const BntxTexture& texture, std::span<const uint8_t> linear, std::vector<uint8_t>& out_rgba,
                   std::string& error) {
    const BntxFormat& format = texture.format;
    const uint32_t width = texture.width, height = texture.height;
    const uint32_t blocks_x = (width + format.block_width - 1) / format.block_width;
    const uint32_t blocks_y = (height + format.block_height - 1) / format.block_height;
    if (!format.Known() || linear.size() < size_t{blocks_x} * blocks_y * format.bytes_per_block) {
        error = "dados insuficientes para decodificar " + format.name;
        return false;
    }
    out_rgba.assign(size_t{width} * height * 4, 255);

    const uint32_t type = format.raw >> 8;
    if (!format.Compressed()) {
        const uint8_t* src = linear.data();
        for (size_t p = 0; p < size_t{width} * height; ++p) {
            uint8_t* dst = out_rgba.data() + 4 * p;
            switch (type) {
            case 0x02: dst[0] = dst[1] = dst[2] = src[p]; break;                                   // R8
            case 0x09: dst[0] = src[2 * p]; dst[1] = src[2 * p + 1]; dst[2] = 0; break;            // R8G8
            case 0x0B: std::memcpy(dst, src + 4 * p, 4); break;                                    // R8G8B8A8
            case 0x0C: dst[0] = src[4 * p + 2]; dst[1] = src[4 * p + 1]; dst[2] = src[4 * p]; dst[3] = src[4 * p + 3]; break;
            default:
                error = "decodificacao de " + format.name + " nao suportada";
                return false;
            }
        }
        return true;
    }

    uint8_t block[64];
    for (uint32_t by = 0; by < blocks_y; ++by) {
        for (uint32_t bx = 0; bx < blocks_x; ++bx) {
            const uint8_t* src = linear.data() + (size_t{by} * blocks_x + bx) * format.bytes_per_block;
            switch (type) {
            case 0x1A: DecodeBc1Color(src, block, false); break;
            case 0x1C:
                DecodeBc1Color(src + 8, block, true);
                DecodeBc4Channel(src, block + 3, 4);
                break;
            case 0x1D:
                DecodeBc4Channel(src, block, 4);
                for (int i = 0; i < 16; ++i) {
                    block[4 * i + 1] = block[4 * i + 2] = block[4 * i];
                    block[4 * i + 3] = 255;
                }
                break;
            case 0x1E:
                DecodeBc4Channel(src, block, 4);
                DecodeBc4Channel(src + 8, block + 1, 4);
                for (int i = 0; i < 16; ++i) {
                    block[4 * i + 2] = 0;
                    block[4 * i + 3] = 255;
                }
                break;
            case 0x20: DecodeBc7Block(src, block); break;
            default:
                error = "decodificacao de " + format.name + " nao suportada";
                return false;
            }
            for (uint32_t y = 0; y < 4; ++y) {
                const uint32_t py = by * 4 + y;
                if (py >= height) break;
                for (uint32_t x = 0; x < 4; ++x) {
                    const uint32_t px = bx * 4 + x;
                    if (px < width) std::memcpy(out_rgba.data() + (size_t{py} * width + px) * 4, block + 4 * (y * 4 + x), 4);
                }
            }
        }
    }
    return true;
}

bool DecodeBntxTexture(std::span<const uint8_t> file, const BntxTexture& texture, std::vector<uint8_t>& out_rgba,
                       std::string& error) {
    std::vector<uint8_t> linear;
    return DeswizzleTexture(file, texture, linear, error) && DecodeTexture(texture, linear, out_rgba, error);
}

bool EncodePng(uint32_t width, uint32_t height, uint32_t channels, std::span<const uint8_t> pixels,
               std::vector<uint8_t>& out, std::string& error) {
    if (width == 0 || height == 0 || (channels != 1 && channels != 3 && channels != 4) ||
        pixels.size() != size_t{width} * height * channels) {
        error = "dimensoes/canais invalidos para PNG";
        return false;
    }

    // Linhas com filtro 0 em blocos deflate "stored" (<= 65535 bytes), envoltos em zlib
    const size_t row = size_t{width} * channels;
    std::vector<uint8_t> raw;
    raw.reserve((row + 1) * height);
    for (uint32_t y = 0; y < height; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), pixels.begin() + static_cast<std::ptrdiff_t>(y * row), pixels.begin() + static_cast<std::ptrdiff_t>((y + 1) * row));
    }
    std::vector<uint8_t> zlib = {0x78, 0x01};
    for (size_t offset = 0; offset < raw.size() || offset == 0;) {
        const size_t count = std::min<size_t>(65535, raw.size() - offset);
        const bool last = offset + count == raw.size();
        zlib.push_back(last ? 1 : 0);
        zlib.push_back(static_cast<uint8_t>(count));
        zlib.push_back(static_cast<uint8_t>(count >> 8));
        zlib.push_back(static_cast<uint8_t>(~count));
        zlib.push_back(static_cast<uint8_t>(~count >> 8));
        zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset), raw.begin() + static_cast<std::ptrdiff_t>(offset + count));
        offset += count;
        if (last) break;
    }
    uint32_t a = 1, b = 0;  // Adler-32
    for (uint8_t value : raw) {
        a = (a + value) % 65521;
        b = (b + a) % 65521;
    }
    PutBigEndian(zlib, (b << 16) | a);

    std::vector<uint8_t> header;
    PutBigEndian(header, width);
    PutBigEndian(header, height);
    header.insert(header.end(), {8, static_cast<uint8_t>(channels == 1 ? 0 : channels == 3 ? 2 : 6), 0, 0, 0});

    out = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    PutChunk(out, "IHDR", header);
    PutChunk(out, "IDAT", zlib);
    PutChunk(out, "IEND", {});
    return true;
}

} // namespace za::gfx
