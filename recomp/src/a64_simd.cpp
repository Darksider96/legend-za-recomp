// Decodificador A64 — SIMD/FP (Advanced SIMD e ponto flutuante do ARMv8.0)
//
// Campos usados (ver Insn): size = log2 do elemento (ou tipo FP: 1 meia, 2 simples, 3 dupla), q = 128 bits (ou a
// variante "2" das formas longas/estreitas), scalar = so o elemento 0, element/index = forma por elemento,
// amount = deslocamento imediato, fsize = tipo de origem do FCVT ou fbits das conversoes de ponto fixo.
// FP16 aritmetico (v8.2), criptografia, saturacoes raras e polinomiais ficam como Op::Unknown.

#include "recomp/a64.hpp"

namespace recomp::a64 {

namespace {

constexpr uint32_t Bits(uint32_t w, unsigned hi, unsigned lo) { return (w >> lo) & ((1u << (hi - lo + 1)) - 1); }
constexpr bool Bit(uint32_t w, unsigned b) { return (w >> b) & 1; }
constexpr uint8_t Reg(uint32_t w, unsigned lo) { return static_cast<uint8_t>(Bits(w, lo + 4, lo)); }

// VFPExpandImm: imm8 -> padrao de bits do tipo FP (1 meia, 2 simples, 3 dupla)
uint64_t VfpExpandImm(uint32_t imm8, unsigned type) {
    const unsigned e = type == 1 ? 5 : type == 2 ? 8 : 11, f = type == 1 ? 10 : type == 2 ? 23 : 52;
    const uint64_t sign = (imm8 >> 7) & 1, b6 = (imm8 >> 6) & 1;
    const uint64_t exp = ((b6 ^ 1) << (e - 1)) | ((b6 ? (uint64_t{1} << (e - 3)) - 1 : 0) << 2) | ((imm8 >> 4) & 3);
    const uint64_t frac = uint64_t{imm8 & 0xF} << (f - 4);
    return (sign << (e + f)) | (exp << f) | frac;
}

// AdvSIMDExpandImm
uint64_t SimdExpandImm(bool op, uint32_t cmode, uint32_t imm8) {
    const uint64_t i = imm8;
    const auto rep32 = [](uint64_t v) { return v | (v << 32); };
    const auto rep16 = [](uint64_t v) { return v | (v << 16) | (v << 32) | (v << 48); };
    switch (cmode >> 1) {
    case 0: return rep32(i);
    case 1: return rep32(i << 8);
    case 2: return rep32(i << 16);
    case 3: return rep32(i << 24);
    case 4: return rep16(i);
    case 5: return rep16(i << 8);
    case 6: return (cmode & 1) ? rep32((i << 16) | 0xFFFF) : rep32((i << 8) | 0xFF);
    default:
        if (!(cmode & 1) && !op) return i * 0x0101010101010101ull;
        if (!(cmode & 1) && op) {
            uint64_t out = 0;
            for (unsigned b = 0; b < 8; ++b) out |= ((i >> b) & 1 ? uint64_t{0xFF} : 0) << (8 * b);
            return out;
        }
        if (!op) return rep32(VfpExpandImm(imm8, 2));
        return VfpExpandImm(imm8, 3);
    }
}

Insn ThreeSame(Insn d, uint32_t w, bool scalar) {
    const bool u = Bit(w, 29);
    const uint32_t size = Bits(w, 23, 22), opcode = Bits(w, 15, 11);
    d.scalar = scalar;
    d.size = static_cast<uint8_t>(size);
    if (opcode >= 0b11000) {  // FP: a = size<1>, sz = size<0>
        const bool a = Bit(w, 23);
        d.size = Bit(w, 22) ? 3 : 2;
        if (!scalar && !d.q && d.size == 3) return d;
        static constexpr Op table[2][2][8] = {
            {{Op::FMaxNm, Op::FMla, Op::FAdd, Op::FMulX, Op::FCmEq, Op::Unknown, Op::FMax, Op::FRecpS},
             {Op::FMinNm, Op::FMls, Op::FSub, Op::Unknown, Op::Unknown, Op::Unknown, Op::FMin, Op::FRsqrtS}},
            {{Op::FMaxNmP, Op::Unknown, Op::FAddP, Op::FMul, Op::FCmGe, Op::FAcGe, Op::FMaxP, Op::FDiv},
             {Op::FMinNmP, Op::Unknown, Op::FAbd, Op::Unknown, Op::FCmGt, Op::FAcGt, Op::FMinP, Op::Unknown}},
        };
        const Op op = table[u][a][opcode - 0b11000];
        const bool scalar_ok = op == Op::FMulX || op == Op::FCmEq || op == Op::FRecpS || op == Op::FRsqrtS || op == Op::FCmGe ||
                               op == Op::FAcGe || op == Op::FAbd || op == Op::FCmGt || op == Op::FAcGt;
        if (!scalar || scalar_ok) d.op = op;
        return d;
    }
    if (opcode == 0b00011) {  // logicos sobre bytes
        if (scalar) return d;
        static constexpr Op logic[2][4] = {{Op::VAnd, Op::VBic, Op::VOrr, Op::VOrn}, {Op::VEor, Op::VBsl, Op::VBit, Op::VBif}};
        d.op = logic[u][size];
        d.size = 0;
        return d;
    }
    Op op = Op::Unknown;
    bool needs64 = false;  // escalar so com elementos de 64 bits
    bool vector_only = false;
    switch (opcode) {
    case 0b00001: op = u ? Op::VUQAdd : Op::VSQAdd; break;
    case 0b00101: op = u ? Op::VUQSub : Op::VSQSub; break;
    case 0b00110: op = u ? Op::VCmHi : Op::VCmGt; needs64 = true; break;
    case 0b00111: op = u ? Op::VCmHs : Op::VCmGe; needs64 = true; break;
    case 0b01000: op = u ? Op::VUShl : Op::VSShl; needs64 = true; break;
    case 0b01100: op = u ? Op::VUMax : Op::VSMax; vector_only = true; break;
    case 0b01101: op = u ? Op::VUMin : Op::VSMin; vector_only = true; break;
    case 0b01110: op = u ? Op::VUAbd : Op::VSAbd; vector_only = true; break;
    case 0b10000: op = u ? Op::VSub : Op::VAdd; needs64 = true; break;
    case 0b10001: op = u ? Op::VCmEq : Op::VCmTst; needs64 = true; break;
    case 0b10010: op = u ? Op::VMls : Op::VMla; vector_only = true; break;
    case 0b10011: op = u ? Op::Unknown : Op::VMul; vector_only = true; break;
    case 0b10100: op = u ? Op::VUMaxP : Op::VSMaxP; vector_only = true; break;
    case 0b10101: op = u ? Op::VUMinP : Op::VSMinP; vector_only = true; break;
    case 0b10111: op = u ? Op::Unknown : Op::VAddP; vector_only = true; break;
    default: break;
    }
    if (scalar && (vector_only || (needs64 && size != 3))) return d;
    if (!scalar && size == 3 && !d.q) return d;
    if (size == 3 && (op == Op::VMul || op == Op::VMla || op == Op::VMls || op == Op::VSMax || op == Op::VSMin || op == Op::VUMax ||
                      op == Op::VUMin || op == Op::VSAbd || op == Op::VUAbd || op == Op::VSMaxP || op == Op::VSMinP ||
                      op == Op::VUMaxP || op == Op::VUMinP)) {
        return d;
    }
    d.op = op;
    return d;
}

Insn ThreeDifferent(Insn d, uint32_t w) {
    const bool u = Bit(w, 29);
    const uint32_t size = Bits(w, 23, 22), opcode = Bits(w, 15, 12);
    if (size == 3) return d;
    d.size = static_cast<uint8_t>(size);  // elemento estreito
    switch (opcode) {
    case 0b0000: d.op = u ? Op::VUAddL : Op::VSAddL; break;
    case 0b0001: d.op = u ? Op::VUAddW : Op::VSAddW; break;
    case 0b0010: d.op = u ? Op::VUSubL : Op::VSSubL; break;
    case 0b0011: d.op = u ? Op::VUSubW : Op::VSSubW; break;
    case 0b0100: d.op = u ? Op::Unknown : Op::VAddHn; break;
    case 0b0110: d.op = u ? Op::Unknown : Op::VSubHn; break;
    case 0b0111: d.op = u ? Op::VUAbdl : Op::VSAbdl; break;
    case 0b1000: d.op = u ? Op::VUMlal : Op::VSMlal; break;
    case 0b1010: d.op = u ? Op::VUMlsl : Op::VSMlsl; break;
    case 0b1100: d.op = u ? Op::VUMull : Op::VSMull; break;
    default: break;
    }
    return d;
}

Insn TwoRegMisc(Insn d, uint32_t w, bool scalar) {
    const bool u = Bit(w, 29);
    const uint32_t size = Bits(w, 23, 22), opcode = Bits(w, 16, 12);
    const bool a = Bit(w, 23);
    d.scalar = scalar;
    d.size = static_cast<uint8_t>(size);
    const auto fp = [&](Op op, bool scalar_ok) {
        d.size = Bit(w, 22) ? 3 : 2;
        if (scalar && !scalar_ok) return;
        if (!scalar && !d.q && d.size == 3) return;
        d.op = op;
    };
    const auto conversion = [&](bool to_int, Rounding rounding, bool is_signed) {
        fp(to_int ? Op::FCvtToInt : Op::IntToFp, true);
        d.rounding = rounding;
        d.sign_extend = is_signed;
        d.sf = d.size == 3;
    };
    // FP (a = size<1> separa pares de operacoes)
    if (opcode >= 0b01100 && opcode <= 0b01111 && a) {
        if (opcode == 0b01100) fp(u ? Op::FCmGeZ : Op::FCmGtZ, true);
        else if (opcode == 0b01101) fp(u ? Op::FCmLeZ : Op::FCmEqZ, true);
        else if (opcode == 0b01110 && !u) fp(Op::FCmLtZ, true);
        else if (opcode == 0b01111) fp(u ? Op::FNeg : Op::FAbs, false);
        return d;
    }
    if (opcode >= 0b10110) {
        switch (opcode) {
        case 0b10110:  // FCVTN: f32 -> f16 (sz 0) ou f64 -> f32 (sz 1)
            if (u || a || scalar) return d;
            d.op = Op::FCvtN;
            d.size = Bit(w, 22) ? 2 : 1;
            d.fsize = Bit(w, 22) ? 3 : 2;
            return d;
        case 0b10111:  // FCVTL: f16 -> f32 ou f32 -> f64
            if (u || a || scalar) return d;
            d.op = Op::FCvtL;
            d.size = Bit(w, 22) ? 3 : 2;
            d.fsize = Bit(w, 22) ? 2 : 1;
            return d;
        case 0b11000:
            if (scalar) return d;
            if (!a) {
                fp(Op::FRint, false);
                d.rounding = u ? Rounding::TiesAway : Rounding::Nearest;
            } else if (!u) {
                fp(Op::FRint, false);
                d.rounding = Rounding::PlusInf;
            }
            return d;
        case 0b11001:
            if (scalar) return d;
            fp(Op::FRint, false);
            d.rounding = !a ? (u ? Rounding::Current : Rounding::MinusInf) : (u ? Rounding::Current : Rounding::Zero);
            d.imm = u && !a;  // FRINTX (1) / FRINTI (0): os dois usam o modo do FPCR
            return d;
        case 0b11010: conversion(true, a ? Rounding::PlusInf : Rounding::Nearest, !u); return d;
        case 0b11011: conversion(true, a ? Rounding::Zero : Rounding::MinusInf, !u); return d;
        case 0b11100:
            if (!a) conversion(true, Rounding::TiesAway, !u);
            return d;
        case 0b11101:
            if (!a) conversion(false, Rounding::Nearest, !u);
            else fp(u ? Op::FRsqrtE : Op::FRecpE, true);
            return d;
        case 0b11111:
            if (u && a) fp(Op::FSqrt, false);
            return d;
        default: return d;
        }
    }
    // Inteiros
    Op op = Op::Unknown;
    bool scalar_ok = false;
    switch (opcode) {
    case 0b00000: op = u ? Op::VRev32 : Op::VRev64; break;
    case 0b00001: op = u ? Op::Unknown : Op::VRev16; break;
    case 0b00010: op = u ? Op::VUAddLP : Op::VSAddLP; break;
    case 0b00100: op = u ? Op::VClz : Op::VCls; break;
    case 0b00101: op = !u ? Op::VCnt : size == 0 ? Op::VNot : size == 1 ? Op::VRbit : Op::Unknown; break;
    case 0b01000: op = u ? Op::VCmGeZ : Op::VCmGtZ; scalar_ok = true; break;
    case 0b01001: op = u ? Op::VCmLeZ : Op::VCmEqZ; scalar_ok = true; break;
    case 0b01010: op = u ? Op::Unknown : Op::VCmLtZ; scalar_ok = true; break;
    case 0b01011: op = u ? Op::VNeg : Op::VAbs; scalar_ok = true; break;
    case 0b10010: op = u ? Op::Unknown : Op::VXtn; break;
    default: break;
    }
    if (scalar && (!scalar_ok || size != 3)) return d;
    if (op == Op::VRev64 && size == 3) return d;
    if (op == Op::VRev32 && size >= 2) return d;
    if (op == Op::VRev16 && size != 0) return d;
    if (op == Op::VCnt && size != 0) return d;
    if (op == Op::VXtn || op == Op::VSAddLP || op == Op::VUAddLP || op == Op::VCls || op == Op::VClz) {
        if (size == 3) return d;
    } else if (!scalar && size == 3 && !d.q) {
        return d;
    }
    if (op == Op::VNot || op == Op::VRbit) d.size = 0;
    d.op = op;
    return d;
}

Insn AcrossLanes(Insn d, uint32_t w) {
    const bool u = Bit(w, 29);
    const uint32_t size = Bits(w, 23, 22), opcode = Bits(w, 16, 12);
    d.size = static_cast<uint8_t>(size);
    if (opcode == 0b01100 || opcode == 0b01111) {  // FP: so 4S
        if (!u || Bit(w, 22) || !d.q) return d;
        d.size = 2;
        d.op = opcode == 0b01100 ? (Bit(w, 23) ? Op::FMinNmV : Op::FMaxNmV) : (Bit(w, 23) ? Op::FMinV : Op::FMaxV);
        return d;
    }
    if (size == 3 || (size == 2 && !d.q)) return d;
    switch (opcode) {
    case 0b00011: d.op = u ? Op::VUAddLV : Op::VSAddLV; break;
    case 0b01010: d.op = u ? Op::VUMaxV : Op::VSMaxV; break;
    case 0b11010: d.op = u ? Op::VUMinV : Op::VSMinV; break;
    case 0b11011: d.op = u ? Op::Unknown : Op::VAddV; break;
    default: break;
    }
    return d;
}

Insn ScalarPairwise(Insn d, uint32_t w) {
    const bool u = Bit(w, 29);
    const uint32_t opcode = Bits(w, 16, 12);
    d.scalar = true;
    if (!u) {
        if (opcode == 0b11011 && Bits(w, 23, 22) == 3) {
            d.op = Op::VAddP;
            d.size = 3;
        }
        return d;
    }
    d.size = Bit(w, 22) ? 3 : 2;
    const bool a = Bit(w, 23);
    if (opcode == 0b01100) d.op = a ? Op::FMinNmP : Op::FMaxNmP;
    else if (opcode == 0b01101 && !a) d.op = Op::FAddP;
    else if (opcode == 0b01111) d.op = a ? Op::FMinP : Op::FMaxP;
    return d;
}

// DUP/INS/UMOV/SMOV: imm5 da o tamanho (bit menos significativo ligado) e o indice
Insn Copy(Insn d, uint32_t w, bool scalar) {
    const uint32_t imm5 = Bits(w, 20, 16), imm4 = Bits(w, 14, 11);
    if ((imm5 & 0xF) == 0) return d;
    unsigned size = 0;
    while (!((imm5 >> size) & 1)) ++size;
    d.size = static_cast<uint8_t>(size);
    d.index = static_cast<uint8_t>(imm5 >> (size + 1));
    if (scalar) {
        if (Bit(w, 29) || imm4 != 0) return d;
        d.op = Op::VDupElem;
        d.scalar = true;
        return d;
    }
    if (Bit(w, 29)) {  // INS (elemento)
        if (!d.q) return d;
        d.op = Op::VInsElem;
        d.index2 = static_cast<uint8_t>(imm4 >> size);
        return d;
    }
    switch (imm4) {
    case 0b0000:
        if (size == 3 && !d.q) return d;
        d.op = Op::VDupElem;
        return d;
    case 0b0001:
        if (size == 3 && !d.q) return d;
        d.op = Op::VDupGp;
        d.sf = size == 3;
        return d;
    case 0b0011:
        if (!d.q) return d;
        d.op = Op::VInsGp;
        d.sf = size == 3;
        return d;
    case 0b0101:  // SMOV: W com Q = 0, X com Q = 1
        if (size >= (d.q ? 3u : 2u)) return d;
        d.op = Op::VSMov;
        d.sf = d.q;
        d.sign_extend = true;
        return d;
    case 0b0111:  // UMOV: W para B/H/S, X para D
        if ((size == 3) != d.q) return d;
        d.op = Op::VUMov;
        d.sf = size == 3;
        return d;
    default: return d;
    }
}

Insn ModifiedImmediate(Insn d, uint32_t w) {
    const bool op = Bit(w, 29);
    const uint32_t cmode = Bits(w, 15, 12);
    if (Bit(w, 11)) return d;
    const uint32_t imm8 = (Bits(w, 18, 16) << 5) | Bits(w, 9, 5);
    if (cmode == 0b1111 && op && !d.q) return d;
    uint64_t imm = SimdExpandImm(op, cmode, imm8);
    const bool shifted32 = (cmode & 0b1000) == 0, shifted16 = (cmode & 0b1100) == 0b1000;
    if ((shifted32 || shifted16) && (cmode & 1)) {
        d.op = op ? Op::VBicImm : Op::VOrrImm;
    } else {
        d.op = Op::VMovImm;
        if (op && cmode != 0b1110 && cmode != 0b1111) imm = ~imm;  // MVNI
    }
    d.imm = static_cast<int64_t>(imm);
    return d;
}

Insn ShiftImmediate(Insn d, uint32_t w, bool scalar) {
    const bool u = Bit(w, 29);
    const uint32_t immh = Bits(w, 22, 19), immhb = Bits(w, 22, 16), opcode = Bits(w, 15, 11);
    if (immh == 0) return d;
    unsigned size = 3;
    while (!((immh >> size) & 1)) --size;
    const unsigned esize = 8u << size;
    d.scalar = scalar;
    d.size = static_cast<uint8_t>(size);
    const auto right = [&] { d.amount = static_cast<uint8_t>(2 * esize - immhb); };
    const auto left = [&] { d.amount = static_cast<uint8_t>(immhb - esize); };
    Op op = Op::Unknown;
    enum { Same, Narrow, Long, Fixed } kind = Same;
    switch (opcode) {
    case 0b00000: op = u ? Op::VUShr : Op::VSShr; right(); break;
    case 0b00010: op = u ? Op::VUSra : Op::VSSra; right(); break;
    case 0b00100: op = u ? Op::VURShr : Op::VSRShr; right(); break;
    case 0b01000: op = u ? Op::VSri : Op::Unknown; right(); break;
    case 0b01010: op = u ? Op::VSli : Op::VShl; left(); break;
    case 0b10000: op = u ? Op::Unknown : Op::VShrn; right(); kind = Narrow; break;
    case 0b10001: op = u ? Op::Unknown : Op::VRShrn; right(); kind = Narrow; break;
    case 0b10100: op = u ? Op::VUShll : Op::VSShll; left(); kind = Long; break;
    case 0b11100: op = Op::IntToFp; kind = Fixed; break;
    case 0b11111: op = Op::FCvtToInt; kind = Fixed; break;
    default: break;
    }
    if (op == Op::Unknown) return d;
    switch (kind) {
    case Same:
        if (scalar ? size != 3 : (size == 3 && !d.q)) return d;
        break;
    case Narrow:
    case Long:
        if (scalar || size == 3) return d;
        break;
    case Fixed:
        if (size < 2 || (!scalar && size == 3 && !d.q)) return d;
        d.fsize = static_cast<uint8_t>(2 * esize - immhb);  // fbits
        d.sign_extend = !u;
        d.rounding = op == Op::FCvtToInt ? Rounding::Zero : Rounding::Nearest;
        d.sf = size == 3;
        break;
    }
    d.op = op;
    return d;
}

Insn ByElement(Insn d, uint32_t w, bool scalar) {
    const bool u = Bit(w, 29), l = Bit(w, 21), m = Bit(w, 20), h = Bit(w, 11);
    const uint32_t size = Bits(w, 23, 22), opcode = Bits(w, 15, 12);
    d.scalar = scalar;
    d.element = true;
    const bool fp = opcode == 0b0001 || opcode == 0b0101 || opcode == 0b1001;
    if (fp) {
        if (size < 2) return d;  // 00 e FP16 (v8.2)
        d.size = static_cast<uint8_t>(size);
        if (size == 3 && (l || (!scalar && !d.q))) return d;
        d.index = static_cast<uint8_t>(size == 2 ? (h << 1) | l : h);
        d.rm = static_cast<uint8_t>(Bits(w, 19, 16) | (m << 4));
        d.op = opcode == 0b0001 ? (u ? Op::Unknown : Op::FMla) : opcode == 0b0101 ? (u ? Op::Unknown : Op::FMls) : (u ? Op::FMulX : Op::FMul);
        return d;
    }
    if (scalar || size == 0 || size == 3) return d;
    d.size = static_cast<uint8_t>(size);
    if (size == 1) {
        d.index = static_cast<uint8_t>((h << 2) | (l << 1) | m);
        d.rm = static_cast<uint8_t>(Bits(w, 19, 16));
    } else {
        d.index = static_cast<uint8_t>((h << 1) | l);
        d.rm = static_cast<uint8_t>(Bits(w, 19, 16) | (m << 4));
    }
    switch (opcode) {
    case 0b1000: d.op = u ? Op::Unknown : Op::VMul; break;
    case 0b0000: d.op = u ? Op::VMla : Op::Unknown; break;
    case 0b0100: d.op = u ? Op::VMls : Op::Unknown; break;
    case 0b1010: d.op = u ? Op::VUMull : Op::VSMull; break;
    case 0b0010: d.op = u ? Op::VUMlal : Op::VSMlal; break;
    case 0b0110: d.op = u ? Op::VUMlsl : Op::VSMlsl; break;
    default: break;
    }
    return d;
}

Insn Permute(Insn d, uint32_t w) {
    const uint32_t size = Bits(w, 23, 22);
    if (size == 3 && !d.q) return d;
    d.size = static_cast<uint8_t>(size);
    static constexpr Op ops[8] = {Op::Unknown, Op::VUzp1, Op::VTrn1, Op::VZip1, Op::Unknown, Op::VUzp2, Op::VTrn2, Op::VZip2};
    d.op = ops[Bits(w, 14, 12)];
    return d;
}

// Ponto flutuante escalar (grupo 11110 e 3 fontes 11111)
Insn FloatingPoint(Insn d, uint32_t w) {
    const uint32_t type = Bits(w, 23, 22);
    // type 10 so existe no FMOV Xd <-> Vn.D[1] (conversao com inteiro, rmode 01)
    const bool upper_d = Bits(w, 28, 24) == 0b11110 && Bit(w, 21) && Bits(w, 15, 10) == 0 && Bits(w, 20, 19) == 1;
    if (type == 2 && !upper_d) return d;
    d.scalar = true;
    d.size = static_cast<uint8_t>(type == 0 ? 2 : type == 1 ? 3 : 1);
    const bool half = type == 3;
    if (Bits(w, 28, 24) == 0b11111) {  // FMADD/FMSUB/FNMADD/FNMSUB
        if (Bit(w, 31) || Bit(w, 29) || half) return d;
        d.ra = Reg(w, 10);
        static constexpr Op ops[4] = {Op::FMadd, Op::FMsub, Op::FNMadd, Op::FNMsub};
        d.op = ops[(Bit(w, 21) << 1) | Bit(w, 15)];
        return d;
    }
    if (Bit(w, 29)) return d;
    if (!Bit(w, 21)) {  // conversao com ponto fixo
        const uint32_t rmode = Bits(w, 20, 19), opcode = Bits(w, 18, 16), scale = Bits(w, 15, 10);
        d.sf = Bit(w, 31);
        d.general = true;
        if (half || (!d.sf && scale < 32)) return d;
        d.fsize = static_cast<uint8_t>(64 - scale);
        if (opcode <= 1 && rmode == 3) {
            d.op = Op::FCvtToInt;
            d.rounding = Rounding::Zero;
            d.sign_extend = opcode == 0;
        } else if ((opcode == 2 || opcode == 3) && rmode == 0) {
            d.op = Op::IntToFp;
            d.sign_extend = opcode == 2;
        }
        return d;
    }
    switch (Bits(w, 11, 10)) {
    case 0b00:
        if (Bits(w, 12, 10) == 0b100) {  // FMOV imediato
            if (Bits(w, 9, 5) != 0 || Bit(w, 31) || half) return d;
            d.op = Op::FMovImm;
            d.imm = static_cast<int64_t>(VfpExpandImm(Bits(w, 20, 13), d.size));
            return d;
        }
        if (Bits(w, 13, 10) == 0b1000) {  // FCMP/FCMPE (com zero no bit 3)
            if (Bit(w, 31) || Bits(w, 15, 14) != 0 || Bits(w, 2, 0) != 0 || half) return d;
            d.op = Op::FCmp;
            d.imm = Bit(w, 3);
            return d;
        }
        if (Bits(w, 14, 10) == 0b10000) {  // 1 fonte
            if (Bit(w, 31)) return d;
            const uint32_t opcode = Bits(w, 20, 15);
            if (opcode >= 0b000100 && opcode <= 0b000111) {
                if (opcode == 0b000110 || opcode - 0b000100 == type) return d;
                d.op = Op::FCvt;
                d.fsize = d.size;
                d.size = static_cast<uint8_t>(opcode == 0b000100 ? 2 : opcode == 0b000101 ? 3 : 1);
                return d;
            }
            if (half) return d;
            switch (opcode) {
            case 0b000000: d.op = Op::FMov; break;
            case 0b000001: d.op = Op::FAbs; break;
            case 0b000010: d.op = Op::FNeg; break;
            case 0b000011: d.op = Op::FSqrt; break;
            case 0b001000: d.op = Op::FRint; d.rounding = Rounding::Nearest; break;
            case 0b001001: d.op = Op::FRint; d.rounding = Rounding::PlusInf; break;
            case 0b001010: d.op = Op::FRint; d.rounding = Rounding::MinusInf; break;
            case 0b001011: d.op = Op::FRint; d.rounding = Rounding::Zero; break;
            case 0b001100: d.op = Op::FRint; d.rounding = Rounding::TiesAway; break;
            case 0b001110: d.op = Op::FRint; d.rounding = Rounding::Current; d.imm = 1; break;
            case 0b001111: d.op = Op::FRint; d.rounding = Rounding::Current; break;
            default: break;
            }
            return d;
        }
        if (Bits(w, 15, 10) == 0) {  // conversao com inteiro e FMOV de/para registrador geral
            const uint32_t rmode = Bits(w, 20, 19), opcode = Bits(w, 18, 16);
            d.sf = Bit(w, 31);
            d.general = true;
            if (opcode == 0b110 || opcode == 0b111) {
                const bool to_gp = opcode == 0b110;
                if (rmode == 1) {  // X <-> V.D[1]
                    if (!d.sf || type != 2) return d;
                    d.size = 3;
                    d.index = 1;
                } else if (rmode != 0 || half || d.sf != (type == 1)) {
                    return d;
                }
                d.op = to_gp ? Op::FMovToGp : Op::FMovFromGp;
                return d;
            }
            if (half) return d;
            if (opcode <= 1) {
                static constexpr Rounding modes[4] = {Rounding::Nearest, Rounding::PlusInf, Rounding::MinusInf, Rounding::Zero};
                d.op = Op::FCvtToInt;
                d.rounding = modes[rmode];
                d.sign_extend = opcode == 0;
            } else if ((opcode == 2 || opcode == 3) && rmode == 0) {
                d.op = Op::IntToFp;
                d.sign_extend = opcode == 2;
            } else if ((opcode == 4 || opcode == 5) && rmode == 0) {
                d.op = Op::FCvtToInt;
                d.rounding = Rounding::TiesAway;
                d.sign_extend = opcode == 4;
            }
            return d;
        }
        return d;
    case 0b01:  // FCCMP/FCCMPE
        if (Bit(w, 31) || half) return d;
        d.op = Op::FCcmp;
        d.cond = static_cast<uint8_t>(Bits(w, 15, 12));
        d.imm = Bits(w, 3, 0);
        return d;
    case 0b10: {  // 2 fontes
        if (Bit(w, 31) || half) return d;
        static constexpr Op ops[16] = {Op::FMul, Op::FDiv, Op::FAdd, Op::FSub, Op::FMax, Op::FMin, Op::FMaxNm, Op::FMinNm,
                                       Op::FNMul, Op::Unknown, Op::Unknown, Op::Unknown, Op::Unknown, Op::Unknown, Op::Unknown, Op::Unknown};
        d.op = ops[Bits(w, 15, 12)];
        return d;
    }
    default:  // FCSEL
        if (Bit(w, 31) || half) return d;
        d.op = Op::FCsel;
        d.cond = static_cast<uint8_t>(Bits(w, 15, 12));
        return d;
    }
}

} // namespace

Insn DecodeSimdData(Insn d, uint32_t w) {
    d.simd = true;
    d.rd = Reg(w, 0);
    d.rn = Reg(w, 5);
    d.rm = Reg(w, 16);
    d.q = Bit(w, 30);
    const uint32_t group = Bits(w, 28, 24);
    if (!Bit(w, 30) && (group == 0b11110 || group == 0b11111)) {
        d.q = false;
        return FloatingPoint(d, w);
    }
    if (Bit(w, 31)) return d;
    const bool scalar = Bit(w, 28);
    if (scalar) {
        if (!Bit(w, 30)) return d;
        d.q = false;
        if (group == 0b11110) {
            if (Bit(w, 21)) {
                if (Bit(w, 10)) return ThreeSame(d, w, true);
                if (Bits(w, 11, 10) == 0b10 && Bits(w, 20, 17) == 0b0000) return TwoRegMisc(d, w, true);
                if (Bits(w, 11, 10) == 0b10 && Bits(w, 20, 17) == 0b1000) return ScalarPairwise(d, w);
                return d;
            }
            if (Bits(w, 23, 21) == 0 && Bit(w, 10) && !Bit(w, 15)) return Copy(d, w, true);
            return d;
        }
        if (group == 0b11111) {
            if (!Bit(w, 10)) return ByElement(d, w, true);
            if (!Bit(w, 23)) return ShiftImmediate(d, w, true);
        }
        return d;
    }
    if (group == 0b01110) {
        if (Bit(w, 21)) {
            if (Bit(w, 10)) return ThreeSame(d, w, false);
            if (Bits(w, 11, 10) == 0b00) return ThreeDifferent(d, w);
            if (Bits(w, 20, 17) == 0b0000) return TwoRegMisc(d, w, false);
            if (Bits(w, 20, 17) == 0b1000) return AcrossLanes(d, w);
            return d;
        }
        if (Bit(w, 10)) {
            if (Bits(w, 23, 22) == 0 && !Bit(w, 15)) return Copy(d, w, false);
            return d;
        }
        if (Bit(w, 15)) return d;
        if (!Bit(w, 29)) {
            if (Bits(w, 11, 10) == 0b10) return Permute(d, w);
            if (Bits(w, 11, 10) == 0b00) {  // TBL/TBX
                if (Bits(w, 23, 22) != 0) return d;
                d.count = static_cast<uint8_t>(Bits(w, 14, 13) + 1);
                d.op = Bit(w, 12) ? Op::VTbx : Op::VTbl;
            }
            return d;
        }
        if (Bits(w, 23, 22) == 0 && (d.q || !Bit(w, 14))) {  // EXT
            d.op = Op::VExt;
            d.index = static_cast<uint8_t>(Bits(w, 14, 11));
        }
        return d;
    }
    if (group == 0b01111) {
        if (!Bit(w, 10)) return ByElement(d, w, false);
        if (Bits(w, 23, 19) == 0) return ModifiedImmediate(d, w);
        if (!Bit(w, 23)) return ShiftImmediate(d, w, false);
    }
    return d;
}

// LD1-LD4/ST1-ST4 (estruturas multiplas e de um elemento) e LD1R-LD4R; bit 26 = 1 e bits 29:27 = 001
Insn DecodeSimdMemory(Insn d, uint32_t w) {
    if (Bit(w, 31)) return d;
    d.q = Bit(w, 30);
    const bool load = Bit(w, 22), post = Bit(w, 23);
    const uint32_t kind = Bits(w, 25, 24);
    d.rm = Reg(w, 16);
    d.mode = post ? AddrMode::PostIndex : AddrMode::Offset;
    if (kind == 0) {  // multiplas estruturas
        if (Bit(w, 21) || (!post && d.rm != 0)) return d;
        const uint32_t opcode = Bits(w, 15, 12), size = Bits(w, 11, 10);
        uint8_t regs = 0, selem = 0;
        switch (opcode) {
        case 0b0000: regs = 4; selem = 4; break;
        case 0b0010: regs = 4; selem = 1; break;
        case 0b0100: regs = 3; selem = 3; break;
        case 0b0110: regs = 3; selem = 1; break;
        case 0b0111: regs = 1; selem = 1; break;
        case 0b1000: regs = 2; selem = 2; break;
        case 0b1010: regs = 2; selem = 1; break;
        default: return d;
        }
        if (size == 3 && !d.q && selem > 1) return d;
        d.op = load ? Op::LoadStruct : Op::StoreStruct;
        d.size = static_cast<uint8_t>(size);
        d.count = regs;
        d.count2 = selem;
        d.imm = regs * (d.q ? 16 : 8);
        return d;
    }
    if (kind != 1 || (!post && d.rm != 0)) return d;  // um elemento / replicar
    const uint32_t opcode = Bits(w, 15, 13), size = Bits(w, 11, 10);
    const bool s = Bit(w, 12), r = Bit(w, 21);
    const uint8_t selem = static_cast<uint8_t>((((opcode & 1) << 1) | r) + 1);
    const uint32_t scale = opcode >> 1;
    d.count = selem;
    d.count2 = selem;
    switch (scale) {
    case 3:
        if (!load || s) return d;
        d.op = Op::LoadReplicate;
        d.size = static_cast<uint8_t>(size);
        d.imm = selem << size;
        return d;
    case 0:
        d.size = 0;
        d.index = static_cast<uint8_t>((d.q << 3) | (s << 2) | size);
        break;
    case 1:
        if (size & 1) return d;
        d.size = 1;
        d.index = static_cast<uint8_t>((d.q << 2) | (s << 1) | (size >> 1));
        break;
    default:
        if (size & 2) return d;
        if (size == 0) {
            d.size = 2;
            d.index = static_cast<uint8_t>((d.q << 1) | s);
        } else {
            if (s) return d;
            d.size = 3;
            d.index = d.q;
        }
        break;
    }
    d.op = load ? Op::LoadLane : Op::StoreLane;
    d.imm = selem << d.size;
    return d;
}

} // namespace recomp::a64
