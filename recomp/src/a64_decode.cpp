#include "recomp/a64.hpp"

#include <cstdio>

namespace recomp::a64 {

namespace {

constexpr uint32_t Bits(uint32_t w, unsigned hi, unsigned lo) { return (w >> lo) & ((1u << (hi - lo + 1)) - 1); }
constexpr bool Bit(uint32_t w, unsigned b) { return (w >> b) & 1; }

constexpr int64_t SignExtend(uint64_t value, unsigned bits) {
    const uint64_t m = uint64_t{1} << (bits - 1);
    return static_cast<int64_t>((value ^ m) - m);
}

uint64_t Ones(unsigned n) { return n >= 64 ? ~uint64_t{0} : (uint64_t{1} << n) - 1; }

// DecodeBitMasks do manual ARM (imediato logico); false se o encoding e reservado
bool DecodeBitMask(unsigned n, unsigned imms, unsigned immr, unsigned datasize, uint64_t& out) {
    const unsigned combined = (n << 6) | (~imms & 0x3F);
    int len = -1;
    for (int i = 6; i >= 0; --i) {
        if ((combined >> i) & 1) {
            len = i;
            break;
        }
    }
    if (len < 1) return false;
    const unsigned esize = 1u << len;
    if (esize > datasize) return false;
    const unsigned levels = esize - 1;
    if ((imms & levels) == levels) return false;
    const unsigned s = imms & levels, r = immr & levels;
    const uint64_t welem = Ones(s + 1);
    const uint64_t emask = Ones(esize);
    const uint64_t rotated = r == 0 ? welem : ((welem >> r) | (welem << (esize - r))) & emask;
    uint64_t pattern = 0;
    for (unsigned i = 0; i < datasize; i += esize) pattern |= rotated << i;
    out = datasize == 64 ? pattern : pattern & 0xFFFFFFFFull;
    return true;
}

Insn DataProcessingImmediate(Insn d, uint32_t w) {
    const uint32_t op = Bits(w, 25, 23);
    d.sf = Bit(w, 31);
    d.rd = static_cast<uint8_t>(Bits(w, 4, 0));
    d.rn = static_cast<uint8_t>(Bits(w, 9, 5));
    switch (op) {
    case 0b000:
    case 0b001: {  // ADR / ADRP
        const int64_t imm = SignExtend((Bits(w, 23, 5) << 2) | Bits(w, 30, 29), 21);
        if (Bit(w, 31)) {
            d.op = Op::Adrp;
            d.target = (d.pc & ~uint64_t{0xFFF}) + static_cast<uint64_t>(imm * 4096);
        } else {
            d.op = Op::Adr;
            d.target = d.pc + static_cast<uint64_t>(imm);
        }
        d.sf = true;
        return d;
    }
    case 0b010: {  // add/sub imediato
        d.op = Bit(w, 30) ? Op::SubImm : Op::AddImm;
        d.set_flags = Bit(w, 29);
        d.imm = static_cast<int64_t>(Bits(w, 21, 10)) << (Bit(w, 22) ? 12 : 0);
        return d;
    }
    case 0b100: {  // logico imediato
        uint64_t mask = 0;
        if ((!d.sf && Bit(w, 22)) || !DecodeBitMask(Bit(w, 22), Bits(w, 15, 10), Bits(w, 21, 16), d.sf ? 64 : 32, mask)) return d;
        static constexpr Op ops[4] = {Op::AndImm, Op::OrrImm, Op::EorImm, Op::AndImm};
        d.op = ops[Bits(w, 30, 29)];
        d.set_flags = Bits(w, 30, 29) == 3;
        d.imm = static_cast<int64_t>(mask);
        return d;
    }
    case 0b101: {  // move wide
        const uint32_t opc = Bits(w, 30, 29), hw = Bits(w, 22, 21);
        if (opc == 1 || (!d.sf && hw >= 2)) return d;
        d.op = opc == 0 ? Op::MovN : opc == 2 ? Op::MovZ : Op::MovK;
        d.shift = static_cast<uint8_t>(hw * 16);
        d.imm = Bits(w, 20, 5);
        return d;
    }
    case 0b110: {  // bitfield
        const uint32_t opc = Bits(w, 30, 29);
        if (opc == 3 || Bit(w, 22) != d.sf || (!d.sf && (Bit(w, 21) || Bit(w, 15)))) return d;
        d.op = opc == 0 ? Op::Sbfm : opc == 1 ? Op::Bfm : Op::Ubfm;
        d.immr = static_cast<uint8_t>(Bits(w, 21, 16));
        d.imms = static_cast<uint8_t>(Bits(w, 15, 10));
        return d;
    }
    case 0b111: {  // extract
        if (Bits(w, 30, 29) != 0 || Bit(w, 21) || Bit(w, 22) != d.sf || (!d.sf && Bit(w, 15))) return d;
        d.op = Op::Extr;
        d.rm = static_cast<uint8_t>(Bits(w, 20, 16));
        d.imms = static_cast<uint8_t>(Bits(w, 15, 10));
        return d;
    }
    default:
        return d;
    }
}

Insn BranchesSystem(Insn d, uint32_t w) {
    if (Bits(w, 30, 26) == 0b00101) {  // B / BL
        d.op = Bit(w, 31) ? Op::Bl : Op::B;
        d.target = d.pc + static_cast<uint64_t>(SignExtend(Bits(w, 25, 0), 26) * 4);
        return d;
    }
    if (Bits(w, 30, 25) == 0b011010) {  // CBZ / CBNZ
        d.op = Bit(w, 24) ? Op::Cbnz : Op::Cbz;
        d.sf = Bit(w, 31);
        d.rd = static_cast<uint8_t>(Bits(w, 4, 0));
        d.target = d.pc + static_cast<uint64_t>(SignExtend(Bits(w, 23, 5), 19) * 4);
        return d;
    }
    if (Bits(w, 30, 25) == 0b011011) {  // TBZ / TBNZ
        d.op = Bit(w, 24) ? Op::Tbnz : Op::Tbz;
        d.bit = static_cast<uint8_t>((Bit(w, 31) << 5) | Bits(w, 23, 19));
        d.sf = Bit(w, 31);
        d.rd = static_cast<uint8_t>(Bits(w, 4, 0));
        d.target = d.pc + static_cast<uint64_t>(SignExtend(Bits(w, 18, 5), 14) * 4);
        return d;
    }
    if (Bits(w, 31, 24) == 0b01010100 && !Bit(w, 4)) {  // B.cond
        d.op = Op::BCond;
        d.cond = static_cast<uint8_t>(Bits(w, 3, 0));
        d.target = d.pc + static_cast<uint64_t>(SignExtend(Bits(w, 23, 5), 19) * 4);
        return d;
    }
    if (Bits(w, 31, 25) == 0b1101011) {  // branch para registrador (sem autenticacao de ponteiro)
        if (Bits(w, 20, 16) != 0b11111 || Bits(w, 15, 10) != 0 || Bits(w, 4, 0) != 0) return d;
        const uint32_t opc = Bits(w, 24, 21);
        d.rn = static_cast<uint8_t>(Bits(w, 9, 5));
        d.op = opc == 0 ? Op::Br : opc == 1 ? Op::Blr : opc == 2 ? Op::Ret : Op::Unknown;
        return d;
    }
    if (Bits(w, 31, 24) == 0b11010100) {  // excecoes
        const uint32_t opc = Bits(w, 23, 21), ll = Bits(w, 1, 0);
        d.imm = Bits(w, 20, 5);
        if (opc == 0 && ll == 1) d.op = Op::Svc;
        else if (opc == 1 && ll == 0) d.op = Op::Brk;
        return d;
    }
    if (Bits(w, 31, 22) == 0b1101010100) {  // sistema
        d.rd = static_cast<uint8_t>(Bits(w, 4, 0));
        if ((w & 0xFFFFF01F) == 0xD503201F) {
            d.op = Op::Hint;
            d.imm = Bits(w, 11, 5);
        } else if ((w & 0xFFFFF0FF) == 0xD503309F || (w & 0xFFFFF0FF) == 0xD50330BF || (w & 0xFFFFF0FF) == 0xD50330DF) {
            d.op = Op::Barrier;
            d.imm = Bits(w, 11, 8);
        } else if ((w & 0xFFFFF0FF) == 0xD503305F) {
            d.op = Op::Clrex;
        } else if (Bit(w, 20)) {  // op0 = 2 ou 3: MRS (L = bit 21 = 1) / MSR registrador (L = 0); op0 = 1 e SYS/DC (R3)
            d.op = Bit(w, 21) ? Op::Mrs : Op::Msr;
            d.sysreg = static_cast<uint16_t>(Bits(w, 20, 5) & 0x7FFF);
        }
        return d;
    }
    return d;
}

Insn LoadsStores(Insn d, uint32_t w) {
    if (Bit(w, 26)) return d;  // SIMD/FP (R3)
    d.rd = static_cast<uint8_t>(Bits(w, 4, 0));
    d.rn = static_cast<uint8_t>(Bits(w, 9, 5));

    if (Bits(w, 29, 27) == 0b011 && Bits(w, 25, 24) == 0) {  // LDR literal
        const uint32_t opc = Bits(w, 31, 30);
        d.target = d.pc + static_cast<uint64_t>(SignExtend(Bits(w, 23, 5), 19) * 4);
        d.mode = AddrMode::Literal;
        if (opc == 3) {
            d.op = Op::Prefetch;
            return d;
        }
        d.op = Op::Load;
        d.size = opc == 0 ? 2 : opc == 1 ? 3 : 2;
        d.sign_extend = opc == 2;
        d.dest64 = opc != 0;
        return d;
    }

    if (Bits(w, 29, 24) == 0b001000) {  // exclusivos e acquire/release; CAS/CASP (v8.1, o1 = 1 com size 0x ou o2 = 1) ficam de fora
        const uint32_t size = Bits(w, 31, 30);
        const bool o2 = Bit(w, 23), load = Bit(w, 22), o1 = Bit(w, 21);
        d.rm = static_cast<uint8_t>(Bits(w, 20, 16));
        d.ra = static_cast<uint8_t>(Bits(w, 14, 10));
        d.ordered = Bit(w, 15);
        if (!o2 && !o1) {
            d.op = load ? Op::LoadExclusive : Op::StoreExclusive;
            d.size = static_cast<uint8_t>(size);
        } else if (!o2 && (size & 2)) {
            d.op = load ? Op::LoadExclusivePair : Op::StoreExclusivePair;
            d.size = static_cast<uint8_t>(size);  // 2: par de W, 3: par de X
        } else if (o2 && !o1 && d.ordered) {
            d.op = load ? Op::LoadAcquire : Op::StoreRelease;
            d.size = static_cast<uint8_t>(size);
        }
        return d;
    }

    if (Bits(w, 29, 27) == 0b101) {  // pares
        const uint32_t opc = Bits(w, 31, 30), mode = Bits(w, 24, 23);
        const bool load = Bit(w, 22);
        if (opc == 3 || (opc == 1 && !load)) return d;  // 01 com L=0 e STGP (MTE)
        d.op = load ? Op::LoadPair : Op::StorePair;
        d.size = opc == 2 ? 3 : 2;
        d.sign_extend = opc == 1;
        d.dest64 = opc != 0;
        d.ra = static_cast<uint8_t>(Bits(w, 14, 10));
        d.imm = SignExtend(Bits(w, 21, 15), 7) << d.size;
        d.mode = mode == 1 ? AddrMode::PostIndex : mode == 3 ? AddrMode::PreIndex : AddrMode::Offset;
        return d;
    }

    if (Bits(w, 29, 27) != 0b111) return d;  // exclusivos/atomicos e o restante ficam para a R3
    const uint32_t size = Bits(w, 31, 30), opc = Bits(w, 23, 22);
    // opc: 00 store, 01 load, 10 load com sinal para X (ou PRFM com size 3), 11 load com sinal para W
    const auto classify = [&](Insn& insn) {
        insn.size = static_cast<uint8_t>(size);
        if (opc == 0) {
            insn.op = Op::Store;
        } else if (opc == 1) {
            insn.op = Op::Load;
            insn.dest64 = size == 3;
        } else if (size == 3) {
            insn.op = opc == 2 ? Op::Prefetch : Op::Unknown;
        } else if (opc == 2) {
            insn.op = Op::Load;
            insn.sign_extend = true;
            insn.dest64 = true;
        } else if (size <= 1) {
            insn.op = Op::Load;
            insn.sign_extend = true;
            insn.dest64 = false;
        }
    };
    if (Bit(w, 24)) {  // offset sem sinal escalado
        classify(d);
        d.imm = static_cast<int64_t>(Bits(w, 21, 10)) << size;
        d.mode = AddrMode::Offset;
        return d;
    }
    if (Bits(w, 25, 24) != 0) return d;
    if (!Bit(w, 21)) {
        const uint32_t kind = Bits(w, 11, 10);
        if (kind == 2) return d;  // LDTR/STTR (nao privilegiado): nao aparece em codigo de usuario
        classify(d);
        d.imm = SignExtend(Bits(w, 20, 12), 9);
        d.mode = kind == 0 ? AddrMode::Offset : kind == 1 ? AddrMode::PostIndex : AddrMode::PreIndex;
        return d;
    }
    if (Bits(w, 11, 10) == 2) {  // offset por registrador
        const uint32_t option = Bits(w, 15, 13);
        if (!(option & 2)) return d;  // option 0x0/0x1 sao reservados
        classify(d);
        d.rm = static_cast<uint8_t>(Bits(w, 20, 16));
        d.extend = static_cast<uint8_t>(option);
        d.amount = Bit(w, 12) ? static_cast<uint8_t>(size) : 0;
        d.mode = AddrMode::Register;
        return d;
    }
    return d;  // atomicos LSE (R3)
}

Insn DataProcessingRegister(Insn d, uint32_t w) {
    d.sf = Bit(w, 31);
    d.rd = static_cast<uint8_t>(Bits(w, 4, 0));
    d.rn = static_cast<uint8_t>(Bits(w, 9, 5));
    d.rm = static_cast<uint8_t>(Bits(w, 20, 16));
    if (Bits(w, 28, 24) == 0b01010) {  // logico com deslocamento
        if (!d.sf && Bit(w, 15)) return d;
        static constexpr Op ops[8] = {Op::And, Op::Bic, Op::Orr, Op::Orn, Op::Eor, Op::Eon, Op::And, Op::Bic};
        const uint32_t opc = Bits(w, 30, 29), n = Bit(w, 21);
        d.op = ops[opc * 2 + n];
        d.set_flags = opc == 3;
        d.shift = static_cast<uint8_t>(Bits(w, 23, 22));
        d.amount = static_cast<uint8_t>(Bits(w, 15, 10));
        return d;
    }
    if (Bits(w, 28, 24) == 0b01011) {
        d.set_flags = Bit(w, 29);
        if (!Bit(w, 21)) {  // add/sub com deslocamento
            if (Bits(w, 23, 22) == 3 || (!d.sf && Bit(w, 15))) return d;
            d.op = Bit(w, 30) ? Op::SubShift : Op::AddShift;
            d.shift = static_cast<uint8_t>(Bits(w, 23, 22));
            d.amount = static_cast<uint8_t>(Bits(w, 15, 10));
            return d;
        }
        if (Bits(w, 23, 22) != 0 || Bits(w, 12, 10) > 4) return d;  // add/sub estendido
        d.op = Bit(w, 30) ? Op::SubExt : Op::AddExt;
        d.extend = static_cast<uint8_t>(Bits(w, 15, 13));
        d.amount = static_cast<uint8_t>(Bits(w, 12, 10));
        return d;
    }
    if (Bits(w, 28, 21) == 0b11010000) {  // adc/sbc
        if (Bits(w, 15, 10) != 0) return d;
        d.op = Bit(w, 30) ? Op::Sbc : Op::Adc;
        d.set_flags = Bit(w, 29);
        return d;
    }
    if (Bits(w, 28, 21) == 0b11010010) {  // ccmp/ccmn
        if (!Bit(w, 29) || Bit(w, 10) || Bit(w, 4)) return d;
        d.op = Bit(w, 30) ? Op::Ccmp : Op::Ccmn;
        d.cond = static_cast<uint8_t>(Bits(w, 15, 12));
        d.imm = Bits(w, 3, 0);            // nzcv quando a condicao falha
        d.mode = Bit(w, 11) ? AddrMode::Offset : AddrMode::Register;  // Offset = rm e imediato de 5 bits
        return d;
    }
    if (Bits(w, 28, 21) == 0b11010100) {  // csel/csinc/csinv/csneg
        if (Bit(w, 29) || Bit(w, 11)) return d;
        static constexpr Op ops[4] = {Op::Csel, Op::Csinc, Op::Csinv, Op::Csneg};
        d.op = ops[Bit(w, 30) * 2 + Bit(w, 10)];
        d.cond = static_cast<uint8_t>(Bits(w, 15, 12));
        return d;
    }
    if (Bits(w, 28, 21) == 0b11010110) {
        const uint32_t opcode = Bits(w, 15, 10);
        if (!Bit(w, 30)) {  // 2 fontes
            if (Bit(w, 29)) return d;
            switch (opcode) {
            case 0b000010: d.op = Op::Udiv; break;
            case 0b000011: d.op = Op::Sdiv; break;
            case 0b001000: d.op = Op::Lslv; break;
            case 0b001001: d.op = Op::Lsrv; break;
            case 0b001010: d.op = Op::Asrv; break;
            case 0b001011: d.op = Op::Rorv; break;
            default: break;  // CRC32 (R3)
            }
            return d;
        }
        if (Bit(w, 29) || Bits(w, 20, 16) != 0) return d;  // 1 fonte
        switch (opcode) {
        case 0: d.op = Op::Rbit; break;
        case 1: d.op = Op::Rev16; break;
        case 2: d.op = d.sf ? Op::Rev32 : Op::Rev; break;
        case 3: d.op = d.sf ? Op::Rev : Op::Unknown; break;
        case 4: d.op = Op::Clz; break;
        case 5: d.op = Op::Cls; break;
        default: break;
        }
        return d;
    }
    if (Bits(w, 28, 24) == 0b11011) {  // 3 fontes
        if (Bits(w, 30, 29) != 0) return d;
        d.ra = static_cast<uint8_t>(Bits(w, 14, 10));
        const uint32_t op31 = Bits(w, 23, 21), o0 = Bit(w, 15);
        if (op31 == 0) d.op = o0 ? Op::Msub : Op::Madd;
        else if (!d.sf) return d;
        else if (op31 == 1) d.op = o0 ? Op::Smsubl : Op::Smaddl;
        else if (op31 == 5) d.op = o0 ? Op::Umsubl : Op::Umaddl;
        else if (op31 == 2 && !o0) d.op = Op::Smulh;
        else if (op31 == 6 && !o0) d.op = Op::Umulh;
        return d;
    }
    return d;
}

} // namespace

Insn Decode(uint32_t word, uint64_t pc) {
    Insn d;
    d.word = word;
    d.pc = pc;
    const uint32_t op0 = Bits(word, 28, 25);
    if ((word >> 16) == 0) {  // UDF #imm16 (permanentemente indefinida: o jogo usa como armadilha)
        d.op = Op::Udf;
        d.imm = Bits(word, 15, 0);
        return d;
    }
    if ((op0 & 0b1110) == 0b1000) return DataProcessingImmediate(d, word);
    if ((op0 & 0b1110) == 0b1010) return BranchesSystem(d, word);
    if ((op0 & 0b0101) == 0b0100) return LoadsStores(d, word);
    if ((op0 & 0b0111) == 0b0101) return DataProcessingRegister(d, word);
    return d;  // SIMD/FP e nao alocados
}

const char* OpName(Op op) {
    static const char* names[] = {
        "unknown", "add_imm", "sub_imm", "movz", "movn", "movk", "and_imm", "orr_imm", "eor_imm", "adr", "adrp", "sbfm", "bfm",
        "ubfm", "extr", "b", "bl", "b.cond", "cbz", "cbnz", "tbz", "tbnz", "br", "blr", "ret", "load", "store", "load_pair",
        "store_pair", "prefetch", "load_exclusive", "store_exclusive", "load_exclusive_pair",
        "store_exclusive_pair", "load_acquire", "store_release", "and", "bic", "orr", "orn", "eor", "eon", "add_shift", "sub_shift", "add_ext", "sub_ext",
        "adc", "sbc", "csel", "csinc", "csinv", "csneg", "ccmp", "ccmn", "udiv", "sdiv", "lslv", "lsrv", "asrv", "rorv", "rbit",
        "rev16", "rev32", "rev", "clz", "cls", "madd", "msub", "smaddl", "smsubl", "umaddl", "umsubl", "smulh", "umulh",
        "hint", "barrier", "clrex", "mrs", "msr", "svc", "brk", "udf",
    };
    const auto index = static_cast<size_t>(op);
    return index < sizeof(names) / sizeof(names[0]) ? names[index] : "?";
}

std::string Describe(const Insn& insn) {
    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), "%08x %-10s sf=%d s=%d rd=%u rn=%u rm=%u ra=%u imm=%lld target=%llx", insn.word,
                  OpName(insn.op), insn.sf, insn.set_flags, insn.rd, insn.rn, insn.rm, insn.ra, static_cast<long long>(insn.imm),
                  static_cast<unsigned long long>(insn.target));
    return buffer;
}

} // namespace recomp::a64
