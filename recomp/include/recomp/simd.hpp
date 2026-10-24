#pragma once
// Semantica SIMD/FP do A64 para o codigo gerado (fase R3)
//
// Vetores: V128 com elementos acessados por memcpy (o GCC vetoriza os lacos curtos). Toda escrita de vetor de 64
// bits ou de escalar zera o resto do registrador, como no ARM: os mapas partem de um V128 zerado.
//
// Ponto flutuante: segue o pseudocodigo do manual ARM com FPCR = 0 (arredondamento ao mais proximo, sem
// flush-to-zero, sem NaN padrao): propagacao de NaN (FPProcessNaNs), FMAX/FMIN com zeros, FMA fundida,
// conversoes saturadas e as estimativas FRECPE/FRSQRTE do ARMv8.0. O codigo gerado precisa de -ffp-contract=off
// (senao o compilador funde multiplicacao e soma de instrucoes diferentes). Excecoes de FP (FPSR) nao sao emuladas.

#include "recomp/context.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace recomp {

// ---- Lanes ----

template <typename E>
inline E Lane(const V128& v, unsigned i) {
    E e;
    std::memcpy(&e, v.b + i * sizeof(E), sizeof(E));
    return e;
}

template <typename E>
inline void SetLane(V128& v, unsigned i, E e) {
    std::memcpy(v.b + i * sizeof(E), &e, sizeof(E));
}

template <typename E>
inline V128 Scalar(E e) {
    V128 r{};
    SetLane(r, 0, e);
    return r;
}

inline V128 FromU64(uint64_t lo, uint64_t hi) {
    V128 r;
    std::memcpy(r.b, &lo, 8);
    std::memcpy(r.b + 8, &hi, 8);
    return r;
}

inline uint64_t Lo64(const V128& v) { return Lane<uint64_t>(v, 0); }
inline uint64_t Hi64(const V128& v) { return Lane<uint64_t>(v, 1); }

template <unsigned Bytes>
inline V128 LoadVector(uint64_t address) {
    V128 r{};
    std::memcpy(r.b, reinterpret_cast<const void*>(address), Bytes);
    return r;
}

template <unsigned Bytes>
inline void StoreVector(uint64_t address, const V128& v) {
    std::memcpy(reinterpret_cast<void*>(address), v.b, Bytes);
}

template <size_t Bytes>
using UIntOfSize = std::conditional_t<Bytes == 1, uint8_t, std::conditional_t<Bytes == 2, uint16_t, std::conditional_t<Bytes == 4, uint32_t, uint64_t>>>;

// Mascara de comparacao: todos os bits do elemento ligados
template <typename E>
inline UIntOfSize<sizeof(E)> Mask(bool condition) {
    return condition ? static_cast<UIntOfSize<sizeof(E)>>(~UIntOfSize<sizeof(E)>{0}) : UIntOfSize<sizeof(E)>{0};
}

template <typename E, unsigned N, typename F>
inline V128 Map1(const V128& a, F f) {
    V128 r{};
    for (unsigned i = 0; i < N; ++i) SetLane(r, i, f(Lane<E>(a, i)));
    return r;
}

template <typename E, unsigned N, typename F>
inline V128 Map2(const V128& a, const V128& b, F f) {
    V128 r{};
    for (unsigned i = 0; i < N; ++i) SetLane(r, i, f(Lane<E>(a, i), Lane<E>(b, i)));
    return r;
}

// Acumuladores (MLA, FMLA, BSL, SRA...): f(destino, a, b)
template <typename E, unsigned N, typename F>
inline V128 Map3(const V128& acc, const V128& a, const V128& b, F f) {
    V128 r{};
    for (unsigned i = 0; i < N; ++i) SetLane(r, i, f(Lane<E>(acc, i), Lane<E>(a, i), Lane<E>(b, i)));
    return r;
}

// Pares adjacentes de a e depois de b (ADDP, FADDP, SMAXP...)
template <typename E, unsigned N, typename F>
inline V128 Pairwise(const V128& a, const V128& b, F f) {
    V128 r{};
    for (unsigned i = 0; i < N / 2; ++i) {
        SetLane(r, i, f(Lane<E>(a, 2 * i), Lane<E>(a, 2 * i + 1)));
        SetLane(r, i + N / 2, f(Lane<E>(b, 2 * i), Lane<E>(b, 2 * i + 1)));
    }
    return r;
}

// Reducao em arvore (metade baixa, metade alta), como Reduce() do manual: a ordem importa para FP
template <typename E, typename F>
inline E ReduceRange(const V128& a, unsigned lo, unsigned n, F f) {
    if (n == 1) return Lane<E>(a, lo);
    return f(ReduceRange<E>(a, lo, n / 2, f), ReduceRange<E>(a, lo + n / 2, n / 2, f));
}

template <typename E, unsigned N, typename F>
inline E Reduce(const V128& a, F f) {
    return ReduceRange<E>(a, 0, N, f);
}

template <typename E, unsigned N>
inline V128 Dup(E e) {
    V128 r{};
    for (unsigned i = 0; i < N; ++i) SetLane(r, i, e);
    return r;
}

template <typename E, unsigned N>
inline V128 DupLane(const V128& v, unsigned index) {
    return Dup<E, N>(Lane<E>(v, index));
}

// Formas longas: elementos estreitos da metade baixa (ou alta, variante "2") estendidos para int64/uint64
template <typename N>
using Extended = std::conditional_t<std::is_signed_v<N>, int64_t, uint64_t>;

template <typename N, typename W, typename F>
inline V128 Long(const V128& a, const V128& b, bool upper, F f) {
    constexpr unsigned lanes = 8 / sizeof(N);
    V128 r{};
    for (unsigned i = 0; i < lanes; ++i) {
        const unsigned j = i + (upper ? lanes : 0);
        SetLane(r, i, static_cast<W>(f(static_cast<Extended<N>>(Lane<N>(a, j)), static_cast<Extended<N>>(Lane<N>(b, j)))));
    }
    return r;
}

template <typename N, typename W, typename F>
inline V128 LongAcc(const V128& acc, const V128& a, const V128& b, bool upper, F f) {
    constexpr unsigned lanes = 8 / sizeof(N);
    V128 r{};
    for (unsigned i = 0; i < lanes; ++i) {
        const unsigned j = i + (upper ? lanes : 0);
        SetLane(r, i, static_cast<W>(f(Lane<W>(acc, i), static_cast<Extended<N>>(Lane<N>(a, j)), static_cast<Extended<N>>(Lane<N>(b, j)))));
    }
    return r;
}

// SADDW/UADDW: a ja e largo (W), b estreito
template <typename N, typename W, typename F>
inline V128 Wide(const V128& a, const V128& b, bool upper, F f) {
    constexpr unsigned lanes = 8 / sizeof(N);
    using SW = std::conditional_t<std::is_signed_v<N>, std::make_signed_t<W>, W>;
    V128 r{};
    for (unsigned i = 0; i < lanes; ++i) {
        const unsigned j = i + (upper ? lanes : 0);
        SetLane(r, i, static_cast<W>(f(static_cast<Extended<N>>(static_cast<SW>(Lane<W>(a, i))), static_cast<Extended<N>>(Lane<N>(b, j)))));
    }
    return r;
}

// Um operando estendido (SSHLL/USHLL/FCVTL e afins)
template <typename N, typename W, typename F>
inline V128 Widen(const V128& a, bool upper, F f) {
    constexpr unsigned lanes = 8 / sizeof(N);
    V128 r{};
    for (unsigned i = 0; i < lanes; ++i) SetLane(r, i, static_cast<W>(f(Lane<N>(a, i + (upper ? lanes : 0)))));
    return r;
}

// Formas estreitas (XTN, SHRN, FCVTN, ADDHN): resultado na metade baixa (zera a alta) ou na alta (variante "2",
// preserva a baixa do destino)
template <typename W, typename N, typename F>
inline V128 Narrow(const V128& dst, const V128& a, bool upper, F f) {
    constexpr unsigned lanes = 8 / sizeof(N);
    V128 r{};
    if (upper) std::memcpy(r.b, dst.b, 8);
    for (unsigned i = 0; i < lanes; ++i) SetLane(r, i + (upper ? lanes : 0), static_cast<N>(f(Lane<W>(a, i))));
    return r;
}

template <typename W, typename N, typename F>
inline V128 Narrow2(const V128& dst, const V128& a, const V128& b, bool upper, F f) {
    constexpr unsigned lanes = 8 / sizeof(N);
    V128 r{};
    if (upper) std::memcpy(r.b, dst.b, 8);
    for (unsigned i = 0; i < lanes; ++i) SetLane(r, i + (upper ? lanes : 0), static_cast<N>(f(Lane<W>(a, i), Lane<W>(b, i))));
    return r;
}

// SADDLP/UADDLP: soma de pares adjacentes estendida
template <typename N, typename W>
inline V128 AddLongPairwise(const V128& a, bool q) {
    const unsigned lanes = (q ? 16 : 8) / sizeof(W);
    V128 r{};
    for (unsigned i = 0; i < lanes; ++i) {
        SetLane(r, i, static_cast<W>(static_cast<Extended<N>>(Lane<N>(a, 2 * i)) + static_cast<Extended<N>>(Lane<N>(a, 2 * i + 1))));
    }
    return r;
}

// SADDLV/UADDLV: soma de todos os elementos, estendida
template <typename N, unsigned Lanes>
inline Extended<N> SumLong(const V128& a) {
    Extended<N> sum = 0;
    for (unsigned i = 0; i < Lanes; ++i) sum += static_cast<Extended<N>>(Lane<N>(a, i));
    return sum;
}

// REV16/REV32/REV64: inverte elementos dentro de cada bloco de Container bytes
template <typename E, unsigned Container>
inline V128 ReverseElements(const V128& a, bool q) {
    constexpr unsigned per = Container / sizeof(E);
    const unsigned lanes = (q ? 16 : 8) / sizeof(E);
    V128 r{};
    for (unsigned i = 0; i < lanes; ++i) SetLane(r, i, Lane<E>(a, (i / per) * per + (per - 1 - i % per)));
    return r;
}

// Permutacoes
template <typename E, unsigned N>
inline V128 Zip(const V128& a, const V128& b, bool second) {
    V128 r{};
    const unsigned base = second ? N / 2 : 0;
    for (unsigned i = 0; i < N / 2; ++i) {
        SetLane(r, 2 * i, Lane<E>(a, base + i));
        SetLane(r, 2 * i + 1, Lane<E>(b, base + i));
    }
    return r;
}

template <typename E, unsigned N>
inline V128 Uzp(const V128& a, const V128& b, bool second) {
    V128 r{};
    for (unsigned i = 0; i < N; ++i) {
        const unsigned j = 2 * i + (second ? 1 : 0);
        SetLane(r, i, j < N ? Lane<E>(a, j) : Lane<E>(b, j - N));
    }
    return r;
}

template <typename E, unsigned N>
inline V128 Trn(const V128& a, const V128& b, bool second) {
    V128 r{};
    for (unsigned i = 0; i < N / 2; ++i) {
        SetLane(r, 2 * i, Lane<E>(a, 2 * i + (second ? 1 : 0)));
        SetLane(r, 2 * i + 1, Lane<E>(b, 2 * i + (second ? 1 : 0)));
    }
    return r;
}

inline V128 Ext(const V128& a, const V128& b, unsigned index, bool q) {
    const unsigned bytes = q ? 16 : 8;
    uint8_t concat[32];
    std::memcpy(concat, a.b, bytes);
    std::memcpy(concat + bytes, b.b, bytes);
    V128 r{};
    std::memcpy(r.b, concat + index, bytes);
    return r;
}

// TBL/TBX: tabela de count registradores consecutivos (modulo 32) a partir de first
inline V128 TableLookup(const Context& c, unsigned first, unsigned count, const V128& indices, bool q, const V128* keep) {
    V128 r{};
    const unsigned bytes = q ? 16 : 8;
    for (unsigned i = 0; i < bytes; ++i) {
        const unsigned index = indices.b[i];
        if (index < 16 * count) r.b[i] = c.vreg[(first + index / 16) % 32].b[index % 16];
        else if (keep) r.b[i] = keep->b[i];
    }
    return r;
}

// ---- Inteiros ----

template <typename E>
inline E SatAdd(E a, E b) {
    E r;
    if (__builtin_add_overflow(a, b, &r)) {
        if constexpr (std::is_unsigned_v<E>) return std::numeric_limits<E>::max();
        else return b < 0 ? std::numeric_limits<E>::min() : std::numeric_limits<E>::max();
    }
    return r;
}

template <typename E>
inline E SatSub(E a, E b) {
    E r;
    if (__builtin_sub_overflow(a, b, &r)) {
        if constexpr (std::is_unsigned_v<E>) return 0;
        else return a < 0 ? std::numeric_limits<E>::min() : std::numeric_limits<E>::max();
    }
    return r;
}

// SSHL/USHL: deslocamento pelo byte baixo (com sinal) do elemento de b; negativo desloca para a direita
template <typename E>
inline E ShiftByRegister(E a, E b) {
    constexpr int bits = sizeof(E) * 8;
    const int shift = static_cast<int8_t>(static_cast<uint8_t>(b));
    using U = std::make_unsigned_t<E>;
    if (shift >= 0) return shift >= bits ? E{0} : static_cast<E>(static_cast<U>(a) << shift);
    const int right = -shift;
    if constexpr (std::is_signed_v<E>) return right >= bits ? static_cast<E>(a < 0 ? -1 : 0) : static_cast<E>(a >> right);
    else return right >= bits ? E{0} : static_cast<E>(a >> right);
}

// ---- Ponto flutuante ----

template <typename T>
using FpBits = std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>;

template <typename T>
inline constexpr FpBits<T> kQuietBit = static_cast<FpBits<T>>(sizeof(T) == 4 ? 0x00400000ull : 0x0008000000000000ull);

template <typename T>
inline bool IsNaN(T v) { return v != v; }

template <typename T>
inline bool IsSNaN(T v) { return IsNaN(v) && !(std::bit_cast<FpBits<T>>(v) & kQuietBit<T>); }

template <typename T>
inline T Quiet(T v) { return std::bit_cast<T>(static_cast<FpBits<T>>(std::bit_cast<FpBits<T>>(v) | kQuietBit<T>)); }

template <typename T>
inline T DefaultNaN() {
    if constexpr (sizeof(T) == 4) return std::bit_cast<float>(0x7FC00000u);
    else return std::bit_cast<double>(0x7FF8000000000000ull);
}

// FPProcessNaNs: so chamadas quando o resultado do host e NaN (entrada NaN ou operacao invalida)
template <typename T>
inline T NaN1(T a) { return IsNaN(a) ? Quiet(a) : DefaultNaN<T>(); }

template <typename T>
inline T NaN2(T a, T b) {
    if (IsSNaN(a)) return Quiet(a);
    if (IsSNaN(b)) return Quiet(b);
    if (IsNaN(a)) return a;
    if (IsNaN(b)) return b;
    return DefaultNaN<T>();
}

template <typename T>
inline T NaN3(T a, T b, T c) {
    if (IsSNaN(a)) return Quiet(a);
    if (IsSNaN(b)) return Quiet(b);
    if (IsSNaN(c)) return Quiet(c);
    if (IsNaN(a)) return a;
    if (IsNaN(b)) return b;
    if (IsNaN(c)) return c;
    return DefaultNaN<T>();
}

template <typename T>
inline T FpAbs(T a) { return std::bit_cast<T>(static_cast<FpBits<T>>(std::bit_cast<FpBits<T>>(a) & ~(FpBits<T>{1} << (sizeof(T) * 8 - 1)))); }

template <typename T>
inline T FpNeg(T a) { return std::bit_cast<T>(static_cast<FpBits<T>>(std::bit_cast<FpBits<T>>(a) ^ (FpBits<T>{1} << (sizeof(T) * 8 - 1)))); }

template <typename T>
inline T FpAdd(T a, T b) {
    const T r = a + b;
    return IsNaN(r) ? NaN2(a, b) : r;
}

template <typename T>
inline T FpSub(T a, T b) {
    const T r = a - b;
    return IsNaN(r) ? NaN2(a, b) : r;
}

template <typename T>
inline T FpMul(T a, T b) {
    const T r = a * b;
    return IsNaN(r) ? NaN2(a, b) : r;
}

template <typename T>
inline T FpDiv(T a, T b) {
    const T r = a / b;
    return IsNaN(r) ? NaN2(a, b) : r;
}

template <typename T>
inline bool InfTimesZero(T a, T b) { return (std::isinf(a) && b == 0) || (a == 0 && std::isinf(b)); }

// FMULX: infinito vezes zero da 2 com o sinal do produto
template <typename T>
inline T FpMulX(T a, T b) {
    if (IsNaN(a) || IsNaN(b)) return NaN2(a, b);
    if (InfTimesZero(a, b)) return std::signbit(a) != std::signbit(b) ? T{-2} : T{2};
    return a * b;
}

template <typename T>
inline T FpMax(T a, T b) {
    if (IsNaN(a) || IsNaN(b)) return NaN2(a, b);
    if (a == 0 && b == 0) return std::signbit(a) ? b : a;  // +0 vence
    return a > b ? a : b;
}

template <typename T>
inline T FpMin(T a, T b) {
    if (IsNaN(a) || IsNaN(b)) return NaN2(a, b);
    if (a == 0 && b == 0) return std::signbit(a) ? a : b;  // -0 vence
    return a < b ? a : b;
}

// FMAXNM/FMINNM: um NaN silencioso contra um numero da o numero
template <typename T>
inline T FpMaxNum(T a, T b) {
    const bool qa = IsNaN(a) && !IsSNaN(a), qb = IsNaN(b) && !IsSNaN(b);
    if (qa && !qb) a = -std::numeric_limits<T>::infinity();
    else if (!qa && qb) b = -std::numeric_limits<T>::infinity();
    return FpMax(a, b);
}

template <typename T>
inline T FpMinNum(T a, T b) {
    const bool qa = IsNaN(a) && !IsSNaN(a), qb = IsNaN(b) && !IsSNaN(b);
    if (qa && !qb) a = std::numeric_limits<T>::infinity();
    else if (!qa && qb) b = std::numeric_limits<T>::infinity();
    return FpMin(a, b);
}

// FPMulAdd: addend + a * b com um arredondamento so
template <typename T>
inline T FpMulAdd(T addend, T a, T b) {
    const T r = std::fma(a, b, addend);
    if (!IsNaN(r)) return r;
    if (IsNaN(addend) && !IsSNaN(addend) && InfTimesZero(a, b)) return DefaultNaN<T>();
    return NaN3(addend, a, b);
}

// FRECPS: 2 - a * b (fundido); FRSQRTS: (3 - a * b) / 2
template <typename T>
inline T FpRecipStep(T a, T b) {
    const T na = FpNeg(a);
    if (IsNaN(na) || IsNaN(b)) return NaN2(na, b);
    if (InfTimesZero(na, b)) return T{2};
    if (std::isinf(na) || std::isinf(b)) return std::signbit(na) != std::signbit(b) ? -std::numeric_limits<T>::infinity() : std::numeric_limits<T>::infinity();
    return std::fma(na, b, T{2});
}

template <typename T>
inline T FpRsqrtStep(T a, T b) {
    const T na = FpNeg(a);
    if (IsNaN(na) || IsNaN(b)) return NaN2(na, b);
    if (InfTimesZero(na, b)) return T{1.5};
    if (std::isinf(na) || std::isinf(b)) return std::signbit(na) != std::signbit(b) ? -std::numeric_limits<T>::infinity() : std::numeric_limits<T>::infinity();
    // (3 + na * b) / 2 com um arredondamento: dividir um operando por 2 (exato fora dos subnormais) evita que o
    // produto estoure antes da metade
    if (std::fabs(b) >= 2 * std::numeric_limits<T>::min()) return std::fma(na, b * T{0.5}, T{1.5});
    if (std::fabs(na) >= 2 * std::numeric_limits<T>::min()) return std::fma(na * T{0.5}, b, T{1.5});
    return std::fma(na, b, T{3}) * T{0.5};
}

template <typename T>
inline T FpSqrt(T a) {
    const T r = std::sqrt(a);
    return IsNaN(r) ? NaN1(a) : r;
}

// Modos (= a64::Rounding): 0 mais proximo (empate par), 1 +inf, 2 -inf, 3 zero, 4 empate longe do zero, 5 FPCR (= 0)
template <int Mode, typename T>
inline T FpRoundInt(T a) {
    if (IsNaN(a)) return Quiet(a);
    if constexpr (Mode == 1) return std::ceil(a);
    else if constexpr (Mode == 2) return std::floor(a);
    else if constexpr (Mode == 3) return std::trunc(a);
    else if constexpr (Mode == 4) return std::round(a);
    else return std::nearbyint(a);
}

template <typename T>
inline T Pow2(int n) {
    T r = 1;
    for (int i = 0; i < n; ++i) r *= 2;
    for (int i = 0; i > n; --i) r /= 2;
    return r;
}

// FPToFixed: NaN da 0, fora da faixa satura
template <typename I, int Mode, unsigned Fbits = 0, typename T>
inline I FpToInt(T a) {
    if (IsNaN(a)) return 0;
    const T x = Fbits ? a * Pow2<T>(Fbits) : a;
    const T r = FpRoundInt<Mode>(x);
    constexpr int bits = sizeof(I) * 8 - (std::is_signed_v<I> ? 1 : 0);
    const T limit = Pow2<T>(bits);
    if (r >= limit) return std::numeric_limits<I>::max();
    if constexpr (std::is_signed_v<I>) {
        if (r < -limit) return std::numeric_limits<I>::min();
    } else {
        if (r < 0) return 0;
    }
    return static_cast<I>(r);
}

// FixedToFP: a conversao do host arredonda uma vez; a escala por potencia de 2 e exata
template <typename T, unsigned Fbits = 0, typename I>
inline T IntToFp(I v) {
    const T r = static_cast<T>(v);
    return Fbits ? r / Pow2<T>(Fbits) : r;
}

inline void FpCompareFlags(Context& c, bool unordered, bool equal, bool less) {
    SetNzcv(c, unordered ? 0b0011 : equal ? 0b0110 : less ? 0b1000 : 0b0010);
}

template <typename T>
inline void FpCompare(Context& c, T a, T b) {
    FpCompareFlags(c, IsNaN(a) || IsNaN(b), a == b, a < b);
}

// ---- Meia precisao (IEEE, AHP = 0) e FCVT ----

template <typename T>
inline uint16_t ToHalf(T v) {
    using U = FpBits<T>;
    constexpr int width = sizeof(T) * 8, mbits = sizeof(T) == 4 ? 23 : 52, bias = sizeof(T) == 4 ? 127 : 1023;
    const U b = std::bit_cast<U>(v);
    const uint16_t sign = static_cast<uint16_t>((b >> (width - 16)) & 0x8000);
    const U abs = b & ~(U{1} << (width - 1));
    const U mant_mask = (U{1} << mbits) - 1;
    const U exp_field = abs >> mbits;
    if (exp_field == (U{1} << (width - 1 - mbits)) - 1) {
        if (abs & mant_mask) return static_cast<uint16_t>(sign | 0x7E00 | ((abs >> (mbits - 10)) & 0x1FF));  // NaN silenciado
        return static_cast<uint16_t>(sign | 0x7C00);
    }
    if (exp_field == 0) return sign;  // subnormais de float/double ficam muito abaixo da meia precisao
    const int e = static_cast<int>(exp_field) - bias;
    const U m = (abs & mant_mask) | (U{1} << mbits);
    if (e >= -14) {
        if (e > 15) return static_cast<uint16_t>(sign | 0x7C00);
        uint32_t h = (static_cast<uint32_t>(e + 15) << 10) | static_cast<uint32_t>((m >> (mbits - 10)) & 0x3FF);
        const U rem = m & ((U{1} << (mbits - 10)) - 1), half = U{1} << (mbits - 11);
        if (rem > half || (rem == half && (h & 1))) ++h;
        if (h >= 0x7C00) h = 0x7C00;
        return static_cast<uint16_t>(sign | h);
    }
    if (e < -25) return sign;
    const unsigned shift = static_cast<unsigned>(mbits - 24 - e);
    U hm = m >> shift;
    const U rem = m & ((U{1} << shift) - 1), half = U{1} << (shift - 1);
    if (rem > half || (rem == half && (hm & 1))) ++hm;
    return static_cast<uint16_t>(sign | hm);
}

template <typename T>
inline T FromHalf(uint16_t h) {
    const bool sign = h >> 15;
    const unsigned exp = (h >> 10) & 0x1F, mant = h & 0x3FF;
    T r;
    if (exp == 31) {
        if (mant == 0) {
            r = std::numeric_limits<T>::infinity();
        } else {
            using U = FpBits<T>;
            constexpr int mbits = sizeof(T) == 4 ? 23 : 52;
            const U bits = (std::bit_cast<U>(std::numeric_limits<T>::infinity())) | kQuietBit<T> | (U{mant} << (mbits - 10));
            r = std::bit_cast<T>(bits);
        }
    } else if (exp == 0) {
        r = static_cast<T>(mant) / Pow2<T>(24);
    } else {
        r = static_cast<T>(mant | 0x400) * Pow2<T>(static_cast<int>(exp) - 25);
    }
    return sign ? FpNeg(r) : r;
}

// FCVT entre precisoes; meia precisao representada por uint16_t
template <typename D, typename S>
inline D ConvertFp(S v) {
    if constexpr (std::is_same_v<D, uint16_t>) return ToHalf(v);
    else if constexpr (std::is_same_v<S, uint16_t>) return FromHalf<D>(v);
    else return IsNaN(v) ? static_cast<D>(Quiet(v)) : static_cast<D>(v);
}

// ---- FRECPE / FRSQRTE (ARMv8.0, sem FEAT_RPRES) ----

inline unsigned RecipEstimate(unsigned a) {
    a = a * 2 + 1;
    const unsigned b = (1u << 19) / a;
    return (b + 1) / 2;
}

inline unsigned RecipSqrtEstimate(unsigned a) {
    if (a < 256) {
        a = a * 2 + 1;
    } else {
        a = (a >> 1) << 1;
        a = (a + 1) * 2;
    }
    unsigned b = 512;
    while (uint64_t{a} * (b + 1) * (b + 1) < (uint64_t{1} << 28)) ++b;
    return (b + 1) / 2;
}

// Fracao normalizada em 52 bits e expoente com vies, como no pseudocodigo
template <typename T>
inline void UnpackFraction(T v, uint64_t& fraction, int& exp) {
    const FpBits<T> bits = std::bit_cast<FpBits<T>>(v);
    if constexpr (sizeof(T) == 4) {
        fraction = uint64_t{bits & 0x7FFFFFu} << 29;
        exp = static_cast<int>((bits >> 23) & 0xFF);
    } else {
        fraction = bits & 0xFFFFFFFFFFFFFull;
        exp = static_cast<int>((bits >> 52) & 0x7FF);
    }
}

template <typename T>
inline T FpRecipEstimate(T v) {
    constexpr bool single = sizeof(T) == 4;
    if (IsNaN(v)) return Quiet(v);
    const bool sign = std::signbit(v);
    if (std::isinf(v)) return sign ? T{-0.0} : T{0.0};
    if (v == 0) return sign ? -std::numeric_limits<T>::infinity() : std::numeric_limits<T>::infinity();
    if (std::fabs(v) < (single ? static_cast<T>(0x1p-128) : static_cast<T>(0x1p-1024))) {
        return sign ? -std::numeric_limits<T>::infinity() : std::numeric_limits<T>::infinity();
    }
    uint64_t fraction;
    int exp;
    UnpackFraction(v, fraction, exp);
    if (exp == 0) {
        if (!((fraction >> 51) & 1)) {
            exp = -1;
            fraction = (fraction & ((uint64_t{1} << 50) - 1)) << 2;
        } else {
            fraction = (fraction & ((uint64_t{1} << 51) - 1)) << 1;
        }
    }
    const unsigned scaled = 0x100 | static_cast<unsigned>((fraction >> 44) & 0xFF);
    int result_exp = (single ? 253 : 2045) - exp;
    const unsigned estimate = RecipEstimate(scaled);
    fraction = uint64_t{estimate & 0xFF} << 44;
    if (result_exp == 0) {
        fraction = (uint64_t{1} << 51) | (fraction >> 1);
    } else if (result_exp == -1) {
        fraction = (uint64_t{1} << 50) | (fraction >> 2);
        result_exp = 0;
    }
    if constexpr (single) {
        return std::bit_cast<float>((uint32_t{sign} << 31) | ((static_cast<uint32_t>(result_exp) & 0xFF) << 23) | static_cast<uint32_t>(fraction >> 29));
    } else {
        return std::bit_cast<double>((uint64_t{sign} << 63) | ((static_cast<uint64_t>(result_exp) & 0x7FF) << 52) | fraction);
    }
}

template <typename T>
inline T FpRsqrtEstimate(T v) {
    constexpr bool single = sizeof(T) == 4;
    if (IsNaN(v)) return Quiet(v);
    if (v == 0) return std::signbit(v) ? -std::numeric_limits<T>::infinity() : std::numeric_limits<T>::infinity();
    if (std::signbit(v)) return DefaultNaN<T>();
    if (std::isinf(v)) return T{0};
    uint64_t fraction;
    int exp;
    UnpackFraction(v, fraction, exp);
    constexpr uint64_t mask = (uint64_t{1} << 52) - 1;
    if (exp == 0) {
        while (!((fraction >> 51) & 1)) {
            fraction = (fraction << 1) & mask;
            --exp;
        }
        fraction = (fraction << 1) & mask;
    }
    const unsigned scaled = (exp & 1) == 0 ? 0x100 | static_cast<unsigned>((fraction >> 44) & 0xFF) : 0x80 | static_cast<unsigned>((fraction >> 45) & 0x7F);
    const int result_exp = ((single ? 380 : 3068) - exp) / 2;
    const unsigned estimate = RecipSqrtEstimate(scaled);
    if constexpr (single) {
        return std::bit_cast<float>(((static_cast<uint32_t>(result_exp) & 0xFF) << 23) | ((estimate & 0xFF) << 15));
    } else {
        return std::bit_cast<double>(((static_cast<uint64_t>(result_exp) & 0x7FF) << 52) | (uint64_t{estimate & 0xFF} << 44));
    }
}

// ---- LD1-LD4 / ST1-ST4 ----

// Estruturas multiplas: regs registradores a partir de t, selem elementos intercalados (LD1 com varios
// registradores tem selem = 1 e repete)
template <typename E>
inline void LoadStructs(Context& c, uint64_t address, unsigned t, unsigned regs, unsigned selem, bool q) {
    const unsigned lanes = (q ? 16 : 8) / sizeof(E);
    const unsigned rpt = selem == 1 ? regs : 1;
    for (unsigned r = 0; r < regs; ++r) {
        if (!q) std::memset(c.vreg[(t + r) % 32].b + 8, 0, 8);
    }
    for (unsigned r = 0; r < rpt; ++r) {
        for (unsigned e = 0; e < lanes; ++e) {
            for (unsigned s = 0; s < selem; ++s) {
                SetLane(c.vreg[(t + r + s) % 32], e, Load<E>(address));
                address += sizeof(E);
            }
        }
    }
}

template <typename E>
inline void StoreStructs(Context& c, uint64_t address, unsigned t, unsigned regs, unsigned selem, bool q) {
    const unsigned lanes = (q ? 16 : 8) / sizeof(E);
    const unsigned rpt = selem == 1 ? regs : 1;
    for (unsigned r = 0; r < rpt; ++r) {
        for (unsigned e = 0; e < lanes; ++e) {
            for (unsigned s = 0; s < selem; ++s) {
                Store<E>(address, Lane<E>(c.vreg[(t + r + s) % 32], e));
                address += sizeof(E);
            }
        }
    }
}

template <typename E>
inline void LoadLane(Context& c, uint64_t address, unsigned t, unsigned selem, unsigned index) {
    for (unsigned s = 0; s < selem; ++s) SetLane(c.vreg[(t + s) % 32], index, Load<E>(address + s * sizeof(E)));
}

template <typename E>
inline void StoreLane(Context& c, uint64_t address, unsigned t, unsigned selem, unsigned index) {
    for (unsigned s = 0; s < selem; ++s) Store<E>(address + s * sizeof(E), Lane<E>(c.vreg[(t + s) % 32], index));
}

template <typename E>
inline void LoadReplicate(Context& c, uint64_t address, unsigned t, unsigned selem, bool q) {
    const unsigned lanes = (q ? 16 : 8) / sizeof(E);
    for (unsigned s = 0; s < selem; ++s) {
        const E e = Load<E>(address + s * sizeof(E));
        V128 r{};
        for (unsigned i = 0; i < lanes; ++i) SetLane(r, i, e);
        c.vreg[(t + s) % 32] = r;
    }
}

} // namespace recomp
