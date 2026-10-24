#pragma once
// Decodificador AArch64 (A64) do recompilador — ARMv8.0-A do Cortex-A57 do Switch
//
// Inteiros: imediatos (add/sub, logico, move wide, bitfield, extract, adr/adrp), ramificacoes (b, bl, b.cond,
// cbz/cbnz, tbz/tbnz, br/blr/ret), loads/stores (offset sem sinal, pre/pos-indice, sem escala, registrador, par,
// literal), registradores (logico, add/sub deslocado/estendido, adc/sbc, csel, ccmp, 1/2/3 fontes), sistema (hint,
// barreiras, mrs/msr, svc, brk, udf), exclusivos (ldxr/stxr, pares) e acquire/release (ldar/stlr).
// SIMD/FP (src/a64_simd.cpp): loads/stores de B/H/S/D/Q, LD1-LD4/ST1-ST4, FP escalar, NEON inteiro e FP.
// CAS/LSE da v8.1 nao existem no A57.

#include <cstdint>
#include <string>

namespace recomp::a64 {

// X(enumerador, nome) — nomes usados em histogramas, CSVs e diagnosticos
#define RECOMP_A64_OPS(X)                                                                                                    \
    X(Unknown, "unknown")                                                                                                     \
    /* Imediatos */                                                                                                           \
    X(AddImm, "add_imm") X(SubImm, "sub_imm") X(MovZ, "movz") X(MovN, "movn") X(MovK, "movk") X(AndImm, "and_imm")           \
    X(OrrImm, "orr_imm") X(EorImm, "eor_imm") X(Adr, "adr") X(Adrp, "adrp") X(Sbfm, "sbfm") X(Bfm, "bfm") X(Ubfm, "ubfm")    \
    X(Extr, "extr")                                                                                                           \
    /* Ramificacoes */                                                                                                        \
    X(B, "b") X(Bl, "bl") X(BCond, "b.cond") X(Cbz, "cbz") X(Cbnz, "cbnz") X(Tbz, "tbz") X(Tbnz, "tbnz") X(Br, "br")         \
    X(Blr, "blr") X(Ret, "ret")                                                                                               \
    /* Loads/stores (simd = registradores V) */                                                                              \
    X(Load, "load") X(Store, "store") X(LoadPair, "load_pair") X(StorePair, "store_pair") X(Prefetch, "prefetch")            \
    X(LoadExclusive, "load_exclusive") X(StoreExclusive, "store_exclusive") X(LoadExclusivePair, "load_exclusive_pair")     \
    X(StoreExclusivePair, "store_exclusive_pair") X(LoadAcquire, "load_acquire") X(StoreRelease, "store_release")           \
    X(LoadStruct, "ld_struct") X(StoreStruct, "st_struct") X(LoadLane, "ld_lane") X(StoreLane, "st_lane")                   \
    X(LoadReplicate, "ld_replicate")                                                                                          \
    /* Registradores */                                                                                                       \
    X(And, "and") X(Bic, "bic") X(Orr, "orr") X(Orn, "orn") X(Eor, "eor") X(Eon, "eon")                                     \
    X(AddShift, "add_shift") X(SubShift, "sub_shift") X(AddExt, "add_ext") X(SubExt, "sub_ext") X(Adc, "adc") X(Sbc, "sbc")  \
    X(Csel, "csel") X(Csinc, "csinc") X(Csinv, "csinv") X(Csneg, "csneg") X(Ccmp, "ccmp") X(Ccmn, "ccmn")                   \
    X(Udiv, "udiv") X(Sdiv, "sdiv") X(Lslv, "lslv") X(Lsrv, "lsrv") X(Asrv, "asrv") X(Rorv, "rorv")                         \
    X(Rbit, "rbit") X(Rev16, "rev16") X(Rev32, "rev32") X(Rev, "rev") X(Clz, "clz") X(Cls, "cls")                           \
    X(Madd, "madd") X(Msub, "msub") X(Smaddl, "smaddl") X(Smsubl, "smsubl") X(Umaddl, "umaddl") X(Umsubl, "umsubl")         \
    X(Smulh, "smulh") X(Umulh, "umulh")                                                                                       \
    /* Sistema */                                                                                                             \
    X(Hint, "hint") X(Barrier, "barrier") X(Clrex, "clrex") X(Mrs, "mrs") X(Msr, "msr") X(Svc, "svc") X(Brk, "brk")          \
    X(Udf, "udf")                                                                                                             \
    /* FP escalar (scalar = true) e vetorial; size = tipo FP (1 meia, 2 simples, 3 dupla) */                                \
    X(FMov, "fmov") X(FMovImm, "fmov_imm") X(FMovToGp, "fmov_to_gp") X(FMovFromGp, "fmov_from_gp")                          \
    X(FAbs, "fabs") X(FNeg, "fneg") X(FSqrt, "fsqrt") X(FCvt, "fcvt") X(FRint, "frint")                                     \
    X(FAdd, "fadd") X(FSub, "fsub") X(FMul, "fmul") X(FDiv, "fdiv") X(FNMul, "fnmul") X(FMulX, "fmulx") X(FAbd, "fabd")     \
    X(FMax, "fmax") X(FMin, "fmin") X(FMaxNm, "fmaxnm") X(FMinNm, "fminnm")                                                 \
    X(FMadd, "fmadd") X(FMsub, "fmsub") X(FNMadd, "fnmadd") X(FNMsub, "fnmsub") X(FMla, "fmla") X(FMls, "fmls")             \
    X(FRecpE, "frecpe") X(FRsqrtE, "frsqrte") X(FRecpS, "frecps") X(FRsqrtS, "frsqrts")                                     \
    X(FCmp, "fcmp") X(FCcmp, "fccmp") X(FCsel, "fcsel")                                                                       \
    X(FCmEq, "fcmeq") X(FCmGe, "fcmge") X(FCmGt, "fcmgt") X(FAcGe, "facge") X(FAcGt, "facgt")                               \
    X(FCmEqZ, "fcmeq_zero") X(FCmGeZ, "fcmge_zero") X(FCmGtZ, "fcmgt_zero") X(FCmLeZ, "fcmle_zero") X(FCmLtZ, "fcmlt_zero")  \
    X(FAddP, "faddp") X(FMaxP, "fmaxp") X(FMinP, "fminp") X(FMaxNmP, "fmaxnmp") X(FMinNmP, "fminnmp")                       \
    X(FMaxV, "fmaxv") X(FMinV, "fminv") X(FMaxNmV, "fmaxnmv") X(FMinNmV, "fminnmv")                                         \
    X(FCvtToInt, "fcvt_to_int") X(IntToFp, "int_to_fp") X(FCvtN, "fcvtn") X(FCvtL, "fcvtl")                                 \
    /* NEON inteiro (vetorial ou escalar em registrador V) */                                                                 \
    X(VAdd, "add_v") X(VSub, "sub_v") X(VMul, "mul_v") X(VMla, "mla_v") X(VMls, "mls_v")                                     \
    X(VAnd, "and_v") X(VBic, "bic_v") X(VOrr, "orr_v") X(VOrn, "orn_v") X(VEor, "eor_v") X(VBsl, "bsl") X(VBit, "bit")       \
    X(VBif, "bif") X(VCmEq, "cmeq") X(VCmGe, "cmge") X(VCmGt, "cmgt") X(VCmHi, "cmhi") X(VCmHs, "cmhs") X(VCmTst, "cmtst")   \
    X(VCmEqZ, "cmeq_zero") X(VCmGeZ, "cmge_zero") X(VCmGtZ, "cmgt_zero") X(VCmLeZ, "cmle_zero") X(VCmLtZ, "cmlt_zero")       \
    X(VSMax, "smax") X(VSMin, "smin") X(VUMax, "umax") X(VUMin, "umin") X(VSAbd, "sabd") X(VUAbd, "uabd")                    \
    X(VSShl, "sshl") X(VUShl, "ushl") X(VUQAdd, "uqadd") X(VSQAdd, "sqadd") X(VUQSub, "uqsub") X(VSQSub, "sqsub")           \
    X(VAddP, "addp") X(VSMaxP, "smaxp") X(VSMinP, "sminp") X(VUMaxP, "umaxp") X(VUMinP, "uminp")                            \
    X(VAbs, "abs") X(VNeg, "neg") X(VNot, "not") X(VRbit, "rbit_v") X(VCnt, "cnt") X(VClz, "clz_v") X(VCls, "cls_v")        \
    X(VRev16, "rev16_v") X(VRev32, "rev32_v") X(VRev64, "rev64") X(VXtn, "xtn") X(VSAddLP, "saddlp") X(VUAddLP, "uaddlp")   \
    X(VAddV, "addv") X(VSAddLV, "saddlv") X(VUAddLV, "uaddlv") X(VSMaxV, "smaxv") X(VSMinV, "sminv") X(VUMaxV, "umaxv")     \
    X(VUMinV, "uminv")                                                                                                        \
    X(VDupElem, "dup_elem") X(VDupGp, "dup_gp") X(VInsGp, "ins_gp") X(VInsElem, "ins_elem") X(VUMov, "umov") X(VSMov, "smov") \
    X(VMovImm, "movi") X(VOrrImm, "orr_vimm") X(VBicImm, "bic_vimm")                                                         \
    X(VShl, "shl") X(VSShr, "sshr") X(VUShr, "ushr") X(VSSra, "ssra") X(VUSra, "usra") X(VSRShr, "srshr")                   \
    X(VURShr, "urshr") X(VSri, "sri") X(VSli, "sli") X(VShrn, "shrn") X(VRShrn, "rshrn") X(VSShll, "sshll")                 \
    X(VUShll, "ushll")                                                                                                        \
    X(VSAddL, "saddl") X(VUAddL, "uaddl") X(VSSubL, "ssubl") X(VUSubL, "usubl") X(VSAddW, "saddw") X(VUAddW, "uaddw")       \
    X(VSSubW, "ssubw") X(VUSubW, "usubw") X(VSMull, "smull_v") X(VUMull, "umull_v") X(VSMlal, "smlal") X(VUMlal, "umlal")   \
    X(VSMlsl, "smlsl") X(VUMlsl, "umlsl") X(VSAbdl, "sabdl") X(VUAbdl, "uabdl") X(VAddHn, "addhn") X(VSubHn, "subhn")       \
    X(VZip1, "zip1") X(VZip2, "zip2") X(VUzp1, "uzp1") X(VUzp2, "uzp2") X(VTrn1, "trn1") X(VTrn2, "trn2")                   \
    X(VExt, "ext") X(VTbl, "tbl") X(VTbx, "tbx")

enum class Op : uint16_t {
#define RECOMP_A64_ENUM(name, text) name,
    RECOMP_A64_OPS(RECOMP_A64_ENUM)
#undef RECOMP_A64_ENUM
};

enum class AddrMode : uint8_t { Offset, PreIndex, PostIndex, Register, Literal };

// Modos de arredondamento das conversoes e do FRINT (FPRounding do manual + A = ties away, X/I = modo do FPCR)
enum class Rounding : uint8_t { Nearest, PlusInf, MinusInf, Zero, TiesAway, Current };

struct Insn {
    Op op{Op::Unknown};
    uint32_t word{0};
    uint64_t pc{0};          // RVA
    uint8_t rd{0}, rn{0}, rm{0}, ra{0};   // ra tambem e Rt2 em pares; rm e Rs (status) em store exclusivo
    bool sf{false};          // operacao de 64 bits (registrador X)
    bool set_flags{false};
    int64_t imm{0};          // imediato ja decodificado (deslocado/estendido; padrao de bits em fmov/movi)
    uint8_t shift{0};        // tipo: 0 LSL, 1 LSR, 2 ASR, 3 ROR; ou hw*16 em move wide
    uint8_t amount{0};       // quantidade do deslocamento / imm3 de extend
    uint8_t extend{0};       // option do extend (UXTB..SXTX)
    uint8_t cond{0};
    uint8_t immr{0}, imms{0};
    uint8_t bit{0};          // tbz/tbnz
    // Memoria
    uint8_t size{0};         // log2 dos bytes: acesso (0..4, 4 = Q), elemento de vetor ou tipo FP
    bool sign_extend{false}; // LDRSB/LDRSH/LDRSW; SMOV; conversoes com sinal
    bool dest64{true};       // destino X (true) ou W (false) em loads com sinal
    bool ordered{false};     // exclusivos: acquire (LDAXR) / release (STLXR); sempre true em LDAR/STLR
    AddrMode mode{AddrMode::Offset};
    uint64_t target{0};      // RVA de destino (ramificacoes, adr/adrp e literais)
    uint16_t sysreg{0};      // mrs/msr: o0:op1:CRn:CRm:op2
    // SIMD/FP
    bool simd{false};        // registradores V (loads/stores e operacoes de vetor)
    bool q{false};           // vetor de 128 bits (senao 64); em formas longas/estreitas, a variante "2" (metade alta)
    bool scalar{false};      // so o elemento 0 (o resto do registrador e zerado)
    bool element{false};     // forma "por elemento": rm[index] em vez de rm
    uint8_t index{0}, index2{0};
    uint8_t count{0};        // registradores em LD1..LD4/ST1..ST4 e TBL; selem em count2
    uint8_t count2{0};
    uint8_t fsize{0};        // FCVT: log2 do tipo de origem; conversoes FP<->inteiro: fbits (ponto fixo)
    bool general{false};     // conversao/FMOV entre FP escalar e registrador geral W/X (largura em sf)
    Rounding rounding{Rounding::Nearest};
};

Insn Decode(uint32_t word, uint64_t pc);
// SIMD/FP: chamado por Decode para o grupo de dados (op0 = x111) e para loads/stores com V = 1
Insn DecodeSimdData(Insn d, uint32_t word);
Insn DecodeSimdMemory(Insn d, uint32_t word);

const char* OpName(Op op);
// Texto curto para logs e diagnosticos (nao e a sintaxe completa do assembler)
std::string Describe(const Insn& insn);

// System registers usados por jogos do Switch
inline constexpr uint16_t kSysTpidrEl0 = 0x5E82;
inline constexpr uint16_t kSysTpidrroEl0 = 0x5E83;
inline constexpr uint16_t kSysCntvctEl0 = 0x5F02;
inline constexpr uint16_t kSysCntpctEl0 = 0x5F01;
inline constexpr uint16_t kSysCntfrqEl0 = 0x5F00;
inline constexpr uint16_t kSysFpcr = 0x5A20;
inline constexpr uint16_t kSysFpsr = 0x5A21;
inline constexpr uint16_t kSysDczidEl0 = 0x5807;

} // namespace recomp::a64
