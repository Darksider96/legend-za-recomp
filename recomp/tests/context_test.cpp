// Testes dos helpers semanticos do runtime (recomp/context.hpp) contra resultados conhecidos dos aliases do A64

#include "recomp/context.hpp"

#include <cstdio>

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    std::printf("%s %s\n", condition ? "[PASS]" : "[FAIL]", what);
    if (!condition) ++g_failures;
}

using recomp::Bitfield;

void TestBitfield() {
    const uint64_t x = 0xF123456789ABCDEFull;
    // LSL #8 = UBFM immr = 56, imms = 55; LSR #8 = UBFM immr = 8, imms = 63; ASR #8 = SBFM immr = 8, imms = 63
    Check(Bitfield<uint64_t>(0, x, 56, 55, false, false) == (x << 8), "UBFM como LSL #8 (64 bits)");
    Check(Bitfield<uint64_t>(0, x, 8, 63, false, false) == (x >> 8), "UBFM como LSR #8 (64 bits)");
    Check(Bitfield<uint64_t>(0, x, 8, 63, true, false) == static_cast<uint64_t>(static_cast<int64_t>(x) >> 8), "SBFM como ASR #8 (64 bits)");
    // UBFX x, #4, #12 = UBFM immr = 4, imms = 15; SBFX com o bit de sinal do campo ligado
    Check(Bitfield<uint64_t>(0, x, 4, 15, false, false) == ((x >> 4) & 0xFFF), "UBFX #4, #12");
    Check(Bitfield<uint32_t>(0, 0x00000F00u, 8, 11, true, false) == 0xFFFFFFFFu, "SBFX de campo negativo estende o sinal (32 bits)");
    // SXTW = SBFM immr = 0, imms = 31; UXTB = UBFM immr = 0, imms = 7 (32 bits)
    Check(Bitfield<uint64_t>(0, 0x80000000ull, 0, 31, true, false) == 0xFFFFFFFF80000000ull, "SXTW");
    Check(Bitfield<uint32_t>(0, 0x12345678u, 0, 7, false, false) == 0x78u, "UXTB");
    // BFI w0, w1, #8, #4 = BFM immr = 24, imms = 3; BFXIL w0, w1, #4, #8 = BFM immr = 4, imms = 11
    Check(Bitfield<uint32_t>(0xFFFFFFFFu, 0x5u, 24, 3, false, true) == 0xFFFFF5FFu, "BFI #8, #4 preserva os demais bits");
    Check(Bitfield<uint32_t>(0xAAAAAAAAu, 0x00000AB0u, 4, 11, false, true) == 0xAAAAAAABu, "BFXIL #4, #8");
    // SBFIZ x, #16, #8 = SBFM immr = 48, imms = 7 com campo negativo
    Check(Bitfield<uint64_t>(0, 0x80ull, 48, 7, true, false) == 0xFFFFFFFFFF800000ull, "SBFIZ #16, #8 estende o sinal");
}

void TestFlags() {
    recomp::Context c;
    // CMP 5, 5: Z=1 C=1 (sem emprestimo); CMP 3, 5: N=1 C=0; CMP INT_MIN, 1: V=1
    recomp::AddWithCarry<uint32_t>(c, 5u, ~5u, true, true);
    const bool eq = c.z && c.c && !c.n && !c.v;
    recomp::AddWithCarry<uint32_t>(c, 3u, ~5u, true, true);
    const bool lt = c.n && !c.c && !c.z && !c.v;
    recomp::AddWithCarry<uint32_t>(c, 0x80000000u, ~1u, true, true);
    const bool overflow = c.v && !c.n;
    Check(eq && lt && overflow, "AddWithCarry: flags de CMP (igual, menor, overflow)");
    recomp::AddWithCarry<uint64_t>(c, ~0ull, 1ull, false, true);  // ADDS -1 + 1: C=1, Z=1
    Check(c.c && c.z && !c.v, "ADDS 0xFFFF...F + 1 gera carry e zero");
    recomp::SetNzcv(c, 0b1001);
    Check(recomp::Condition(c, 0xB) == false && recomp::Condition(c, 0xA) && recomp::Condition(c, 0xE) && recomp::Condition(c, 0xF),
          "Condition: N=1 V=1 -> GE verdadeiro, LT falso; AL e NV sempre verdadeiros");
}

void TestMisc() {
    Check(recomp::SignedDivide<uint32_t>(0x80000000u, 0xFFFFFFFFu) == 0x80000000u && recomp::UnsignedDivide<uint64_t>(7, 0) == 0,
          "SDIV INT_MIN / -1 = INT_MIN e divisao por zero = 0");
    Check(recomp::ReverseBytes<uint64_t>(0x0102030405060708ull, 8) == 0x0807060504030201ull &&
              recomp::ReverseBytes<uint64_t>(0x0102030405060708ull, 4) == 0x0403020108070605ull &&
              recomp::ReverseBytes<uint32_t>(0x11223344u, 2) == 0x22114433u,
          "REV / REV32 / REV16");
    Check(recomp::ReverseBits<uint32_t>(1u) == 0x80000000u && recomp::CountLeadingSignBits<uint32_t>(0xFFFF0000u) == 15u &&
              recomp::CountLeadingSignBits<uint64_t>(1ull) == 62u,
          "RBIT e CLS");
    Check(recomp::Extract<uint64_t>(0x1ull, 0x8000000000000000ull, 63) == 0x3ull && recomp::MulHighUnsigned(~0ull, ~0ull) == ~0ull - 1 &&
              recomp::MulHighSigned(static_cast<uint64_t>(-1ll), 5) == ~0ull,
          "EXTR, UMULH e SMULH");
}

} // namespace

int main() {
    TestBitfield();
    TestFlags();
    TestMisc();
    std::printf("%s (%d falhas)\n", g_failures == 0 ? "OK" : "FALHOU", g_failures);
    return g_failures == 0 ? 0 : 1;
}
