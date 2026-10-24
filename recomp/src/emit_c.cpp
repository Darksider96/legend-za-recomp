#include "recomp/emit_c.hpp"

#include <cstring>
#include <format>

namespace recomp {

namespace {

using a64::AddrMode;
using a64::Insn;
using a64::Op;

const char* Type(bool sf) { return sf ? "uint64_t" : "uint32_t"; }
const char* SignedType(bool sf) { return sf ? "int64_t" : "int32_t"; }

// Leitura de registrador: 31 = XZR (zero) ou SP, conforme a instrucao
std::string Reg(unsigned n, bool sf, bool sp = false) {
    if (n == 31) return sp ? (sf ? "c.sp" : "static_cast<uint32_t>(c.sp)") : (sf ? "uint64_t{0}" : "uint32_t{0}");
    return sf ? std::format("c.x[{}]", n) : std::format("static_cast<uint32_t>(c.x[{}])", n);
}

// Escrita: 32 bits zeram a parte alta; XZR descarta (mas avalia a expressao, que pode atualizar flags)
std::string Write(unsigned n, bool sf, const std::string& value, bool sp = false) {
    if (n == 31 && !sp) return std::format("(void)({});", value);
    const std::string dst = n == 31 ? "c.sp" : std::format("c.x[{}]", n);
    return sf ? std::format("{} = {};", dst, value) : std::format("{} = static_cast<uint32_t>({});", dst, value);
}

std::string Shifted(const Insn& d) {
    const std::string rm = Reg(d.rm, d.sf);
    if (d.amount == 0 && d.shift != 3) return rm;
    switch (d.shift) {
    case 0: return std::format("static_cast<{}>({} << {})", Type(d.sf), rm, d.amount);
    case 1: return std::format("static_cast<{}>({} >> {})", Type(d.sf), rm, d.amount);
    case 2: return std::format("static_cast<{}>(static_cast<{}>({}) >> {})", Type(d.sf), SignedType(d.sf), rm, d.amount);
    default: return std::format("recomp::RotateRight<{}>({}, {})", Type(d.sf), rm, d.amount);
    }
}

// Operando estendido (UXTB..SXTX) com deslocamento; o resultado tem o tipo da operacao
std::string Extended(unsigned rm, unsigned option, unsigned amount, bool sf) {
    static const char* casts[8] = {"uint8_t", "uint16_t", "uint32_t", "uint64_t", "int8_t", "int16_t", "int32_t", "int64_t"};
    const std::string value = std::format("static_cast<{}>(static_cast<{}>({}))", sf ? "int64_t" : "int32_t", casts[option],
                                          Reg(rm, (option & 3) == 3));
    const std::string typed = std::format("static_cast<{}>({})", Type(sf), value);
    return amount ? std::format("static_cast<{}>({} << {})", Type(sf), typed, amount) : typed;
}

std::string Hex(uint64_t v) { return std::format("0x{:x}", v); }

// Endereco absoluto do jogo para um RVA
std::string Guest(uint64_t rva) { return std::format("(recomp::kImageBase + {}ull)", Hex(rva)); }

struct Emitter {
    const FunctionRange& function;
    EmitResult& out;
    const std::set<uint64_t>& labels;

    bool Inside(uint64_t rva) const { return rva >= function.start && rva < function.start + function.size; }

    std::string Jump(uint64_t target) {
        if (Inside(target)) return std::format("goto L_{:x};", target);
        out.callees.insert(target);
        return std::format("{{ {}(c); return; }}", FunctionName(target));
    }

    void Line(const std::string& text) { out.code += "    " + text + "\n"; }

    void Unsupported(const Insn& d) {
        out.unsupported.push_back(d);
        Line(std::format("recomp::Unimplemented(c, {}, {});  // {}", Hex(d.pc), Hex(d.word), a64::OpName(d.op)));
    }

    std::string MemoryType(const Insn& d) const {
        static const char* types[4] = {"uint8_t", "uint16_t", "uint32_t", "uint64_t"};
        return types[d.size];
    }

    // Valor carregado ja estendido para 64 bits (zero ou sinal) como o registrador final espera
    std::string Loaded(const Insn& d, const std::string& address) const {
        const std::string raw = std::format("recomp::Load<{}>({})", MemoryType(d), address);
        if (!d.sign_extend) return raw;
        static const char* signed_types[4] = {"int8_t", "int16_t", "int32_t", "int64_t"};
        return d.dest64 ? std::format("static_cast<uint64_t>(static_cast<int64_t>(static_cast<{}>({})))", signed_types[d.size], raw)
                        : std::format("static_cast<uint32_t>(static_cast<int32_t>(static_cast<{}>({})))", signed_types[d.size], raw);
    }

    void Memory(const Insn& d) {
        const bool pair = d.op == Op::LoadPair || d.op == Op::StorePair;
        const bool load = d.op == Op::Load || d.op == Op::LoadPair;
        std::string address;
        switch (d.mode) {
        case AddrMode::Literal: address = Guest(d.target); break;
        case AddrMode::Register: address = std::format("{} + {}", Reg(d.rn, true, true), Extended(d.rm, d.extend, d.amount, true)); break;
        case AddrMode::PostIndex: address = Reg(d.rn, true, true); break;
        default: address = std::format("{} + static_cast<uint64_t>({}ll)", Reg(d.rn, true, true), d.imm); break;
        }
        std::string body = std::format("{{ const uint64_t a = {}; ", address);
        const uint64_t stride = uint64_t{1} << d.size;
        const auto access = [&](unsigned reg, const std::string& at) {
            if (load) {
                const bool wide = d.sign_extend ? d.dest64 : d.size == 3;
                return reg == 31 ? std::string() : Write(reg, wide, Loaded(d, at)) + " ";
            }
            return std::format("recomp::Store<{}>({}, static_cast<{}>({})); ", MemoryType(d), at, MemoryType(d), Reg(reg, d.size == 3));
        };
        body += access(d.rd, "a");
        if (pair) body += access(d.ra, std::format("a + {}", stride));
        if (d.mode == AddrMode::PreIndex || d.mode == AddrMode::PostIndex) {
            body += d.mode == AddrMode::PreIndex ? std::format("{} = a; ", d.rn == 31 ? "c.sp" : std::format("c.x[{}]", d.rn))
                                                 : std::format("{} = a + static_cast<uint64_t>({}ll); ", d.rn == 31 ? "c.sp" : std::format("c.x[{}]", d.rn), d.imm);
        }
        Line(body + "}");
    }

    void Emit(const Insn& d) {
        const char* T = Type(d.sf);
        const unsigned width = d.sf ? 64 : 32;
        switch (d.op) {
        case Op::AddImm:
        case Op::SubImm: {
            const bool sub = d.op == Op::SubImm;
            const std::string operand = std::format("static_cast<{}>({}{}ull)", T, sub ? "~" : "", Hex(static_cast<uint64_t>(d.imm)));
            Line(Write(d.rd, d.sf, std::format("recomp::AddWithCarry<{}>(c, {}, {}, {}, {})", T, Reg(d.rn, d.sf, true), operand, sub, d.set_flags),
                       !d.set_flags));
            break;
        }
        case Op::AddShift:
        case Op::SubShift:
        case Op::AddExt:
        case Op::SubExt: {
            const bool sub = d.op == Op::SubShift || d.op == Op::SubExt;
            const bool ext = d.op == Op::AddExt || d.op == Op::SubExt;
            const std::string op2 = ext ? Extended(d.rm, d.extend, d.amount, d.sf) : Shifted(d);
            const std::string operand = sub ? std::format("static_cast<{}>(~{})", T, op2) : op2;
            Line(Write(d.rd, d.sf, std::format("recomp::AddWithCarry<{}>(c, {}, {}, {}, {})", T, Reg(d.rn, d.sf, ext), operand, sub, d.set_flags),
                       ext && !d.set_flags));
            break;
        }
        case Op::Adc:
        case Op::Sbc: {
            const std::string operand = d.op == Op::Sbc ? std::format("static_cast<{}>(~{})", T, Reg(d.rm, d.sf)) : Reg(d.rm, d.sf);
            Line(Write(d.rd, d.sf, std::format("recomp::AddWithCarry<{}>(c, {}, {}, c.c, {})", T, Reg(d.rn, d.sf), operand, d.set_flags)));
            break;
        }
        case Op::MovZ: Line(Write(d.rd, d.sf, std::format("static_cast<{}>({}ull << {})", T, Hex(static_cast<uint64_t>(d.imm)), d.shift))); break;
        case Op::MovN: Line(Write(d.rd, d.sf, std::format("static_cast<{}>(~({}ull << {}))", T, Hex(static_cast<uint64_t>(d.imm)), d.shift))); break;
        case Op::MovK:
            Line(Write(d.rd, d.sf, std::format("static_cast<{}>(({} & ~(0xffffull << {})) | ({}ull << {}))", T, Reg(d.rd, d.sf), d.shift,
                                               Hex(static_cast<uint64_t>(d.imm)), d.shift)));
            break;
        case Op::AndImm:
        case Op::OrrImm:
        case Op::EorImm: {
            const char* op = d.op == Op::AndImm ? "&" : d.op == Op::OrrImm ? "|" : "^";
            const std::string value = std::format("static_cast<{}>({} {} {}ull)", T, Reg(d.rn, d.sf), op, Hex(static_cast<uint64_t>(d.imm)));
            if (d.set_flags) {
                Line(std::format("{{ const {} r = {}; recomp::SetLogicFlags(c, r); {} }}", T, value, Write(d.rd, d.sf, "r")));
            } else {
                Line(Write(d.rd, d.sf, value, true));
            }
            break;
        }
        case Op::And: case Op::Bic: case Op::Orr: case Op::Orn: case Op::Eor: case Op::Eon: {
            const bool invert = d.op == Op::Bic || d.op == Op::Orn || d.op == Op::Eon;
            const char* op = (d.op == Op::And || d.op == Op::Bic) ? "&" : (d.op == Op::Orr || d.op == Op::Orn) ? "|" : "^";
            const std::string operand = invert ? std::format("static_cast<{}>(~{})", T, Shifted(d)) : Shifted(d);
            const std::string value = std::format("static_cast<{}>({} {} {})", T, Reg(d.rn, d.sf), op, operand);
            if (d.set_flags) {
                Line(std::format("{{ const {} r = {}; recomp::SetLogicFlags(c, r); {} }}", T, value, Write(d.rd, d.sf, "r")));
            } else {
                Line(Write(d.rd, d.sf, value));
            }
            break;
        }
        case Op::Adr:
        case Op::Adrp: Line(Write(d.rd, true, Guest(d.target))); break;
        case Op::Sbfm:
        case Op::Ubfm:
        case Op::Bfm:
            Line(Write(d.rd, d.sf, std::format("recomp::Bitfield<{}>({}, {}, {}, {}, {}, {})", T, Reg(d.rd, d.sf), Reg(d.rn, d.sf), d.immr, d.imms,
                                               d.op == Op::Sbfm, d.op == Op::Bfm)));
            break;
        case Op::Extr: Line(Write(d.rd, d.sf, std::format("recomp::Extract<{}>({}, {}, {})", T, Reg(d.rn, d.sf), Reg(d.rm, d.sf), d.imms))); break;
        case Op::Csel: case Op::Csinc: case Op::Csinv: case Op::Csneg: {
            const std::string rm = Reg(d.rm, d.sf);
            const std::string other = d.op == Op::Csel ? rm
                                      : d.op == Op::Csinc ? std::format("static_cast<{}>({} + 1)", T, rm)
                                      : d.op == Op::Csinv ? std::format("static_cast<{}>(~{})", T, rm)
                                                          : std::format("static_cast<{}>({}{{0}} - {})", T, T, rm);
            Line(Write(d.rd, d.sf, std::format("recomp::Condition(c, {}) ? {} : {}", d.cond, Reg(d.rn, d.sf), other)));
            break;
        }
        case Op::Ccmp:
        case Op::Ccmn: {
            const std::string operand = d.mode == AddrMode::Offset ? std::format("{}{{{}}}", T, d.rm) : Reg(d.rm, d.sf);
            const bool sub = d.op == Op::Ccmp;
            Line(std::format("if (recomp::Condition(c, {})) (void)recomp::AddWithCarry<{}>(c, {}, {}, {}, true); else recomp::SetNzcv(c, {});",
                             d.cond, T, Reg(d.rn, d.sf), sub ? std::format("static_cast<{}>(~{})", T, operand) : operand, sub, d.imm));
            break;
        }
        case Op::Udiv: Line(Write(d.rd, d.sf, std::format("recomp::UnsignedDivide<{}>({}, {})", T, Reg(d.rn, d.sf), Reg(d.rm, d.sf)))); break;
        case Op::Sdiv: Line(Write(d.rd, d.sf, std::format("recomp::SignedDivide<{}>({}, {})", T, Reg(d.rn, d.sf), Reg(d.rm, d.sf)))); break;
        case Op::Lslv: Line(Write(d.rd, d.sf, std::format("static_cast<{}>({} << ({} % {}))", T, Reg(d.rn, d.sf), Reg(d.rm, d.sf), width))); break;
        case Op::Lsrv: Line(Write(d.rd, d.sf, std::format("static_cast<{}>({} >> ({} % {}))", T, Reg(d.rn, d.sf), Reg(d.rm, d.sf), width))); break;
        case Op::Asrv:
            Line(Write(d.rd, d.sf, std::format("static_cast<{}>(static_cast<{}>({}) >> ({} % {}))", T, SignedType(d.sf), Reg(d.rn, d.sf),
                                               Reg(d.rm, d.sf), width)));
            break;
        case Op::Rorv: Line(Write(d.rd, d.sf, std::format("recomp::RotateRight<{}>({}, static_cast<unsigned>({}))", T, Reg(d.rn, d.sf), Reg(d.rm, d.sf)))); break;
        case Op::Rbit: Line(Write(d.rd, d.sf, std::format("recomp::ReverseBits<{}>({})", T, Reg(d.rn, d.sf)))); break;
        case Op::Rev16: Line(Write(d.rd, d.sf, std::format("recomp::ReverseBytes<{}>({}, 2)", T, Reg(d.rn, d.sf)))); break;
        case Op::Rev32: Line(Write(d.rd, true, std::format("recomp::ReverseBytes<uint64_t>({}, 4)", Reg(d.rn, true)))); break;
        case Op::Rev: Line(Write(d.rd, d.sf, std::format("recomp::ReverseBytes<{}>({}, {})", T, Reg(d.rn, d.sf), d.sf ? 8 : 4))); break;
        case Op::Clz: Line(Write(d.rd, d.sf, std::format("static_cast<{}>(std::countl_zero({}))", T, Reg(d.rn, d.sf)))); break;
        case Op::Cls: Line(Write(d.rd, d.sf, std::format("recomp::CountLeadingSignBits<{}>({})", T, Reg(d.rn, d.sf)))); break;
        case Op::Madd:
        case Op::Msub:
            Line(Write(d.rd, d.sf, std::format("static_cast<{}>({} {} static_cast<{}>({} * {}))", T, Reg(d.ra, d.sf), d.op == Op::Madd ? "+" : "-", T,
                                               Reg(d.rn, d.sf), Reg(d.rm, d.sf))));
            break;
        case Op::Smaddl: case Op::Smsubl: case Op::Umaddl: case Op::Umsubl: {
            const bool is_signed = d.op == Op::Smaddl || d.op == Op::Smsubl;
            const bool add = d.op == Op::Smaddl || d.op == Op::Umaddl;
            const std::string product = is_signed
                ? std::format("static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>({})) * static_cast<int32_t>({}))", Reg(d.rn, false), Reg(d.rm, false))
                : std::format("static_cast<uint64_t>(uint64_t{{{}}} * {})", Reg(d.rn, false), Reg(d.rm, false));
            Line(Write(d.rd, true, std::format("{} {} {}", Reg(d.ra, true), add ? "+" : "-", product)));
            break;
        }
        case Op::Smulh: Line(Write(d.rd, true, std::format("recomp::MulHighSigned({}, {})", Reg(d.rn, true), Reg(d.rm, true)))); break;
        case Op::Umulh: Line(Write(d.rd, true, std::format("recomp::MulHighUnsigned({}, {})", Reg(d.rn, true), Reg(d.rm, true)))); break;
        case Op::Load: case Op::Store: case Op::LoadPair: case Op::StorePair: Memory(d); break;
        case Op::Prefetch: case Op::Hint: case Op::Clrex: break;
        case Op::Barrier: Line("recomp::Barrier();"); break;
        case Op::B: Line(Jump(d.target)); break;
        case Op::Bl:
            out.callees.insert(d.target);
            Line(std::format("c.x[30] = {}; {}(c);", Guest(d.pc + 4), FunctionName(d.target)));
            break;
        case Op::BCond: Line(std::format("if (recomp::Condition(c, {})) {}", d.cond, Jump(d.target))); break;
        case Op::Cbz:
        case Op::Cbnz: Line(std::format("if ({} {} 0) {}", Reg(d.rd, d.sf), d.op == Op::Cbz ? "==" : "!=", Jump(d.target))); break;
        case Op::Tbz:
        case Op::Tbnz:
            Line(std::format("if (((c.x[{}] >> {}) & 1) {} 0) {}", d.rd == 31 ? 0 : d.rd, d.bit, d.op == Op::Tbz ? "==" : "!=", Jump(d.target)));
            if (d.rd == 31) out.code.replace(out.code.rfind("c.x[0]"), 6, "uint64_t{0}");
            break;
        case Op::Br: Line(std::format("{{ recomp::Dispatch(c, {}); return; }}", Reg(d.rn, true))); break;
        case Op::Blr: Line(std::format("{{ const uint64_t t = {}; c.x[30] = {}; recomp::Dispatch(c, t); }}", Reg(d.rn, true), Guest(d.pc + 4))); break;
        case Op::Ret: Line("return;"); break;
        case Op::Brk: Line(std::format("recomp::Trap(c, {}, {});", Hex(d.pc), d.imm)); break;
        case Op::Mrs:
            if (d.sysreg == a64::kSysTpidrEl0) Line(Write(d.rd, true, "c.tpidr"));
            else if (d.sysreg == a64::kSysTpidrroEl0) Line(Write(d.rd, true, "c.tpidrro"));
            else if (d.sysreg == a64::kSysCntvctEl0) Line(Write(d.rd, true, "recomp::ReadCounter()"));
            else if (d.sysreg == a64::kSysCntfrqEl0) Line(Write(d.rd, true, "uint64_t{19200000}"));
            else if (d.sysreg == a64::kSysFpcr) Line(Write(d.rd, true, "uint64_t{c.fpcr}"));
            else if (d.sysreg == a64::kSysFpsr) Line(Write(d.rd, true, "uint64_t{c.fpsr}"));
            else Unsupported(d);
            break;
        case Op::Msr:
            if (d.sysreg == a64::kSysTpidrEl0) Line(std::format("c.tpidr = {};", Reg(d.rd, true)));
            else if (d.sysreg == a64::kSysFpcr) Line(std::format("c.fpcr = static_cast<uint32_t>({});", Reg(d.rd, true)));
            else if (d.sysreg == a64::kSysFpsr) Line(std::format("c.fpsr = static_cast<uint32_t>({});", Reg(d.rd, true)));
            else Unsupported(d);
            break;
        default: Unsupported(d); break;
        }
    }
};

bool EndsFlow(Op op) { return op == Op::B || op == Op::Br || op == Op::Ret || op == Op::Brk; }

} // namespace

std::string FunctionName(uint64_t rva) { return std::format("f_{:x}", rva); }

bool EmitFunction(const NsoModule& module, const FunctionRange& function, EmitResult& out) {
    out = {};
    const auto image = module.Image();
    if (function.start + function.size > module.Text().size || function.size % 4) return false;
    std::vector<Insn> insns;
    std::set<uint64_t> labels;
    for (uint64_t pc = function.start; pc < function.start + function.size; pc += 4) {
        uint32_t word = 0;
        std::memcpy(&word, image.data() + pc, 4);
        insns.push_back(a64::Decode(word, pc));
        const Insn& d = insns.back();
        const bool branch = d.op == Op::B || d.op == Op::BCond || d.op == Op::Cbz || d.op == Op::Cbnz || d.op == Op::Tbz || d.op == Op::Tbnz;
        if (branch && d.target >= function.start && d.target < function.start + function.size) labels.insert(d.target);
    }
    out.instructions = insns.size();
    Emitter emitter{function, out, labels};
    out.code = std::format("void {}(recomp::Context& c) {{\n", FunctionName(function.start));
    for (const Insn& d : insns) {
        if (labels.contains(d.pc)) out.code += std::format("L_{:x}:\n", d.pc);
        emitter.Emit(d);
    }
    // Sem RET/B no fim: o fluxo cai na funcao seguinte
    if (insns.empty() || !EndsFlow(insns.back().op)) {
        const uint64_t next = function.start + function.size;
        out.callees.insert(next);
        out.code += std::format("    {}(c);\n", FunctionName(next));
    }
    out.code += "}\n";
    return true;
}

std::string EmitFile(const std::vector<EmitResult>& functions) {
    std::string file = "// Gerado por recomp/tools/a64_recomp a partir do dump do usuario — nao versionar\n"
                       "#include \"recomp/context.hpp\"\n\n#include <bit>\n\nnamespace guest {\n\n";
    std::set<uint64_t> declared;
    for (const EmitResult& f : functions) {
        for (uint64_t callee : f.callees) {
            if (declared.insert(callee).second) file += std::format("void {}(recomp::Context& c);\n", FunctionName(callee));
        }
    }
    file += "\n";
    for (const EmitResult& f : functions) file += f.code + "\n";
    return file + "} // namespace guest\n";
}

} // namespace recomp
