// Resolucao de tabelas de salto (switch do clang) dentro de uma funcao
//
// Forma reconhecida (registradores e ordem podem variar; a base da tabela pode vir de bem antes):
//   cmp  wI, #N ; b.hi default            (ou b.hs com N entradas)
//   adrp xT, tabela ; add xT, xT, :lo12:tabela
//   adr  xB, base                         (ou adrp + add)
//   ldrb/ldrh/ldrsw/ldr wE, [xT, xI{, lsl #s}]
//   add  xR, xB, xE{, lsl #k | sxtw #k | uxtw #k}
//   br   xR
// As definicoes sao procuradas para tras, linearmente, ate o inicio da funcao. Qualquer escrita intermediaria nao
// reconhecida, destino fora da funcao ou desalinhado invalida a tabela inteira (o BR volta a passar pelo Dispatch).

#include "recomp/jump_tables.hpp"

#include <cstring>
#include <set>

namespace recomp {

namespace {

using a64::AddrMode;
using a64::Insn;
using a64::Op;

// Registradores gerais que a instrucao escreve (conservador: na duvida, escreve)
bool Writes(const Insn& d, unsigned r) {
    if (r == 31) return false;
    const bool writeback = d.mode == AddrMode::PreIndex || d.mode == AddrMode::PostIndex;
    switch (d.op) {
    case Op::B: case Op::BCond: case Op::Cbz: case Op::Cbnz: case Op::Tbz: case Op::Tbnz: case Op::Br: case Op::Ret:
    case Op::Hint: case Op::Barrier: case Op::Clrex: case Op::Prefetch: case Op::Msr: case Op::Brk: case Op::Udf:
        return false;
    case Op::Bl: case Op::Blr: case Op::Svc:
        return r <= 18 || r == 30;  // chamadas destroem os registradores temporarios
    case Op::Store: case Op::StorePair: case Op::StoreRelease: case Op::StoreStruct: case Op::StoreLane:
        return writeback && d.rn == r;
    case Op::StoreExclusive: case Op::StoreExclusivePair:
        return d.rm == r;
    case Op::Load: case Op::LoadPair:
        if (writeback && d.rn == r) return true;
        if (d.simd) return false;
        return d.rd == r || (d.op == Op::LoadPair && d.ra == r);
    case Op::LoadStruct: case Op::LoadLane: case Op::LoadReplicate:
        return d.mode == AddrMode::PostIndex && d.rn == r;
    case Op::LoadExclusivePair:
        return d.rd == r || d.ra == r;
    case Op::Ccmp: case Op::Ccmn:
        return false;
    default:
        break;
    }
    if (d.simd) {
        // SIMD/FP escreve registradores V, exceto as formas que vao para W/X
        const bool to_gp = d.op == Op::FMovToGp || d.op == Op::VUMov || d.op == Op::VSMov || (d.general && d.op == Op::FCvtToInt);
        return to_gp && d.rd == r;
    }
    if (d.set_flags && d.rd == 31) return false;  // cmp/cmn/tst
    return d.rd == r;
}

int Definition(const std::vector<Insn>& insns, int before, unsigned r) {
    for (int j = before - 1; j >= 0; --j) {
        if (Writes(insns[j], r)) return j;
    }
    return -1;
}

// Valor constante (RVA) de um registrador montado com ADR ou ADRP + ADD
bool Constant(const std::vector<Insn>& insns, int before, unsigned r, uint64_t& value) {
    const int j = Definition(insns, before, r);
    if (j < 0) return false;
    const Insn& d = insns[j];
    if (d.op == Op::Adr || d.op == Op::Adrp) {
        value = d.target;
        return true;
    }
    if (d.op == Op::AddImm && !d.set_flags && d.sf && d.rd == r) {
        uint64_t base = 0;
        if (!Constant(insns, j, d.rn, base)) return false;
        value = base + static_cast<uint64_t>(d.imm);
        return true;
    }
    return false;
}

}  // namespace

std::map<uint64_t, std::vector<uint64_t>> ResolveJumpTables(const NsoModule& module, const FunctionRange& function, const std::vector<Insn>& insns) {
    std::map<uint64_t, std::vector<uint64_t>> tables;
    const auto image = module.Image();
    for (int k = 0; k < static_cast<int>(insns.size()); ++k) {
        const Insn& br = insns[k];
        if (br.op != Op::Br || br.rn == 17 || br.rn == 31) continue;
        // add xR, xB, xE, (lsl|ext) #s
        const int ja = Definition(insns, k, br.rn);
        if (ja < 0) continue;
        const Insn& add = insns[ja];
        if ((add.op != Op::AddShift && add.op != Op::AddExt) || add.set_flags || !add.sf || add.rd != br.rn) continue;
        if (add.op == Op::AddShift && add.shift != 0) continue;
        uint64_t base = 0;
        if (!Constant(insns, ja, add.rn, base)) continue;
        // ldr wE, [xT, xI, ...]
        const int jl = Definition(insns, ja, add.rm);
        if (jl < 0) continue;
        const Insn& load = insns[jl];
        if (load.op != Op::Load || load.simd || load.mode != AddrMode::Register || load.rd != add.rm || load.size > 2) continue;
        if (load.amount != 0 && load.amount != load.size) continue;
        uint64_t table = 0;
        if (!Constant(insns, jl, load.rn, table)) continue;
        const unsigned index = load.rm;
        // Guarda: cmp wI/xI, #N seguido de b.hi (N + 1 entradas) ou b.hs (N entradas), sem escrita em I no meio
        int jc = -1;
        for (int j = jl - 1; j >= 0; --j) {
            const Insn& d = insns[j];
            if (d.op == Op::SubImm && d.set_flags && d.rd == 31 && d.rn == index) {
                jc = j;
                break;
            }
            if (Writes(d, index)) break;
        }
        if (jc < 0) continue;
        uint64_t entries = 0;
        for (int j = jc + 1; j < jl; ++j) {
            if (insns[j].op != Op::BCond) continue;
            if (insns[j].cond == 8) entries = static_cast<uint64_t>(insns[jc].imm) + 1;       // HI
            else if (insns[j].cond == 2) entries = static_cast<uint64_t>(insns[jc].imm);  // HS
            break;
        }
        if (entries == 0 || entries > 4096) continue;
        const uint64_t esize = uint64_t{1} << load.size;
        if (table + entries * esize > image.size()) continue;

        std::set<uint64_t> targets;
        bool ok = true;
        for (uint64_t i = 0; i < entries && ok; ++i) {
            uint64_t raw = 0;
            std::memcpy(&raw, image.data() + table + i * esize, esize);
            // Valor do registrador E depois do load (W zera a parte alta; LDRSW estende para 64)
            uint64_t e = raw;
            if (load.sign_extend) {
                const unsigned bits = 8u << load.size;
                const uint64_t m = uint64_t{1} << (bits - 1);
                e = (raw ^ m) - m;
                if (!load.dest64) e &= 0xFFFFFFFFull;
            }
            // Operando estendido do add
            uint64_t off = e;
            if (add.op == Op::AddExt) {
                switch (add.extend) {
                case 0: off = e & 0xFF; break;
                case 1: off = e & 0xFFFF; break;
                case 2: off = e & 0xFFFFFFFFull; break;
                case 4: off = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(e))); break;
                case 5: off = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(e))); break;
                case 6: off = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(e))); break;
                default: break;
                }
            }
            const uint64_t target = base + (off << add.amount);
            ok = target % 4 == 0 && target >= function.start && target < function.start + function.size;
            targets.insert(target);
        }
        if (ok) tables[br.pc].assign(targets.begin(), targets.end());
    }
    return tables;
}

} // namespace recomp
