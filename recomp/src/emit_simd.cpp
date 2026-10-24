// Gerador de C++ para SIMD/FP: cada instrucao vira uma chamada aos helpers de recomp/simd.hpp com o tipo do
// elemento, o numero de elementos e um lambda com a operacao por elemento

#include "emit_internal.hpp"

#include <format>

namespace recomp::emit {

namespace {

using a64::Insn;
using a64::Op;

const char* U(unsigned size) {
    static const char* t[4] = {"uint8_t", "uint16_t", "uint32_t", "uint64_t"};
    return t[size & 3];
}

const char* S(unsigned size) {
    static const char* t[4] = {"int8_t", "int16_t", "int32_t", "int64_t"};
    return t[size & 3];
}

// Tipo FP: 1 meia (representada em bits), 2 simples, 3 dupla
const char* F(unsigned size) { return size == 3 ? "double" : size == 2 ? "float" : "uint16_t"; }

std::string V(unsigned n) { return std::format("c.vreg[{}]", n); }

unsigned Lanes(const Insn& d) { return d.scalar ? 1 : (d.q ? 16u : 8u) >> d.size; }

std::string Set(const Insn& d, const std::string& value) { return std::format("c.vreg[{}] = {};", d.rd, value); }

// Segundo operando: Vm ou Vm[index] replicado (formas por elemento)
std::string Second(const Insn& d, const char* type) {
    if (!d.element) return V(d.rm);
    return std::format("recomp::DupLane<{}, {}>({}, {})", type, 16u >> d.size, V(d.rm), d.index);
}

std::string Map1(const Insn& d, const char* type, const std::string& body) {
    return std::format("recomp::Map1<{}, {}>({}, [](auto a) {{ return {}; }})", type, Lanes(d), V(d.rn), body);
}

std::string Map2(const Insn& d, const char* type, const std::string& body) {
    return std::format("recomp::Map2<{}, {}>({}, {}, [](auto a, auto b) {{ return {}; }})", type, Lanes(d), V(d.rn), Second(d, type), body);
}

std::string Map3(const Insn& d, const char* type, const std::string& body) {
    return std::format("recomp::Map3<{}, {}>({}, {}, {}, [](auto acc, auto a, auto b) {{ return {}; }})", type, Lanes(d), V(d.rd), V(d.rn),
                       Second(d, type), body);
}

// Destino acumulado com um operando (SSRA, SRI, SLI): acc = Vd, a = Vn
std::string Accumulate(const Insn& d, const char* type, const std::string& body) {
    return std::format("recomp::Map2<{}, {}>({}, {}, [](auto acc, auto a) {{ return {}; }})", type, Lanes(d), V(d.rd), V(d.rn), body);
}

// Pares: escalar (Vn[0] op Vn[1]) ou vetor (Pairwise)
std::string Pair(const Insn& d, const char* type, const std::string& body) {
    if (d.scalar) {
        return std::format("recomp::Scalar<{0}>([](auto a, auto b) {{ return {1}; }}(recomp::Lane<{0}>({2}, 0), recomp::Lane<{0}>({2}, 1)))", type, body,
                           V(d.rn));
    }
    return std::format("recomp::Pairwise<{}, {}>({}, {}, [](auto a, auto b) {{ return {}; }})", type, Lanes(d), V(d.rn), V(d.rm), body);
}

std::string Reduce(const Insn& d, const char* type, const std::string& body) {
    return std::format("recomp::Scalar<{0}>(recomp::Reduce<{0}, {1}>({2}, [](auto a, auto b) {{ return {3}; }}))", type, Lanes(d), V(d.rn), body);
}

std::string Hex(uint64_t v) { return std::format("0x{:x}ull", v); }

std::string Mask(const std::string& condition) { return std::format("recomp::Mask<decltype(a)>({})", condition); }

// Inteiro do tamanho e sinal da conversao
const char* IntType(unsigned size, bool is_signed) { return is_signed ? S(size) : U(size); }

std::string PostIndex(const Insn& d) {
    if (d.mode != a64::AddrMode::PostIndex) return "";
    const std::string base = d.rn == 31 ? "c.sp" : std::format("c.x[{}]", d.rn);
    return d.rm == 31 ? std::format(" {} = a + {}ull;", base, d.imm) : std::format(" {} = a + c.x[{}];", base, d.rm);
}

}  // namespace

bool Simd(const Insn& d, std::string& code) {
    const char* u = U(d.size);
    const char* s = S(d.size);
    const char* f = F(d.size);
    const unsigned esize = 8u << d.size;
    const int mode = static_cast<int>(d.rounding);
    switch (d.op) {
    // ---- FP por elemento (vetorial ou escalar) ----
    case Op::FAdd: code = Set(d, Map2(d, f, "recomp::FpAdd(a, b)")); return true;
    case Op::FSub: code = Set(d, Map2(d, f, "recomp::FpSub(a, b)")); return true;
    case Op::FMul: code = Set(d, Map2(d, f, "recomp::FpMul(a, b)")); return true;
    case Op::FDiv: code = Set(d, Map2(d, f, "recomp::FpDiv(a, b)")); return true;
    case Op::FNMul: code = Set(d, Map2(d, f, "recomp::FpNeg(recomp::FpMul(a, b))")); return true;
    case Op::FMulX: code = Set(d, Map2(d, f, "recomp::FpMulX(a, b)")); return true;
    case Op::FAbd: code = Set(d, Map2(d, f, "recomp::FpAbs(recomp::FpSub(a, b))")); return true;
    case Op::FMax: code = Set(d, Map2(d, f, "recomp::FpMax(a, b)")); return true;
    case Op::FMin: code = Set(d, Map2(d, f, "recomp::FpMin(a, b)")); return true;
    case Op::FMaxNm: code = Set(d, Map2(d, f, "recomp::FpMaxNum(a, b)")); return true;
    case Op::FMinNm: code = Set(d, Map2(d, f, "recomp::FpMinNum(a, b)")); return true;
    case Op::FRecpS: code = Set(d, Map2(d, f, "recomp::FpRecipStep(a, b)")); return true;
    case Op::FRsqrtS: code = Set(d, Map2(d, f, "recomp::FpRsqrtStep(a, b)")); return true;
    case Op::FCmEq: code = Set(d, Map2(d, f, Mask("a == b"))); return true;
    case Op::FCmGe: code = Set(d, Map2(d, f, Mask("a >= b"))); return true;
    case Op::FCmGt: code = Set(d, Map2(d, f, Mask("a > b"))); return true;
    case Op::FAcGe: code = Set(d, Map2(d, f, Mask("recomp::FpAbs(a) >= recomp::FpAbs(b)"))); return true;
    case Op::FAcGt: code = Set(d, Map2(d, f, Mask("recomp::FpAbs(a) > recomp::FpAbs(b)"))); return true;
    case Op::FMla: code = Set(d, Map3(d, f, "recomp::FpMulAdd(acc, a, b)")); return true;
    case Op::FMls: code = Set(d, Map3(d, f, "recomp::FpMulAdd(acc, recomp::FpNeg(a), b)")); return true;
    case Op::FAddP: code = Set(d, Pair(d, f, "recomp::FpAdd(a, b)")); return true;
    case Op::FMaxP: code = Set(d, Pair(d, f, "recomp::FpMax(a, b)")); return true;
    case Op::FMinP: code = Set(d, Pair(d, f, "recomp::FpMin(a, b)")); return true;
    case Op::FMaxNmP: code = Set(d, Pair(d, f, "recomp::FpMaxNum(a, b)")); return true;
    case Op::FMinNmP: code = Set(d, Pair(d, f, "recomp::FpMinNum(a, b)")); return true;
    case Op::FMaxV: code = Set(d, Reduce(d, f, "recomp::FpMax(a, b)")); return true;
    case Op::FMinV: code = Set(d, Reduce(d, f, "recomp::FpMin(a, b)")); return true;
    case Op::FMaxNmV: code = Set(d, Reduce(d, f, "recomp::FpMaxNum(a, b)")); return true;
    case Op::FMinNmV: code = Set(d, Reduce(d, f, "recomp::FpMinNum(a, b)")); return true;
    case Op::FMov: code = Set(d, Map1(d, u, "a")); return true;  // copia de bits (sem tocar em NaN)
    case Op::FAbs: code = Set(d, Map1(d, f, "recomp::FpAbs(a)")); return true;
    case Op::FNeg: code = Set(d, Map1(d, f, "recomp::FpNeg(a)")); return true;
    case Op::FSqrt: code = Set(d, Map1(d, f, "recomp::FpSqrt(a)")); return true;
    case Op::FRint: code = Set(d, Map1(d, f, std::format("recomp::FpRoundInt<{}>(a)", mode))); return true;
    case Op::FRecpE: code = Set(d, Map1(d, f, "recomp::FpRecipEstimate(a)")); return true;
    case Op::FRsqrtE: code = Set(d, Map1(d, f, "recomp::FpRsqrtEstimate(a)")); return true;
    case Op::FCmEqZ: code = Set(d, Map1(d, f, Mask("a == 0"))); return true;
    case Op::FCmGeZ: code = Set(d, Map1(d, f, Mask("a >= 0"))); return true;
    case Op::FCmGtZ: code = Set(d, Map1(d, f, Mask("a > 0"))); return true;
    case Op::FCmLeZ: code = Set(d, Map1(d, f, Mask("a <= 0"))); return true;
    case Op::FCmLtZ: code = Set(d, Map1(d, f, Mask("a < 0"))); return true;
    case Op::FCvt:
        code = Set(d, std::format("recomp::Scalar<{0}>(recomp::ConvertFp<{0}, {1}>(recomp::Lane<{1}>({2}, 0)))", f, F(d.fsize), V(d.rn)));
        return true;
    case Op::FCvtN:
        code = Set(d, std::format("recomp::Narrow<{0}, {1}>({2}, {3}, {4}, [](auto a) {{ return recomp::ConvertFp<{1}, {0}>(a); }})", F(d.fsize), f, V(d.rd),
                                  V(d.rn), d.q));
        return true;
    case Op::FCvtL:
        code = Set(d, std::format("recomp::Widen<{0}, {1}>({2}, {3}, [](auto a) {{ return recomp::ConvertFp<{1}, {0}>(a); }})", F(d.fsize), f, V(d.rn), d.q));
        return true;
    case Op::FMovImm: code = Set(d, std::format("recomp::Scalar<{}>({})", u, Hex(static_cast<uint64_t>(d.imm)))); return true;
    case Op::FMovToGp:
        code = Write(d.rd, d.sf, d.index ? std::format("recomp::Lane<uint64_t>({}, 1)", V(d.rn)) : std::format("recomp::Lane<{}>({}, 0)", u, V(d.rn)));
        return true;
    case Op::FMovFromGp:
        code = d.index ? std::format("recomp::SetLane<uint64_t>(c.vreg[{}], 1, {});", d.rd, Reg(d.rn, true))
                       : Set(d, std::format("recomp::Scalar<{0}>(static_cast<{0}>({1}))", u, Reg(d.rn, d.sf)));
        return true;
    case Op::FCvtToInt:
        if (d.general) {
            const char* type = d.sf ? (d.sign_extend ? "int64_t" : "uint64_t") : (d.sign_extend ? "int32_t" : "uint32_t");
            code = Write(d.rd, d.sf, std::format("recomp::FpToInt<{}, {}, {}>(recomp::Lane<{}>({}, 0))", type, mode, d.fsize, f, V(d.rn)));
        } else {
            code = Set(d, Map1(d, f, std::format("recomp::FpToInt<{}, {}, {}>(a)", IntType(d.size, d.sign_extend), mode, d.fsize)));
        }
        return true;
    case Op::IntToFp:
        if (d.general) {
            const char* type = d.sf ? (d.sign_extend ? "int64_t" : "uint64_t") : (d.sign_extend ? "int32_t" : "uint32_t");
            code = Set(d, std::format("recomp::Scalar<{0}>(recomp::IntToFp<{0}, {1}>(static_cast<{2}>({3})))", f, d.fsize, type, Reg(d.rn, d.sf)));
        } else {
            code = Set(d, Map1(d, IntType(d.size, d.sign_extend), std::format("recomp::IntToFp<{}, {}>(a)", f, d.fsize)));
        }
        return true;
    case Op::FMadd: case Op::FMsub: case Op::FNMadd: case Op::FNMsub: {
        const std::string a = std::format("recomp::Lane<{}>({}, 0)", f, V(d.ra)), n = std::format("recomp::Lane<{}>({}, 0)", f, V(d.rn)),
                          m = std::format("recomp::Lane<{}>({}, 0)", f, V(d.rm));
        const bool neg_a = d.op == Op::FNMadd || d.op == Op::FNMsub, neg_n = d.op == Op::FMsub || d.op == Op::FNMadd;
        code = Set(d, std::format("recomp::Scalar<{}>(recomp::FpMulAdd({}, {}, {}))", f, neg_a ? "recomp::FpNeg(" + a + ")" : a,
                                  neg_n ? "recomp::FpNeg(" + n + ")" : n, m));
        return true;
    }
    case Op::FCmp:
    case Op::FCcmp: {
        const std::string compare = std::format("recomp::FpCompare(c, recomp::Lane<{0}>({1}, 0), {2});", f, V(d.rn),
                                                d.op == Op::FCmp && d.imm ? std::format("{}{{0}}", f) : std::format("recomp::Lane<{}>({}, 0)", f, V(d.rm)));
        code = d.op == Op::FCmp ? compare : std::format("if (recomp::Condition(c, {})) {} else recomp::SetNzcv(c, {});", d.cond, compare, d.imm);
        return true;
    }
    case Op::FCsel:
        code = Set(d, std::format("recomp::Scalar<{0}>(recomp::Condition(c, {1}) ? recomp::Lane<{0}>({2}, 0) : recomp::Lane<{0}>({3}, 0))", u, d.cond,
                                  V(d.rn), V(d.rm)));
        return true;

    // ---- Inteiros por elemento ----
    case Op::VAdd: code = Set(d, Map2(d, u, "static_cast<decltype(a)>(a + b)")); return true;
    case Op::VSub: code = Set(d, Map2(d, u, "static_cast<decltype(a)>(a - b)")); return true;
    case Op::VMul: code = Set(d, Map2(d, u, "static_cast<decltype(a)>(uint64_t{a} * b)")); return true;
    case Op::VMla: code = Set(d, Map3(d, u, "static_cast<decltype(a)>(acc + uint64_t{a} * b)")); return true;
    case Op::VMls: code = Set(d, Map3(d, u, "static_cast<decltype(a)>(acc - uint64_t{a} * b)")); return true;
    case Op::VAnd: code = Set(d, Map2(d, u, "static_cast<decltype(a)>(a & b)")); return true;
    case Op::VBic: code = Set(d, Map2(d, u, "static_cast<decltype(a)>(a & ~b)")); return true;
    case Op::VOrr: code = Set(d, Map2(d, u, "static_cast<decltype(a)>(a | b)")); return true;
    case Op::VOrn: code = Set(d, Map2(d, u, "static_cast<decltype(a)>(a | ~b)")); return true;
    case Op::VEor: code = Set(d, Map2(d, u, "static_cast<decltype(a)>(a ^ b)")); return true;
    case Op::VBsl: code = Set(d, Map3(d, u, "static_cast<decltype(a)>((acc & a) | (~acc & b))")); return true;
    case Op::VBit: code = Set(d, Map3(d, u, "static_cast<decltype(a)>((a & b) | (acc & ~b))")); return true;
    case Op::VBif: code = Set(d, Map3(d, u, "static_cast<decltype(a)>((acc & b) | (a & ~b))")); return true;
    case Op::VCmEq: code = Set(d, Map2(d, u, Mask("a == b"))); return true;
    case Op::VCmTst: code = Set(d, Map2(d, u, Mask("(a & b) != 0"))); return true;
    case Op::VCmGe: code = Set(d, Map2(d, s, Mask("a >= b"))); return true;
    case Op::VCmGt: code = Set(d, Map2(d, s, Mask("a > b"))); return true;
    case Op::VCmHi: code = Set(d, Map2(d, u, Mask("a > b"))); return true;
    case Op::VCmHs: code = Set(d, Map2(d, u, Mask("a >= b"))); return true;
    case Op::VSMax: code = Set(d, Map2(d, s, "a > b ? a : b")); return true;
    case Op::VSMin: code = Set(d, Map2(d, s, "a < b ? a : b")); return true;
    case Op::VUMax: code = Set(d, Map2(d, u, "a > b ? a : b")); return true;
    case Op::VUMin: code = Set(d, Map2(d, u, "a < b ? a : b")); return true;
    case Op::VSAbd:
    case Op::VUAbd:
        code = Set(d, Map2(d, d.op == Op::VSAbd ? s : u, "static_cast<decltype(a)>(a > b ? uint64_t(a) - uint64_t(b) : uint64_t(b) - uint64_t(a))"));
        return true;
    case Op::VSShl: code = Set(d, Map2(d, s, "recomp::ShiftByRegister(a, b)")); return true;
    case Op::VUShl: code = Set(d, Map2(d, u, "recomp::ShiftByRegister(a, b)")); return true;
    case Op::VSQAdd: code = Set(d, Map2(d, s, "recomp::SatAdd(a, b)")); return true;
    case Op::VUQAdd: code = Set(d, Map2(d, u, "recomp::SatAdd(a, b)")); return true;
    case Op::VSQSub: code = Set(d, Map2(d, s, "recomp::SatSub(a, b)")); return true;
    case Op::VUQSub: code = Set(d, Map2(d, u, "recomp::SatSub(a, b)")); return true;
    case Op::VAddP: code = Set(d, Pair(d, u, "static_cast<decltype(a)>(a + b)")); return true;
    case Op::VSMaxP: code = Set(d, Pair(d, s, "a > b ? a : b")); return true;
    case Op::VSMinP: code = Set(d, Pair(d, s, "a < b ? a : b")); return true;
    case Op::VUMaxP: code = Set(d, Pair(d, u, "a > b ? a : b")); return true;
    case Op::VUMinP: code = Set(d, Pair(d, u, "a < b ? a : b")); return true;
    case Op::VAbs: code = Set(d, Map1(d, s, "static_cast<decltype(a)>(a < 0 ? uint64_t{0} - uint64_t(a) : uint64_t(a))")); return true;
    case Op::VNeg: code = Set(d, Map1(d, s, "static_cast<decltype(a)>(uint64_t{0} - uint64_t(a))")); return true;
    case Op::VNot: code = Set(d, Map1(d, u, "static_cast<decltype(a)>(~a)")); return true;
    case Op::VRbit: code = Set(d, Map1(d, u, "recomp::ReverseBits<uint8_t>(a)")); return true;
    case Op::VCnt: code = Set(d, Map1(d, u, "static_cast<uint8_t>(std::popcount(a))")); return true;
    case Op::VClz: code = Set(d, Map1(d, u, "static_cast<decltype(a)>(std::countl_zero(a))")); return true;
    case Op::VCls: code = Set(d, Map1(d, u, "recomp::CountLeadingSignBits<decltype(a)>(a)")); return true;
    case Op::VCmEqZ: code = Set(d, Map1(d, s, Mask("a == 0"))); return true;
    case Op::VCmGeZ: code = Set(d, Map1(d, s, Mask("a >= 0"))); return true;
    case Op::VCmGtZ: code = Set(d, Map1(d, s, Mask("a > 0"))); return true;
    case Op::VCmLeZ: code = Set(d, Map1(d, s, Mask("a <= 0"))); return true;
    case Op::VCmLtZ: code = Set(d, Map1(d, s, Mask("a < 0"))); return true;
    case Op::VRev64: code = Set(d, std::format("recomp::ReverseElements<{}, 8>({}, {})", u, V(d.rn), d.q)); return true;
    case Op::VRev32: code = Set(d, std::format("recomp::ReverseElements<{}, 4>({}, {})", u, V(d.rn), d.q)); return true;
    case Op::VRev16: code = Set(d, std::format("recomp::ReverseElements<{}, 2>({}, {})", u, V(d.rn), d.q)); return true;
    case Op::VXtn:
        code = Set(d, std::format("recomp::Narrow<{}, {}>({}, {}, {}, [](auto a) {{ return a; }})", U(d.size + 1), u, V(d.rd), V(d.rn), d.q));
        return true;
    case Op::VSAddLP:
    case Op::VUAddLP:
        code = Set(d, std::format("recomp::AddLongPairwise<{}, {}>({}, {})", d.op == Op::VSAddLP ? s : u, U(d.size + 1), V(d.rn), d.q));
        return true;
    case Op::VAddV: code = Set(d, Reduce(d, u, "static_cast<decltype(a)>(a + b)")); return true;
    case Op::VSMaxV: code = Set(d, Reduce(d, s, "a > b ? a : b")); return true;
    case Op::VSMinV: code = Set(d, Reduce(d, s, "a < b ? a : b")); return true;
    case Op::VUMaxV: code = Set(d, Reduce(d, u, "a > b ? a : b")); return true;
    case Op::VUMinV: code = Set(d, Reduce(d, u, "a < b ? a : b")); return true;
    case Op::VSAddLV:
    case Op::VUAddLV:
        code = Set(d, std::format("recomp::Scalar<{}>(static_cast<{}>(recomp::SumLong<{}, {}>({})))", U(d.size + 1), U(d.size + 1),
                                  d.op == Op::VSAddLV ? s : u, Lanes(d), V(d.rn)));
        return true;

    // ---- Copias e imediatos ----
    case Op::VDupElem:
        code = d.scalar ? Set(d, std::format("recomp::Scalar<{0}>(recomp::Lane<{0}>({1}, {2}))", u, V(d.rn), d.index))
                        : Set(d, std::format("recomp::DupLane<{}, {}>({}, {})", u, Lanes(d), V(d.rn), d.index));
        return true;
    case Op::VDupGp: code = Set(d, std::format("recomp::Dup<{0}, {1}>(static_cast<{0}>({2}))", u, Lanes(d), Reg(d.rn, d.sf))); return true;
    case Op::VInsGp: code = std::format("recomp::SetLane<{0}>(c.vreg[{1}], {2}, static_cast<{0}>({3}));", u, d.rd, d.index, Reg(d.rn, d.sf)); return true;
    case Op::VInsElem:
        code = std::format("recomp::SetLane<{0}>(c.vreg[{1}], {2}, recomp::Lane<{0}>(c.vreg[{3}], {4}));", u, d.rd, d.index, d.rn, d.index2);
        return true;
    case Op::VUMov: code = Write(d.rd, d.sf, std::format("recomp::Lane<{}>({}, {})", u, V(d.rn), d.index)); return true;
    case Op::VSMov:
        code = Write(d.rd, d.sf, std::format("static_cast<{}>(static_cast<{}>(recomp::Lane<{}>({}, {})))", d.sf ? "uint64_t" : "uint32_t",
                                             d.sf ? "int64_t" : "int32_t", s, V(d.rn), d.index));
        return true;
    case Op::VMovImm: {
        const std::string imm = Hex(static_cast<uint64_t>(d.imm));
        code = Set(d, std::format("recomp::FromU64({}, {})", imm, d.q ? imm : "0"));
        return true;
    }
    case Op::VOrrImm:
    case Op::VBicImm: {
        const std::string imm = d.op == Op::VOrrImm ? "| " + Hex(static_cast<uint64_t>(d.imm)) : "& ~" + Hex(static_cast<uint64_t>(d.imm));
        code = Set(d, std::format("recomp::FromU64(recomp::Lo64({0}) {1}, {2})", V(d.rd), imm, d.q ? std::format("recomp::Hi64({}) {}", V(d.rd), imm) : "0"));
        return true;
    }

    // ---- Deslocamentos imediatos ----
    case Op::VShl: code = Set(d, Map1(d, u, std::format("static_cast<decltype(a)>(uint64_t{{a}} << {})", d.amount))); return true;
    case Op::VSShr: code = Set(d, Map1(d, s, std::format("static_cast<decltype(a)>(a >> {})", std::min<unsigned>(d.amount, esize - 1)))); return true;
    case Op::VUShr:
        code = Set(d, Map1(d, u, d.amount >= esize ? std::string("decltype(a){0}") : std::format("static_cast<decltype(a)>(a >> {})", d.amount)));
        return true;
    case Op::VSSra:
        code = Set(d, Accumulate(d, s, std::format("static_cast<decltype(a)>(uint64_t(acc) + uint64_t(a >> {}))", std::min<unsigned>(d.amount, esize - 1))));
        return true;
    case Op::VUSra:
        code = Set(d, Accumulate(d, u, d.amount >= esize ? std::string("acc") : std::format("static_cast<decltype(a)>(acc + (a >> {}))", d.amount)));
        return true;
    case Op::VSRShr:
        code = Set(d, Map1(d, s, std::format("static_cast<decltype(a)>((static_cast<__int128>(a) + (__int128{{1}} << {})) >> {})", d.amount - 1, d.amount)));
        return true;
    case Op::VURShr:
        code = Set(d, Map1(d, u, std::format("static_cast<decltype(a)>((static_cast<unsigned __int128>(a) + (static_cast<unsigned __int128>(1) << {})) >> {})",
                                            d.amount - 1, d.amount)));
        return true;
    case Op::VSri: {
        if (d.amount >= esize) {
            code = Set(d, Accumulate(d, u, "acc"));
            return true;
        }
        const uint64_t ones = esize == 64 ? ~uint64_t{0} : (uint64_t{1} << esize) - 1;
        code = Set(d, Accumulate(d, u, std::format("static_cast<decltype(a)>((acc & ~{}) | (a >> {}))", Hex(ones >> d.amount), d.amount)));
        return true;
    }
    case Op::VSli: {
        const uint64_t ones = esize == 64 ? ~uint64_t{0} : (uint64_t{1} << esize) - 1;
        const uint64_t mask = (ones << d.amount) & ones;
        code = Set(d, Accumulate(d, u, std::format("static_cast<decltype(a)>((acc & ~{0}) | ((uint64_t{{a}} << {1}) & {0}))", Hex(mask), d.amount)));
        return true;
    }
    case Op::VShrn:
    case Op::VRShrn: {
        const std::string body = d.op == Op::VShrn ? std::format("a >> {}", d.amount)
                                                   : std::format("(static_cast<unsigned __int128>(a) + (static_cast<unsigned __int128>(1) << {})) >> {}",
                                                                 d.amount - 1, d.amount);
        code = Set(d, std::format("recomp::Narrow<{}, {}>({}, {}, {}, [](auto a) {{ return {}; }})", U(d.size + 1), u, V(d.rd), V(d.rn), d.q, body));
        return true;
    }
    case Op::VSShll:
    case Op::VUShll: {
        const char* wide = U(d.size + 1);
        const std::string body = d.op == Op::VSShll ? std::format("static_cast<uint64_t>(static_cast<int64_t>(a)) << {}", d.amount)
                                                    : std::format("uint64_t{{a}} << {}", d.amount);
        code = Set(d, std::format("recomp::Widen<{}, {}>({}, {}, [](auto a) {{ return {}; }})", d.op == Op::VSShll ? s : u, wide, V(d.rn), d.q, body));
        return true;
    }

    // ---- Formas longas, largas e estreitas ----
    case Op::VSAddL: case Op::VUAddL: case Op::VSSubL: case Op::VUSubL: case Op::VSMull: case Op::VUMull: case Op::VSAbdl: case Op::VUAbdl: {
        const bool is_signed = d.op == Op::VSAddL || d.op == Op::VSSubL || d.op == Op::VSMull || d.op == Op::VSAbdl;
        const char* narrow = is_signed ? s : u;
        const char* body = d.op == Op::VSAddL || d.op == Op::VUAddL ? "a + b"
                           : d.op == Op::VSSubL || d.op == Op::VUSubL ? "a - b"
                           : d.op == Op::VSMull || d.op == Op::VUMull ? "a * b"
                                                                      : "a > b ? a - b : b - a";
        code = Set(d, std::format("recomp::Long<{}, {}>({}, {}, {}, [](auto a, auto b) {{ return {}; }})", narrow, U(d.size + 1), V(d.rn), Second(d, narrow),
                                  d.q, body));
        return true;
    }
    case Op::VSMlal: case Op::VUMlal: case Op::VSMlsl: case Op::VUMlsl: {
        const bool is_signed = d.op == Op::VSMlal || d.op == Op::VSMlsl;
        const char* narrow = is_signed ? s : u;
        const char* body = d.op == Op::VSMlal || d.op == Op::VUMlal ? "uint64_t(acc) + uint64_t(a * b)" : "uint64_t(acc) - uint64_t(a * b)";
        code = Set(d, std::format("recomp::LongAcc<{}, {}>({}, {}, {}, {}, [](auto acc, auto a, auto b) {{ return {}; }})", narrow, U(d.size + 1), V(d.rd),
                                  V(d.rn), Second(d, narrow), d.q, body));
        return true;
    }
    case Op::VSAddW: case Op::VUAddW: case Op::VSSubW: case Op::VUSubW: {
        const bool is_signed = d.op == Op::VSAddW || d.op == Op::VSSubW;
        const char* body = d.op == Op::VSAddW || d.op == Op::VUAddW ? "a + b" : "a - b";
        code = Set(d, std::format("recomp::Wide<{}, {}>({}, {}, {}, [](auto a, auto b) {{ return {}; }})", is_signed ? s : u, U(d.size + 1), V(d.rn),
                                  V(d.rm), d.q, body));
        return true;
    }
    case Op::VAddHn:
    case Op::VSubHn:
        code = Set(d, std::format("recomp::Narrow2<{}, {}>({}, {}, {}, {}, [](auto a, auto b) {{ return (uint64_t(a) {} uint64_t(b)) >> {}; }})",
                                  U(d.size + 1), u, V(d.rd), V(d.rn), V(d.rm), d.q, d.op == Op::VAddHn ? "+" : "-", esize));
        return true;

    // ---- Permutacoes e tabelas ----
    case Op::VZip1: case Op::VZip2:
        code = Set(d, std::format("recomp::Zip<{}, {}>({}, {}, {})", u, Lanes(d), V(d.rn), V(d.rm), d.op == Op::VZip2));
        return true;
    case Op::VUzp1: case Op::VUzp2:
        code = Set(d, std::format("recomp::Uzp<{}, {}>({}, {}, {})", u, Lanes(d), V(d.rn), V(d.rm), d.op == Op::VUzp2));
        return true;
    case Op::VTrn1: case Op::VTrn2:
        code = Set(d, std::format("recomp::Trn<{}, {}>({}, {}, {})", u, Lanes(d), V(d.rn), V(d.rm), d.op == Op::VTrn2));
        return true;
    case Op::VExt: code = Set(d, std::format("recomp::Ext({}, {}, {}, {})", V(d.rn), V(d.rm), d.index, d.q)); return true;
    case Op::VTbl:
    case Op::VTbx:
        code = Set(d, std::format("recomp::TableLookup(c, {}, {}, {}, {}, {})", d.rn, d.count, V(d.rm), d.q,
                                  d.op == Op::VTbx ? std::format("&c.vreg[{}]", d.rd) : std::string("nullptr")));
        return true;

    // ---- LD1-LD4 / ST1-ST4 ----
    case Op::LoadStruct: case Op::StoreStruct:
        code = std::format("{{ const uint64_t a = {}; recomp::{}<{}>(c, a, {}, {}, {}, {});{} }}", Reg(d.rn, true, true),
                           d.op == Op::LoadStruct ? "LoadStructs" : "StoreStructs", u, d.rd, d.count, d.count2, d.q, PostIndex(d));
        return true;
    case Op::LoadLane: case Op::StoreLane:
        code = std::format("{{ const uint64_t a = {}; recomp::{}<{}>(c, a, {}, {}, {});{} }}", Reg(d.rn, true, true),
                           d.op == Op::LoadLane ? "LoadLane" : "StoreLane", u, d.rd, d.count2, d.index, PostIndex(d));
        return true;
    case Op::LoadReplicate:
        code = std::format("{{ const uint64_t a = {}; recomp::LoadReplicate<{}>(c, a, {}, {}, {});{} }}", Reg(d.rn, true, true), u, d.rd, d.count2, d.q,
                           PostIndex(d));
        return true;
    default:
        return false;
    }
}

} // namespace recomp::emit
