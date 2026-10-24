// Testes unitarios das bordas de ponto flutuante e semantica SIMD/FP (recomp/simd.hpp)

#include "recomp/simd.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    std::printf("%s %s\n", condition ? "[PASS]" : "[FAIL]", what);
    if (!condition) ++g_failures;
}

// 1. Meia precisao, ida e volta exaustiva
void TestHalfExhaustive() {
    int passed = 0;
    for (uint32_t i = 0; i <= 0xFFFF; ++i) {
        const uint16_t h = static_cast<uint16_t>(i);
        const unsigned exp = (h >> 10) & 0x1F;
        const unsigned frac = h & 0x3FF;
        const bool is_nan = (exp == 0x1F && frac != 0);

        const uint16_t h_f32 = recomp::ToHalf(recomp::FromHalf<float>(h));
        const uint16_t h_f64 = recomp::ToHalf(recomp::FromHalf<double>(h));

        if (!is_nan) {
            if (h_f32 == h && h_f64 == h) {
                ++passed;
            } else {
                std::printf("  [FAIL detail] h=0x%04x: f32=0x%04x, f64=0x%04x (esperado 0x%04x)\n", h, h_f32, h_f64, h);
            }
        } else {
            const uint16_t expected = static_cast<uint16_t>(h | 0x0200);
            if (h_f32 == expected && h_f64 == expected) {
                ++passed;
            } else {
                std::printf("  [FAIL detail] h=0x%04x (NaN): f32=0x%04x, f64=0x%04x (esperado 0x%04x)\n", h, h_f32, h_f64, expected);
            }
        }
    }
    std::printf("  Meia precisao exaustiva: %d / 65536 valores passaram\n", passed);
    Check(passed == 65536, "Meia precisao, ida e volta exaustiva (65536 valores)");
}

// 2. Arredondamento para meia (ToHalf)
void TestHalfRounding() {
    bool ok = true;
    auto check_case = [&](float val, uint16_t expected, const char* name) {
        const uint16_t got = recomp::ToHalf(val);
        if (got != expected) {
            std::printf("  [FAIL detail] %s: obtido 0x%04x, esperado 0x%04x\n", name, got, expected);
            ok = false;
        }
    };

    check_case(65504.0f, 0x7BFF, "65504.0f -> 0x7BFF");
    check_case(65520.0f, 0x7C00, "65520.0f -> 0x7C00");
    check_case(-65520.0f, 0xFC00, "-65520.0f -> 0xFC00");
    check_case(0x1p-24f, 0x0001, "0x1p-24f -> 0x0001");
    check_case(0x1p-25f, 0x0000, "0x1p-25f -> 0x0000 (empate par)");
    check_case(0x1.8p-25f, 0x0001, "0x1.8p-25f -> 0x0001");
    check_case(1.0f + 0x1p-11f, 0x3C00, "1.0f + 0x1p-11f -> 0x3C00 (empate par)");
    check_case(1.0f + 0x1.8p-10f, 0x3C02, "1.0f + 0x1.8p-10f -> 0x3C02");
    check_case(-0.0f, 0x8000, "-0.0f -> 0x8000");

    Check(ok, "Arredondamento para meia (ToHalf)");
}

// 3. Conversao FP -> inteiro (FpToInt)
void TestFpToInt() {
    bool ok = true;
    const float nan_f = std::numeric_limits<float>::quiet_NaN();

    auto check_val = [&](auto got, auto expected, const char* name) {
        if (got != expected) {
            std::printf("  [FAIL detail] %s: obtido %lld (0x%llx), esperado %lld (0x%llx)\n",
                        name, static_cast<long long>(got), static_cast<unsigned long long>(got),
                        static_cast<long long>(expected), static_cast<unsigned long long>(expected));
            ok = false;
        }
    };

    check_val(recomp::FpToInt<int32_t, 3>(3e9f), INT32_MAX, "FpToInt<int32_t, 3>(3e9f)");
    check_val(recomp::FpToInt<int32_t, 3>(-3e9f), INT32_MIN, "FpToInt<int32_t, 3>(-3e9f)");
    check_val(recomp::FpToInt<int32_t, 3>(nan_f), int32_t{0}, "FpToInt<int32_t, 3>(NaN)");
    check_val(recomp::FpToInt<uint32_t, 3>(-1.0f), uint32_t{0}, "FpToInt<uint32_t, 3>(-1.0f)");
    check_val(recomp::FpToInt<uint64_t, 3>(1e30), UINT64_MAX, "FpToInt<uint64_t, 3>(1e30)");

    check_val(recomp::FpToInt<int32_t, 0>(2.5f), int32_t{2}, "FpToInt<int32_t, 0>(2.5f)");
    check_val(recomp::FpToInt<int32_t, 0>(3.5f), int32_t{4}, "FpToInt<int32_t, 0>(3.5f)");
    check_val(recomp::FpToInt<int32_t, 4>(2.5f), int32_t{3}, "FpToInt<int32_t, 4>(2.5f)");
    check_val(recomp::FpToInt<int32_t, 4>(-2.5f), int32_t{-3}, "FpToInt<int32_t, 4>(-2.5f)");
    check_val(recomp::FpToInt<int32_t, 2>(-0.5f), int32_t{-1}, "FpToInt<int32_t, 2>(-0.5f)");
    check_val(recomp::FpToInt<int32_t, 1>(0.2f), int32_t{1}, "FpToInt<int32_t, 1>(0.2f)");

    check_val((recomp::FpToInt<int32_t, 3, 8>(1.5f)), int32_t{384}, "FpToInt<int32_t, 3, 8>(1.5f)");

    Check(ok, "Conversao FP -> inteiro (FpToInt)");
}

// 4. FMAX/FMIN e zeros/NaN (float)
void TestFpMaxMin() {
    bool ok = true;
    const float qnan = std::bit_cast<float>(0x7FC00000u);
    const float snan = std::bit_cast<float>(0x7FA00001u);

    auto check_bits = [&](float val, uint32_t expected, const char* name) {
        const uint32_t got = std::bit_cast<uint32_t>(val);
        if (got != expected) {
            std::printf("  [FAIL detail] %s: obtido 0x%08x, esperado 0x%08x\n", name, got, expected);
            ok = false;
        }
    };

    check_bits(recomp::FpMax(-0.0f, 0.0f), 0x00000000u, "FpMax(-0.0f, 0.0f)");
    check_bits(recomp::FpMax(0.0f, -0.0f), 0x00000000u, "FpMax(0.0f, -0.0f)");
    check_bits(recomp::FpMin(-0.0f, 0.0f), 0x80000000u, "FpMin(-0.0f, 0.0f)");
    check_bits(recomp::FpMin(0.0f, -0.0f), 0x80000000u, "FpMin(0.0f, -0.0f)");

    const float max_qnan = recomp::FpMax(qnan, 1.0f);
    if (!std::isnan(max_qnan)) {
        std::printf("  [FAIL detail] FpMax(qNaN, 1.0f): esperado NaN, obtido 0x%08x\n", std::bit_cast<uint32_t>(max_qnan));
        ok = false;
    }

    check_bits(recomp::FpMaxNum(qnan, 1.0f), std::bit_cast<uint32_t>(1.0f), "FpMaxNum(qNaN, 1.0f)");
    check_bits(recomp::FpMinNum(1.0f, qnan), std::bit_cast<uint32_t>(1.0f), "FpMinNum(1.0f, qNaN)");

    check_bits(recomp::FpMax(snan, 1.0f), 0x7FE00001u, "FpMax(sNaN 0x7FA00001, 1.0f)");
    check_bits(recomp::FpMaxNum(qnan, snan), 0x7FE00001u, "FpMaxNum(qNaN, sNaN 0x7FA00001)");

    Check(ok, "FMAX/FMIN e zeros/NaN (float)");
}

// 5. Propagacao de NaN (float)
void TestNanPropagation() {
    bool ok = true;
    const float qnan1 = std::bit_cast<float>(0x7FC00001u);
    const float qnan2 = std::bit_cast<float>(0x7FC00002u);
    const float snan2 = std::bit_cast<float>(0x7F800002u);
    const float inf_pos = std::numeric_limits<float>::infinity();
    const float inf_neg = -std::numeric_limits<float>::infinity();

    auto check_bits = [&](float val, uint32_t expected, const char* name) {
        const uint32_t got = std::bit_cast<uint32_t>(val);
        if (got != expected) {
            std::printf("  [FAIL detail] %s: obtido 0x%08x, esperado 0x%08x\n", name, got, expected);
            ok = false;
        }
    };

    check_bits(recomp::FpAdd(qnan1, qnan2), 0x7FC00001u, "FpAdd(qNaN 0x7FC00001, qNaN 0x7FC00002)");
    check_bits(recomp::FpAdd(qnan1, snan2), 0x7FC00002u, "FpAdd(qNaN 0x7FC00001, sNaN 0x7F800002)");
    check_bits(recomp::FpAdd(inf_pos, inf_neg), 0x7FC00000u, "FpAdd(+inf, -inf)");
    check_bits(recomp::FpMul(0.0f, inf_pos), 0x7FC00000u, "FpMul(0.0f, inf)");

    Check(ok, "Propagacao de NaN (float)");
}

// 6. FMA fundida
void TestFusedFma() {
    bool ok = true;
    const float addend = -(1.0f + 0x1p-11f);
    const float a = 1.0f + 0x1p-12f;
    const float b = 1.0f + 0x1p-12f;
    const float res = recomp::FpMulAdd(addend, a, b);

    if (res != 0x1p-24f) {
        std::printf("  [FAIL detail] FpMulAdd(-(1.0f + 0x1p-11f), ...): obtido 0x%08x, esperado 0x%08x (0x1p-24f)\n",
                    std::bit_cast<uint32_t>(res), std::bit_cast<uint32_t>(0x1p-24f));
        ok = false;
    }

    const float qnan = std::bit_cast<float>(0x7FC00000u);
    const float inf_pos = std::numeric_limits<float>::infinity();
    const float res2 = recomp::FpMulAdd(qnan, 0.0f, inf_pos);
    const uint32_t res2_bits = std::bit_cast<uint32_t>(res2);

    if (res2_bits != 0x7FC00000u) {
        std::printf("  [FAIL detail] FpMulAdd(qNaN, 0.0f, inf): obtido 0x%08x, esperado 0x7FC00000\n", res2_bits);
        ok = false;
    }

    Check(ok, "FMA fundida (FpMulAdd)");
}

// 7. Estimativas
void TestEstimates() {
    bool ok = true;
    auto check_bits = [&](float val, uint32_t expected, const char* name) {
        const uint32_t got = std::bit_cast<uint32_t>(val);
        if (got != expected) {
            std::printf("  [FAIL detail] %s: obtido 0x%08x, esperado 0x%08x\n", name, got, expected);
            ok = false;
        }
    };

    check_bits(recomp::FpRecipEstimate(1.0f), 0x3F7F8000u, "FpRecipEstimate(1.0f)");
    check_bits(recomp::FpRsqrtEstimate(1.0f), 0x3F7F8000u, "FpRsqrtEstimate(1.0f)");
    check_bits(recomp::FpRsqrtEstimate(4.0f), 0x3EFF8000u, "FpRsqrtEstimate(4.0f)");

    const float rec_zero = recomp::FpRecipEstimate(0.0f);
    if (!std::isinf(rec_zero) || std::signbit(rec_zero)) {
        std::printf("  [FAIL detail] FpRecipEstimate(0.0f): esperado +inf, obtido 0x%08x\n", std::bit_cast<uint32_t>(rec_zero));
        ok = false;
    }

    check_bits(recomp::FpRsqrtEstimate(-1.0f), 0x7FC00000u, "FpRsqrtEstimate(-1.0f)");

    Check(ok, "Estimativas (FpRecipEstimate / FpRsqrtEstimate)");
}

} // namespace

int main() {
    TestHalfExhaustive();
    TestHalfRounding();
    TestFpToInt();
    TestFpMaxMin();
    TestNanPropagation();
    TestFusedFma();
    TestEstimates();
    std::printf("%s (%d falhas)\n", g_failures == 0 ? "OK" : "FALHOU", g_failures);
    return g_failures == 0 ? 0 : 1;
}
