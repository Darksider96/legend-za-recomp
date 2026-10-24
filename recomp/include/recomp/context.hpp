#pragma once
// Estado da CPU do jogo (AArch64) e operacoes usadas pelo codigo gerado
//
// Memoria: o jogo e mapeado no host no mesmo endereco do Switch, entao um endereco do jogo e um ponteiro do host.
// As funcoes aqui sao inline e sem estado global para que o codigo gerado possa otimizar livremente.

#include <atomic>
#include <bit>
#include <cstdint>
#include <cstring>
#include <type_traits>

#if !defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16)
#error "compile com -mcx16: LDXP/STXP de 64 bits usam cmpxchg16b"
#endif

namespace recomp {

// Base onde o .text do main fica mapeado (mesma do Switch e do relatorio de boot)
inline constexpr uint64_t kImageBase = 0x7100000000ULL;

// Registrador SIMD/FP de 128 bits (V0..V31); B/H/S/D/Q sao a parte baixa
struct alignas(16) V128 {
    uint8_t b[16]{};
};

inline constexpr uint64_t kNoExclusive = ~uint64_t{0};

struct Context {
    uint64_t x[32]{};      // X0..X30; x[31] nao e usado (XZR/SP sao tratados pelo gerador)
    uint64_t sp{0};
    bool n{false}, z{false}, c{false}, v{false};
    uint64_t tpidr{0};     // TPIDR_EL0
    uint64_t tpidrro{0};   // TPIDRRO_EL0 (TLS do Horizon)
    uint32_t fpcr{0}, fpsr{0};
    V128 vreg[32]{};
    // Monitor exclusivo local (LDXR/STXR): endereco e valor lidos pelo ultimo load exclusivo
    uint64_t exclusive_address{kNoExclusive};
    unsigned __int128 exclusive_value{0};
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

// Exclusivos e acquire/release. As threads do jogo rodam em threads do host, entao o monitor exclusivo e emulado
// com compare-and-swap: o STXR so grava se a memoria ainda tem o valor lido pelo LDXR (mesmo modelo dos emuladores
// de Switch; aceita ABA, que spinlocks e contadores toleram). O endereco e alinhado por exigencia da arquitetura.
template <typename T>
inline std::atomic_ref<T> AtomicAt(uint64_t address) {
    return std::atomic_ref<T>(*reinterpret_cast<T*>(address));
}

template <typename T>
inline T LoadExclusive(Context& c, uint64_t address, bool acquire) {
    const T value = AtomicAt<T>(address).load(acquire ? std::memory_order_acquire : std::memory_order_relaxed);
    c.exclusive_address = address;
    c.exclusive_value = value;
    return value;
}

// Devolve o status do STXR: 0 gravou, 1 falhou
template <typename T>
inline uint32_t StoreExclusive(Context& c, uint64_t address, T value, bool /*release*/) {
    const bool armed = c.exclusive_address == address;
    c.exclusive_address = kNoExclusive;
    if (!armed) return 1;
    T expected = static_cast<T>(c.exclusive_value);
    return AtomicAt<T>(address).compare_exchange_strong(expected, value, std::memory_order_seq_cst) ? 0 : 1;
}

// LDXP/STXP: par de W (64 bits) ou de X (128 bits, cmpxchg16b). A leitura do par pode rasgar; nesse caso o
// compare-and-swap do STXP falha e o jogo repete, como no hardware.
template <typename T>
using PairOf = std::conditional_t<sizeof(T) == 8, unsigned __int128, uint64_t>;

template <typename T>
inline void LoadExclusivePair(Context& c, uint64_t address, bool acquire, T& lo, T& hi) {
    PairOf<T> pair;
    std::memcpy(&pair, reinterpret_cast<const void*>(address), sizeof(pair));
    if (acquire) std::atomic_thread_fence(std::memory_order_acquire);
    lo = static_cast<T>(pair);
    hi = static_cast<T>(pair >> (sizeof(T) * 8));
    c.exclusive_address = address;
    c.exclusive_value = pair;
}

template <typename T>
inline uint32_t StoreExclusivePair(Context& c, uint64_t address, T lo, T hi, bool /*release*/) {
    const bool armed = c.exclusive_address == address;
    c.exclusive_address = kNoExclusive;
    if (!armed) return 1;
    const PairOf<T> expected = static_cast<PairOf<T>>(c.exclusive_value);
    const PairOf<T> desired = PairOf<T>{lo} | (PairOf<T>{hi} << (sizeof(T) * 8));
    return __sync_bool_compare_and_swap(reinterpret_cast<PairOf<T>*>(address), expected, desired) ? 0 : 1;
}

template <typename T>
inline T LoadAcquire(uint64_t address) {
    return AtomicAt<T>(address).load(std::memory_order_acquire);
}

// STLR seguido de LDAR fica ordenado no ARM (RCsc) e o x86 reordena store -> load: seq_cst (xchg) evita isso
template <typename T>
inline void StoreRelease(uint64_t address, T value) {
    AtomicAt<T>(address).store(value, std::memory_order_seq_cst);
}

// Implementados pelo runtime (fase R4): chamada indireta por endereco do jogo, instrucao nao traduzida, BRK e
// contador do sistema (CNTVCT_EL0, 19,2 MHz no Switch)
void Dispatch(Context& c, uint64_t guest_address);
[[noreturn]] void Unimplemented(Context& c, uint64_t rva, uint32_t word);
[[noreturn]] void Trap(Context& c, uint64_t rva, uint32_t imm);
uint64_t ReadCounter();

} // namespace recomp
