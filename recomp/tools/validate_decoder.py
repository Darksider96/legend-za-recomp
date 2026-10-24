"""Valida o decodificador A64 do recompilador contra o Capstone (oraculo de teste, nao dependencia do runtime).

uso: python validate_decoder.py <a64_dump.csv> <text.bin>
  a64_dump.csv: saida de recomp/tools/a64_dump; text.bin: .text do main (export do Ghidra ou nso_inspect).
  Requer: pip install capstone (usar um venv isolado).

Para cada instrucao amostrada compara a operacao canonica do decodificador com o mnemonico do Capstone (aliases do
assembler mapeados para a forma canonica), alvos de ramificacao, registradores principais e imediatos. Instrucoes
que o decodificador nao cobre sao classificadas (SIMD/FP, exclusivos/atomicos, sistema...) para orientar a fase R3.
"""

import collections
import csv
import re
import sys

import capstone
from capstone import arm64_const as A

ALIASES = {
    "add": {"add_imm", "add_shift", "add_ext"}, "adds": {"add_imm", "add_shift", "add_ext"},
    "sub": {"sub_imm", "sub_shift", "sub_ext"}, "subs": {"sub_imm", "sub_shift", "sub_ext"},
    "cmp": {"sub_imm", "sub_shift", "sub_ext"}, "cmn": {"add_imm", "add_shift", "add_ext"},
    "neg": {"sub_shift"}, "negs": {"sub_shift"},
    "mov": {"orr", "add_imm", "movz", "movn", "orr_imm"}, "movz": {"movz"}, "movn": {"movn"}, "movk": {"movk"},
    "mvn": {"orn"}, "and": {"and", "and_imm"}, "ands": {"and", "and_imm"}, "tst": {"and", "and_imm"},
    "orr": {"orr", "orr_imm"}, "orn": {"orn"}, "eor": {"eor", "eor_imm"}, "eon": {"eon"}, "bic": {"bic"}, "bics": {"bic"},
    "lsl": {"ubfm", "lslv"}, "lsr": {"ubfm", "lsrv"}, "asr": {"sbfm", "asrv"}, "ror": {"extr", "rorv"},
    "ubfx": {"ubfm"}, "ubfiz": {"ubfm"}, "uxtb": {"ubfm"}, "uxth": {"ubfm"}, "ubfm": {"ubfm"},
    "sbfx": {"sbfm"}, "sbfiz": {"sbfm"}, "sxtb": {"sbfm"}, "sxth": {"sbfm"}, "sxtw": {"sbfm"}, "sbfm": {"sbfm"},
    "bfi": {"bfm"}, "bfxil": {"bfm"}, "bfc": {"bfm"}, "bfm": {"bfm"}, "extr": {"extr"},
    "adr": {"adr"}, "adrp": {"adrp"}, "b": {"b", "b.cond"}, "bl": {"bl"}, "cbz": {"cbz"}, "cbnz": {"cbnz"},
    "tbz": {"tbz"}, "tbnz": {"tbnz"}, "br": {"br"}, "blr": {"blr"}, "ret": {"ret"},
    "prfm": {"prefetch"}, "prfum": {"prefetch"},
    "ldp": {"load_pair"}, "ldpsw": {"load_pair"}, "ldnp": {"load_pair"}, "stp": {"store_pair"}, "stnp": {"store_pair"},
    "adc": {"adc"}, "adcs": {"adc"}, "sbc": {"sbc"}, "sbcs": {"sbc"}, "ngc": {"sbc"}, "ngcs": {"sbc"},
    "csel": {"csel"}, "csinc": {"csinc"}, "cset": {"csinc"}, "cinc": {"csinc"},
    "csinv": {"csinv"}, "csetm": {"csinv"}, "cinv": {"csinv"}, "csneg": {"csneg"}, "cneg": {"csneg"},
    "ccmp": {"ccmp"}, "ccmn": {"ccmn"}, "udiv": {"udiv"}, "sdiv": {"sdiv"},
    "rbit": {"rbit"}, "rev16": {"rev16"}, "rev32": {"rev32"}, "rev": {"rev"}, "rev64": {"rev"}, "clz": {"clz"}, "cls": {"cls"},
    "madd": {"madd"}, "mul": {"madd"}, "msub": {"msub"}, "mneg": {"msub"},
    "smaddl": {"smaddl"}, "smull": {"smaddl"}, "smsubl": {"smsubl"}, "smnegl": {"smsubl"},
    "umaddl": {"umaddl"}, "umull": {"umaddl"}, "umsubl": {"umsubl"}, "umnegl": {"umsubl"}, "smulh": {"smulh"}, "umulh": {"umulh"},
    "nop": {"hint"}, "yield": {"hint"}, "wfe": {"hint"}, "wfi": {"hint"}, "sev": {"hint"}, "sevl": {"hint"}, "hint": {"hint"},
    "csdb": {"hint"}, "esb": {"hint"}, "psb": {"hint"}, "bti": {"hint"}, "paciasp": {"hint"}, "autiasp": {"hint"},
    "dmb": {"barrier"}, "dsb": {"barrier"}, "isb": {"barrier"}, "clrex": {"clrex"}, "mrs": {"mrs"}, "msr": {"msr"},
    "svc": {"svc"}, "brk": {"brk"},
}
LOADS = re.compile(r"^(ldr|ldur)(b|h|sb|sh|sw)?$")
STORES = re.compile(r"^(str|stur)(b|h)?$")
SIMD_REG = re.compile(r"\b([bhsdqv]\d+|v\d+\.)")


def expected_ops(insn):
    m = insn.mnemonic
    if m.startswith("b."):
        return {"b.cond"}
    if LOADS.match(m):
        return {"load"}
    if STORES.match(m):
        return {"store"}
    return ALIASES.get(m)


def category(insn):
    m = insn.mnemonic
    if SIMD_REG.search(insn.op_str) or m.startswith(("f", "scvtf", "ucvtf", "fcvt")):
        return "simd/fp"
    if m.startswith(("ldxr", "stxr", "ldaxr", "stlxr", "ldar", "stlr", "ldxp", "stxp", "ldaxp", "stlxp", "ldapr", "stlur", "ldapur")):
        return "exclusivo/acquire-release"
    if m.startswith(("ldadd", "ldclr", "ldeor", "ldset", "ldsmax", "ldsmin", "ldumax", "ldumin", "swp", "cas", "stadd", "stclr", "stset", "steor")):
        return "atomico LSE"
    if m.startswith("crc32"):
        return "crc32"
    if m in ("dc", "ic", "at", "tlbi", "sys", "sysl"):
        return "sistema (dc/ic/sys)"
    return "outro:" + m


def main():
    dump, text_path = sys.argv[1], sys.argv[2]
    text = open(text_path, "rb").read()
    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
    md.detail = True
    agree = mismatch = 0
    missing = collections.Counter()
    wrong = collections.Counter()
    examples = collections.defaultdict(list)
    target_mismatch = 0
    reg_mismatch = 0
    for row in csv.DictReader(open(dump)):
        rva = int(row["rva"], 16)
        decoded = list(md.disasm(text[rva:rva + 4], rva))
        if not decoded:
            if row["op"] != "unknown":
                wrong["capstone invalido, decodificador aceitou"] += 1
                examples["capstone invalido"].append(row["word"])
            continue
        insn = decoded[0]
        expected = None if SIMD_REG.search(insn.op_str) else expected_ops(insn)
        if row["op"] == "unknown":
            missing[category(insn) if expected is None else "FALTANDO:" + insn.mnemonic] += 1
            if expected is None:
                pass
            elif len(examples["faltando " + insn.mnemonic]) < 3:
                examples["faltando " + insn.mnemonic].append(f"{rva:x} {row['word']} {insn.mnemonic} {insn.op_str}")
            continue
        if expected is None or row["op"] not in expected:
            mismatch += 1
            key = f"{insn.mnemonic} -> {row['op']}"
            wrong[key] += 1
            if len(examples[key]) < 3:
                examples[key].append(f"{rva:x} {row['word']} {insn.mnemonic} {insn.op_str}")
            continue
        agree += 1
        # Alvos de ramificacao e adr/adrp/literais
        if row["op"] in ("b", "bl", "b.cond", "cbz", "cbnz", "tbz", "tbnz", "adr", "adrp") or row["mode"] == "4":
            imms = [op.imm for op in insn.operands if op.type == A.ARM64_OP_IMM]
            if imms and int(row["target"], 16) != imms[-1]:
                target_mismatch += 1
                if len(examples["alvo"]) < 5:
                    examples["alvo"].append(f"{rva:x} {insn.mnemonic} {insn.op_str} decodificado={row['target']}")
        # Registrador de destino/primeiro operando
        regs = [op for op in insn.operands if op.type == A.ARM64_OP_REG]
        if regs and row["op"] not in ("store", "store_pair", "cbz", "cbnz", "tbz", "tbnz", "br", "blr", "ret", "msr", "prefetch", "ccmp", "ccmn"):
            name = insn.reg_name(regs[0].reg)
            number = 31 if name in ("xzr", "wzr", "sp", "wsp") else int(name[1:]) if name[1:].isdigit() else None
            if number is not None and insn.mnemonic not in ("cmp", "cmn", "tst") and number != int(row["rd"]):
                reg_mismatch += 1
                if len(examples["rd"]) < 5:
                    examples["rd"].append(f"{rva:x} {insn.mnemonic} {insn.op_str} rd={row['rd']}")
    total = agree + mismatch
    print(f"decodificadas e comparadas: {total}; mnemonico confere: {agree} ({100.0 * agree / max(total, 1):.3f}%)")
    print(f"alvos divergentes: {target_mismatch}; registrador de destino divergente: {reg_mismatch}")
    print("divergencias:", wrong.most_common(15))
    print("nao decodificadas por categoria:", missing.most_common(25))
    for key, values in list(examples.items())[:25]:
        print(" ", key, values)


if __name__ == "__main__":
    main()
