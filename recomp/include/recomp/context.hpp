#pragma once
// Estado da CPU do jogo (AArch64) e operacoes usadas pelo codigo gerado
//
// Memoria: o jogo e mapeado no host no mesmo endereco do Switch, entao um endereco do jogo e um ponteiro do host.
// As funcoes aqui sao inline e sem estado global para que o codigo gerado possa otimizar livremente.

#include <atomic>
#include <bit>
#include <cstdint>
#include <cstring>

namespace recomp {

// Base onde o .text do main fica mapeado (mesma do Switch e do relatorio de boot)
inline constexpr uint64_t kImageBase = 0x7100000000ULL;

struct Context {
    uint64_t x[32]{};      // X0..X30; x[31] nao e usado (XZR/SP sao tratados pelo gerador)
    uint64_t sp{0};
    bool n{false}, z{false}, c{false}, v{false};
    uint64_t tpidr{0};     // TPIDR_EL0
    uint64_t tpidrro{0};   // TPIDRRO_EL0 (TLS do Horizon)
    uint32_t fpcr{0}, fpsr{0};
};

template <typename T>
inline T Load(uint64_t address) {
    T value;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
    return value;
}

template <typename T>
inline void Store(uint64_t address, T value) {
    std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(T));
}

// ADD/SUB com flags (ADDS/SUBS/ADCS/SBCS/CMP/CMN): resultado e NZCV como AddWithCarry do manual ARM
template <typename T>
inline T AddWithCarry(Context& c, T a, T b, bool carry_in, bool set_flags) {
    using S = std::make_signed_t<T>;
    const T result = static_cast<T>(a + b + static_cast<T>(carry_in));
    if (set_flags) {
        c.n = static_cast<S>(result) < 0;
        c.z = result == 0;
        c.c = carry_in ? result <= a : result < a;
        c.v = ((~(a ^ b) & (a ^ result)) >> (sizeof(T) * 8 - 1)) & 1;
    }
    return result;
}

template <typename T>
inline void SetLogicFlags(Context& c, T result) {
    c.n = (result >> (sizeof(T) * 8 - 1)) & 1;
    c.z = result == 0;
    c.c = false;
    c.v = false;
}

inline bool Condition(const Context& c, unsigned cond) {
    bool r = false;
    switch (cond >> 1) {
    case 0: r = c.z; break;                      // EQ / NE
    case 1: r = c.c; break;                      // CS / CC
    case 2: r = c.n; break;                      // MI / PL
    case 3: r = c.v; break;                      // VS / VC
    case 4: r = c.c && !c.z; break;              // HI / LS
    case 5: r = c.n == c.v; break;               // GE / LT
    case 6: r = c.n == c.v && !c.z; break;       // GT / LE
    default: r = true; break;                    // AL / NV
    }
    return (cond & 1) && cond != 15 ? !r : r;
}

inline void SetNzcv(Context& c, unsigned nzcv) {
    c.n = (nzcv >> 3) & 1;
    c.z = (nzcv >> 2) & 1;
    c.c = (nzcv >> 1) & 1;
    c.v = nzcv & 1;
}

template <typename T>
inline T RotateRight(T value, unsigned amount) {
    return std::rotr(value, static_cast<int>(amount % (sizeof(T) * 8)));
}

// UBFM/SBFM/BFM com immr = r, imms = s (lsl, lsr, asr, ubfx, sbfx, uxtb, sxtw, bfi, bfxil... sao aliases)
template <typename T>
inline T Bitfield(T dst, T src, unsigned r, unsigned s, bool sign, bool merge) {
    constexpr unsigned width = sizeof(T) * 8;
    using S = std::make_signed_t<T>;
    const auto mask = [](unsigned bits) { return bits >= width ? ~T{0} : static_cast<T>((T{1} << bits) - 1); };
    if (s >= r) {
        // Extrai bits [r, s] para a posicao 0
        const unsigned len = s - r + 1;
        T field = static_cast<T>((src >> r) & mask(len));
        if (merge) return static_cast<T>((dst & ~mask(len)) | field);
        if (sign && len < width && ((field >> (len - 1)) & 1)) field |= static_cast<T>(~mask(len));
        return field;
    }
    // Insere bits [0, s] na posicao width - r
    const unsigned len = s + 1, pos = width - r;
    const T field = static_cast<T>((src & mask(len)) << pos);
    const T field_mask = static_cast<T>(mask(len) << pos);
    if (merge) return static_cast<T>((dst & ~field_mask) | field);
    if (sign && ((src >> s) & 1) && pos + len < width) {
        return static_cast<T>(static_cast<S>(field) | static_cast<S>(~mask(pos + len)));
    }
    return field;
}

template <typename T>
inline T Extract(T hi, T lo, unsigned lsb) {
    constexpr unsigned width = sizeof(T) * 8;
    return lsb == 0 ? lo : static_cast<T>((lo >> lsb) | (hi << (width - lsb)));
}

template <typename T>
inline T UnsignedDivide(T a, T b) { return b == 0 ? 0 : static_cast<T>(a / b); }

template <typename T>
inline T SignedDivide(T a, T b) {
    using S = std::make_signed_t<T>;
    const S sa = static_cast<S>(a), sb = static_cast<S>(b);
    if (sb == 0) return 0;
    if (sb == -1) return static_cast<T>(T{0} - a);  // evita overflow de INT_MIN / -1
    return static_cast<T>(sa / sb);
}

template <typename T>
inline T ReverseBits(T value) {
    T out = 0;
    for (unsigned i = 0; i < sizeof(T) * 8; ++i) out |= static_cast<T>(((value >> i) & 1) << (sizeof(T) * 8 - 1 - i));
    return out;
}

// REV16 / REV32 / REV: inverte os bytes dentro de cada bloco de container_bytes
template <typename T>
inline T ReverseBytes(T value, unsigned container_bytes) {
    T out = 0;
    for (unsigned base = 0; base < sizeof(T); base += container_bytes) {
        for (unsigned i = 0; i < container_bytes; ++i) {
            const T byte = (value >> (8 * (base + i))) & 0xFF;
            out |= static_cast<T>(byte << (8 * (base + container_bytes - 1 - i)));
        }
    }
    return out;
}

template <typename T>
inline T CountLeadingSignBits(T value) {
    using S = std::make_signed_t<T>;
    const T x = static_cast<T>(value ^ static_cast<T>(static_cast<S>(value) >> 1));
    return static_cast<T>(std::countl_zero(x) - 1);
}

inline uint64_t MulHighUnsigned(uint64_t a, uint64_t b) { return static_cast<uint64_t>((static_cast<unsigned __int128>(a) * b) >> 64); }
inline uint64_t MulHighSigned(uint64_t a, uint64_t b) {
    return static_cast<uint64_t>((static_cast<__int128>(static_cast<int64_t>(a)) * static_cast<int64_t>(b)) >> 64);
}

inline void Barrier() { std::atomic_thread_fence(std::memory_order_seq_cst); }

// Implementados pelo runtime (fase R4): chamada indireta por endereco do jogo, instrucao nao traduzida, BRK e
// contador do sistema (CNTVCT_EL0, 19,2 MHz no Switch)
void Dispatch(Context& c, uint64_t guest_address);
[[noreturn]] void Unimplemented(Context& c, uint64_t rva, uint32_t word);
[[noreturn]] void Trap(Context& c, uint64_t rva, uint32_t imm);
uint64_t ReadCounter();

} // namespace recomp
