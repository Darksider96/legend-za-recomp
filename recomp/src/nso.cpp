#include "recomp/nso.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <unordered_map>

namespace recomp {

namespace {

template <typename T>
bool ReadAt(std::span<const uint8_t> data, uint64_t offset, T& out) {
    if (offset > data.size() || sizeof(T) > data.size() - offset) return false;
    std::memcpy(&out, data.data() + offset, sizeof(T));
    return true;
}

// DWARF EH: inteiros LEB128 e ponteiros codificados (DW_EH_PE_*)
bool ReadUleb(std::span<const uint8_t> data, uint64_t& pos, uint64_t& out) {
    out = 0;
    for (unsigned shift = 0; pos < data.size() && shift < 64; shift += 7) {
        const uint8_t b = data[pos++];
        out |= uint64_t{b & 0x7Fu} << shift;
        if (!(b & 0x80)) return true;
    }
    return false;
}

bool ReadSleb(std::span<const uint8_t> data, uint64_t& pos, int64_t& out) {
    uint64_t value = 0;
    unsigned shift = 0;
    uint8_t b = 0;
    do {
        if (pos >= data.size() || shift >= 64) return false;
        b = data[pos++];
        value |= uint64_t{b & 0x7Fu} << shift;
        shift += 7;
    } while (b & 0x80);
    if (shift < 64 && (b & 0x40)) value |= ~uint64_t{0} << shift;
    out = static_cast<int64_t>(value);
    return true;
}

bool ReadEncoded(std::span<const uint8_t> data, uint64_t& pos, uint8_t encoding, uint64_t data_base, uint64_t& out) {
    if (encoding == 0xFF) return false;  // DW_EH_PE_omit
    const uint64_t field = pos;
    uint64_t value = 0;
    switch (encoding & 0x0F) {
    case 0x00: { uint64_t v; if (!ReadAt(data, pos, v)) return false; value = v; pos += 8; break; }
    case 0x01: if (!ReadUleb(data, pos, value)) return false; break;
    case 0x02: { uint16_t v; if (!ReadAt(data, pos, v)) return false; value = v; pos += 2; break; }
    case 0x03: { uint32_t v; if (!ReadAt(data, pos, v)) return false; value = v; pos += 4; break; }
    case 0x04: { uint64_t v; if (!ReadAt(data, pos, v)) return false; value = v; pos += 8; break; }
    case 0x09: { int64_t v; if (!ReadSleb(data, pos, v)) return false; value = static_cast<uint64_t>(v); break; }
    case 0x0A: { int16_t v; if (!ReadAt(data, pos, v)) return false; value = static_cast<uint64_t>(int64_t{v}); pos += 2; break; }
    case 0x0B: { int32_t v; if (!ReadAt(data, pos, v)) return false; value = static_cast<uint64_t>(int64_t{v}); pos += 4; break; }
    case 0x0C: { int64_t v; if (!ReadAt(data, pos, v)) return false; value = static_cast<uint64_t>(v); pos += 8; break; }
    default: return false;
    }
    switch (encoding & 0x70) {
    case 0x00: break;
    case 0x10: value += field; break;      // pcrel
    case 0x30: value += data_base; break;  // datarel
    default: return false;
    }
    out = value;
    return true;
}

constexpr uint32_t kSha256K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

uint32_t Rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }

void Sha256Block(uint32_t state[8], const uint8_t* block) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (uint32_t{block[4 * i]} << 24) | (uint32_t{block[4 * i + 1]} << 16) | (uint32_t{block[4 * i + 2]} << 8) | block[4 * i + 3];
    }
    for (int i = 16; i < 64; ++i) {
        const uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4], f = state[5], g = state[6], h = state[7];
    for (int i = 0; i < 64; ++i) {
        const uint32_t t1 = h + (Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25)) + ((e & f) ^ (~e & g)) + kSha256K[i] + w[i];
        const uint32_t t2 = (Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g, g = f, f = e, e = d + t1, d = c, c = b, b = a, a = t1 + t2;
    }
    state[0] += a, state[1] += b, state[2] += c, state[3] += d, state[4] += e, state[5] += f, state[6] += g, state[7] += h;
}

} // namespace

std::array<uint8_t, 32> Sha256(std::span<const uint8_t> data) {
    uint32_t state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    size_t full = data.size() / 64;
    for (size_t i = 0; i < full; ++i) Sha256Block(state, data.data() + 64 * i);
    uint8_t tail[128] = {};
    const size_t rest = data.size() - 64 * full;
    if (rest) std::memcpy(tail, data.data() + 64 * full, rest);
    tail[rest] = 0x80;
    const size_t tail_size = rest + 9 <= 64 ? 64 : 128;
    const uint64_t bits = uint64_t{data.size()} * 8;
    for (int i = 0; i < 8; ++i) tail[tail_size - 1 - i] = static_cast<uint8_t>(bits >> (8 * i));
    for (size_t i = 0; i < tail_size; i += 64) Sha256Block(state, tail + i);
    std::array<uint8_t, 32> out{};
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 4; ++j) out[4 * i + j] = static_cast<uint8_t>(state[i] >> (24 - 8 * j));
    }
    return out;
}

bool Lz4DecompressBlock(std::span<const uint8_t> src, std::span<uint8_t> dst) {
    size_t in = 0, out = 0;
    while (in < src.size()) {
        const uint8_t token = src[in++];
        size_t literals = token >> 4;
        if (literals == 15) {
            for (uint8_t b = 255; b == 255;) {
                if (in >= src.size()) return false;
                b = src[in++];
                literals += b;
            }
        }
        if (literals > src.size() - in || literals > dst.size() - out) return false;
        std::memcpy(dst.data() + out, src.data() + in, literals);
        in += literals;
        out += literals;
        if (in == src.size()) break;  // ultima sequencia: so literais
        if (src.size() - in < 2) return false;
        const size_t offset = src[in] | (src[in + 1] << 8);
        in += 2;
        size_t match = (token & 0x0F) + 4;
        if ((token & 0x0F) == 15) {
            for (uint8_t b = 255; b == 255;) {
                if (in >= src.size()) return false;
                b = src[in++];
                match += b;
            }
        }
        if (offset == 0 || offset > out || match > dst.size() - out) return false;
        for (size_t i = 0; i < match; ++i, ++out) dst[out] = dst[out - offset];  // pode se sobrepor
    }
    return out == dst.size();
}

bool NsoModule::Load(const std::string& path, std::string& error) {
    path_ = path;
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "nao foi possivel abrir " + path;
        return false;
    }
    const std::vector<uint8_t> file((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    const std::span<const uint8_t> bytes(file);
    uint32_t magic = 0, flags = 0;
    if (!ReadAt(bytes, 0, magic) || magic != 0x304F534E || !ReadAt(bytes, 0xC, flags)) {  // "NSO0"
        error = path + ": nao e NSO0";
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        Segment& s = segments_[i];
        const uint64_t header = 0x10 + 0x10 * static_cast<uint64_t>(i);
        ReadAt(bytes, header, s.file_offset);
        ReadAt(bytes, header + 4, s.memory_offset);
        ReadAt(bytes, header + 8, s.size);
        ReadAt(bytes, 0x60 + 4 * static_cast<uint64_t>(i), s.file_size);
        s.compressed = (flags >> i) & 1;
        s.hash_checked = (flags >> (3 + i)) & 1;
        std::memcpy(s.sha256.data(), bytes.data() + 0xA0 + 0x20 * i, 32);
    }
    ReadAt(bytes, 0x3C, bss_size_);
    uint32_t dynsym_size = 0;
    ReadAt(bytes, 0x9C, dynsym_size);

    const uint64_t data_end = uint64_t{Data().memory_offset} + Data().size;
    image_.assign((data_end + bss_size_ + 0xFFF) & ~uint64_t{0xFFF}, 0);
    for (int i = 0; i < 3; ++i) {
        const Segment& s = segments_[i];
        if (uint64_t{s.file_offset} + s.file_size > file.size() || uint64_t{s.memory_offset} + s.size > image_.size()) {
            error = path + ": segmento " + std::to_string(i) + " fora dos limites";
            return false;
        }
        const std::span<const uint8_t> src = bytes.subspan(s.file_offset, s.file_size);
        const std::span<uint8_t> dst(image_.data() + s.memory_offset, s.size);
        bool loaded = false;
        if (s.compressed) {
            loaded = Lz4DecompressBlock(src, dst);
        } else if (s.file_size == s.size) {
            std::memcpy(dst.data(), src.data(), s.size);
            loaded = true;
        }
        if (!loaded) {
            error = path + ": falha ao descomprimir o segmento " + std::to_string(i);
            return false;
        }
        if (s.hash_checked && Sha256(dst) != s.sha256) {
            error = path + ": SHA-256 do segmento " + std::to_string(i) + " nao confere";
            return false;
        }
    }

    uint32_t mod0 = 0, mod0_magic = 0;
    int32_t offsets[6] = {};
    if (!ReadAt(Image(), 4, mod0) || !ReadAt(Image(), mod0, mod0_magic) || mod0_magic != 0x30444F4D ||  // "MOD0"
        !ReadAt(Image(), uint64_t{mod0} + 4, offsets)) {
        error = path + ": MOD0 ausente";
        return false;
    }
    mod0_ = mod0;
    dynamic_ = mod0 + static_cast<int64_t>(offsets[0]);
    eh_frame_hdr_ = mod0 + static_cast<int64_t>(offsets[3]);
    eh_frame_hdr_end_ = mod0 + static_cast<int64_t>(offsets[4]);
    dynsym_count_ = dynsym_size / 24;
    return ParseDynamic(error) && ParseEhFrame(error);
}

bool NsoModule::ParseDynamic(std::string& error) {
    const std::span<const uint8_t> image = Image();
    std::unordered_map<int64_t, uint64_t> tags;
    for (uint64_t pos = dynamic_;; pos += 16) {
        int64_t tag = 0;
        uint64_t value = 0;
        if (!ReadAt(image, pos, tag) || !ReadAt(image, pos + 8, value)) {
            error = path_ + ": .dynamic truncada";
            return false;
        }
        if (tag == 0) break;
        tags[tag] = value;
    }
    const auto tag = [&tags](int64_t t) { const auto it = tags.find(t); return it == tags.end() ? uint64_t{0} : it->second; };
    const uint64_t symtab = tag(6), strtab = tag(5), strsz = tag(10);
    if (!symtab || !strtab) {
        error = path_ + ": sem DT_SYMTAB/DT_STRTAB";
        return false;
    }
    symbols_.clear();
    for (uint64_t i = 0; i < dynsym_count_; ++i) {
        const uint64_t entry = symtab + 24 * i;
        uint32_t name = 0;
        uint8_t info = 0;
        Symbol symbol;
        if (!ReadAt(image, entry, name) || !ReadAt(image, entry + 4, info) || !ReadAt(image, entry + 6, symbol.section) ||
            !ReadAt(image, entry + 8, symbol.value) || !ReadAt(image, entry + 16, symbol.size)) {
            error = path_ + ": dynsym truncada";
            return false;
        }
        symbol.type = info & 0x0F;
        symbol.binding = info >> 4;
        if (name < strsz && strtab + name < image.size()) {
            const char* text = reinterpret_cast<const char*>(image.data() + strtab + name);
            symbol.name.assign(text, strnlen(text, strsz - name));
        }
        symbols_.push_back(std::move(symbol));
    }

    relocations_.clear();
    const auto read_relocs = [&](uint64_t start, uint64_t size, bool plt) {
        for (uint64_t pos = start; start && pos + 24 <= start + size; pos += 24) {
            Relocation r;
            uint64_t info = 0;
            ReadAt(image, pos, r.offset);
            ReadAt(image, pos + 8, info);
            ReadAt(image, pos + 16, r.addend);
            r.type = static_cast<uint32_t>(info);
            r.symbol = static_cast<uint32_t>(info >> 32);
            r.plt = plt;
            relocations_.push_back(r);
        }
    };
    read_relocs(tag(7), tag(8), false);   // DT_RELA / DT_RELASZ
    read_relocs(tag(23), tag(2), true);   // DT_JMPREL / DT_PLTRELSZ

    // DT_RELR (36) / DT_RELRSZ (35): entrada par = endereco a corrigir; impar = bitmap dos 63 enderecos seguintes
    const uint64_t relr = tag(36), relr_size = tag(35);
    uint64_t where = 0;
    for (uint64_t pos = relr; relr && pos + 8 <= relr + relr_size; pos += 8) {
        uint64_t entry = 0;
        ReadAt(image, pos, entry);
        const auto add = [&](uint64_t offset) {
            Relocation r;
            r.offset = offset;
            r.type = kRelocRelative;
            ReadAt(image, offset, r.addend);
            r.relr = true;
            relocations_.push_back(r);
        };
        if ((entry & 1) == 0) {
            add(entry);
            where = entry + 8;
        } else {
            for (unsigned bit = 1; bit < 64; ++bit) {
                if ((entry >> bit) & 1) add(where + 8 * (bit - 1));
            }
            where += 63 * 8;
        }
    }
    init_array_ = tag(25);
    init_array_count_ = tag(27) / 8;
    return true;
}

bool NsoModule::ParseEhFrame(std::string& error) {
    const std::span<const uint8_t> image = Image();
    uint64_t pos = eh_frame_hdr_;
    uint8_t version = 0, ptr_enc = 0, count_enc = 0, table_enc = 0;
    if (!ReadAt(image, pos, version) || version != 1 || !ReadAt(image, pos + 1, ptr_enc) || !ReadAt(image, pos + 2, count_enc) ||
        !ReadAt(image, pos + 3, table_enc)) {
        error = path_ + ": .eh_frame_hdr invalido";
        return false;
    }
    pos += 4;
    uint64_t eh_frame = 0, count = 0;
    if (!ReadEncoded(image, pos, ptr_enc, eh_frame_hdr_, eh_frame) || !ReadEncoded(image, pos, count_enc, eh_frame_hdr_, count)) {
        error = path_ + ": .eh_frame_hdr sem tabela";
        return false;
    }

    struct CieInfo {
        uint8_t fde_encoding{0};
    };
    std::unordered_map<uint64_t, CieInfo> cies;
    const auto parse_cie = [&](uint64_t cie, CieInfo& out) {
        if (const auto it = cies.find(cie); it != cies.end()) {
            out = it->second;
            return true;
        }
        uint32_t length = 0, id = 0;
        uint8_t cie_version = 0;
        if (!ReadAt(image, cie, length) || length == 0xFFFFFFFF || !ReadAt(image, cie + 4, id) || id != 0 ||
            !ReadAt(image, cie + 8, cie_version)) {
            return false;
        }
        uint64_t p = cie + 9;
        const char* aug = reinterpret_cast<const char*>(image.data() + p);
        const std::string augmentation(aug, strnlen(aug, 16));
        p += augmentation.size() + 1;
        uint64_t ignored = 0;
        int64_t signed_ignored = 0;
        if (cie_version >= 4) p += 2;  // address_size, segment_size
        if (!ReadUleb(image, p, ignored) || !ReadSleb(image, p, signed_ignored)) return false;
        if (cie_version == 1) {
            ++p;
        } else if (!ReadUleb(image, p, ignored)) {
            return false;
        }
        CieInfo info;
        if (!augmentation.empty() && augmentation[0] == 'z') {
            if (!ReadUleb(image, p, ignored)) return false;
            for (size_t i = 1; i < augmentation.size(); ++i) {
                const char c = augmentation[i];
                if (c == 'R') {
                    info.fde_encoding = image[p++];
                } else if (c == 'L') {
                    ++p;
                } else if (c == 'P') {
                    const uint8_t enc = image[p++];
                    uint64_t personality = 0;
                    if (!ReadEncoded(image, p, enc & 0x7F, eh_frame_hdr_, personality)) return false;
                } else if (c != 'S' && c != 'B') {
                    return false;
                }
            }
        }
        cies.emplace(cie, info);
        out = info;
        return true;
    };

    functions_.clear();
    functions_.reserve(count);
    for (uint64_t i = 0; i < count; ++i) {
        uint64_t initial = 0, fde = 0;
        if (!ReadEncoded(image, pos, table_enc, eh_frame_hdr_, initial) || !ReadEncoded(image, pos, table_enc, eh_frame_hdr_, fde)) {
            error = path_ + ": tabela do .eh_frame_hdr truncada na entrada " + std::to_string(i);
            return false;
        }
        uint32_t length = 0, cie_pointer = 0;
        if (!ReadAt(image, fde, length) || length == 0xFFFFFFFF || !ReadAt(image, fde + 4, cie_pointer)) {
            error = path_ + ": FDE invalida em " + std::to_string(fde);
            return false;
        }
        CieInfo cie;
        if (!parse_cie(fde + 4 - cie_pointer, cie)) {
            error = path_ + ": CIE invalida para a FDE em " + std::to_string(fde);
            return false;
        }
        uint64_t p = fde + 8, begin = 0, range = 0;
        if (!ReadEncoded(image, p, cie.fde_encoding, eh_frame_hdr_, begin) ||
            !ReadEncoded(image, p, cie.fde_encoding & 0x0F, eh_frame_hdr_, range) || begin != initial) {
            error = path_ + ": FDE em " + std::to_string(fde) + " nao confere com a tabela";
            return false;
        }
        functions_.push_back({begin, range});
    }
    std::sort(functions_.begin(), functions_.end(), [](const FunctionRange& a, const FunctionRange& b) { return a.start < b.start; });
    return true;
}

const Symbol* NsoModule::FindExport(const std::string& name) const {
    for (const Symbol& symbol : symbols_) {
        if (symbol.section != 0 && symbol.name == name) return &symbol;
    }
    return nullptr;
}

} // namespace recomp
