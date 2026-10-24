"""Teste diferencial por instrucao: codigo traduzido (insn_difftest.exe) x emulador AArch64 de referencia (Unicorn).

uso: python insn_difftest.py <build-dir> <main> [--cases N] [--seed S] [--show N] [--no-build]
  build-dir: build do CMake de recomp/ (gera build-dir/insn/ com a64_insn_tests e compila insn_difftest).
  Requer: pip install unicorn (opcional: capstone, so para mostrar as instrucoes que divergem). Usar um venv isolado.

Cada teste e uma instrucao real do jogo, traduzida sozinha. Para cada uma sao sorteados N estados iniciais
(registradores X, SP, NZCV, V0-V31, TLS); instrucoes de memoria recebem a base numa area de rascunho mapeada no
mesmo endereco nos dois lados. Compara X0-X30, SP, NZCV, V0-V31, o destino de saida (fall-through, salto, chamada,
retorno) e o SHA-256 da area de rascunho. Sai com 0 somente se todos os casos baterem.
"""

import argparse
import collections
import csv
import hashlib
import os
import random
import shutil
import struct
import subprocess
import sys

import unicorn
from unicorn import arm64_const as R

IMAGE_BASE = 0x7100000000
SCRATCH_BASE = 0x7000000000
SCRATCH_SIZE = 0x11000
MEMORY_BASE = SCRATCH_BASE + 0x400   # base usada nos acessos: offsets sem sinal ate 64 KiB e negativos ate 1 KiB
CLREX_PAGE = 0x6000000000            # limpa o monitor exclusivo do emulador entre casos
RETURNED = 1

CASE = struct.Struct("<Q31QQII2Q512s")
RESULT = struct.Struct("<31QQII512sQ32s")
SPECIAL = [0, 1, 2, 0x7F, 0x80, 0xFF, 0x7FFF, 0x8000, 0xFFFF, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x100000000,
           0x7FFFFFFFFFFFFFFF, 0x8000000000000000, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFE]
X_REGS = [R.UC_ARM64_REG_X0 + i for i in range(29)] + [R.UC_ARM64_REG_X29, R.UC_ARM64_REG_X30]
Q_REGS = [R.UC_ARM64_REG_Q0 + i for i in range(32)]


def scratch_pattern():
    out = bytearray()
    mask = (1 << 64) - 1
    for i in range(SCRATCH_SIZE // 8):
        z = ((i + 1) * 0x9E3779B97F4A7C15) & mask
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & mask
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & mask
        z ^= z >> 31
        out += struct.pack("<Q", z)
    return bytes(out)


def random_value(rng):
    roll = rng.random()
    if roll < 0.25:
        return rng.choice(SPECIAL)
    if roll < 0.4:
        return rng.randrange(0, 4096)
    return rng.getrandbits(64)


def make_case(test, rng):
    x = [random_value(rng) for _ in range(31)]
    sp = rng.getrandbits(64) & ~0xF
    if test["memory"]:
        if test["rn"] == 31:
            sp = MEMORY_BASE
        else:
            x[test["rn"]] = MEMORY_BASE
        if test["mode"] == 3 and test["rm"] != 31:  # AddrMode::Register: indice pequeno para cair na area de rascunho
            x[test["rm"]] = rng.randrange(0, 0x800)
    if test["op"] in ("br", "blr", "ret"):  # destino dentro da imagem: o emulador busca a instrucao seguinte
        target = IMAGE_BASE + (rng.randrange(0, 0x2000000) & ~3)
        if test["rn"] == 31:
            sp = target
        else:
            x[test["rn"]] = target
    vectors = bytes(rng.getrandbits(8) for _ in range(512))
    nzcv = rng.getrandbits(4) << 28
    return dict(rva=test["rva"], x=x, sp=sp, nzcv=nzcv, fpcr=0, tpidr=rng.getrandbits(64), tpidrro=rng.getrandbits(64), v=vectors)


def pack_case(case):
    return CASE.pack(case["rva"], *case["x"], case["sp"], case["nzcv"], case["fpcr"], case["tpidr"], case["tpidrro"], case["v"])


def unpack_result(raw):
    fields = RESULT.unpack(raw)
    return dict(x=list(fields[:31]), sp=fields[31], nzcv=fields[32], fpcr=fields[33], v=fields[34], exit=fields[35], sha=fields[36])


class Reference:
    def __init__(self, image, pristine):
        self.uc = unicorn.Uc(unicorn.UC_ARCH_ARM64, unicorn.UC_MODE_ARM)
        size = (len(image) + 0xFFF) & ~0xFFF
        self.uc.mem_map(IMAGE_BASE, size)
        self.uc.mem_write(IMAGE_BASE, image)
        self.uc.mem_map(SCRATCH_BASE, SCRATCH_SIZE)
        self.uc.mem_map(CLREX_PAGE, 0x1000)
        self.uc.mem_write(CLREX_PAGE, struct.pack("<I", 0xD503305F))
        self.pristine = pristine

    def run(self, case):
        uc = self.uc
        uc.mem_write(SCRATCH_BASE, self.pristine)
        uc.emu_start(CLREX_PAGE, CLREX_PAGE + 4)
        for reg, value in zip(X_REGS, case["x"]):
            uc.reg_write(reg, value)
        uc.reg_write(R.UC_ARM64_REG_SP, case["sp"])
        uc.reg_write(R.UC_ARM64_REG_NZCV, case["nzcv"])
        uc.reg_write(R.UC_ARM64_REG_FPCR, case["fpcr"])
        uc.reg_write(R.UC_ARM64_REG_FPSR, 0)
        uc.reg_write(R.UC_ARM64_REG_TPIDR_EL0, case["tpidr"])
        uc.reg_write(R.UC_ARM64_REG_TPIDRRO_EL0, case["tpidrro"])
        for i, reg in enumerate(Q_REGS):
            uc.reg_write(reg, int.from_bytes(case["v"][i * 16:(i + 1) * 16], "little"))
        pc = IMAGE_BASE + case["rva"]
        uc.emu_start(pc, pc + 4, count=1)
        return dict(x=[uc.reg_read(reg) for reg in X_REGS], sp=uc.reg_read(R.UC_ARM64_REG_SP),
                    nzcv=uc.reg_read(R.UC_ARM64_REG_NZCV) & 0xF0000000, fpcr=uc.reg_read(R.UC_ARM64_REG_FPCR),
                    v=b"".join(uc.reg_read(reg).to_bytes(16, "little") for reg in Q_REGS),
                    exit=uc.reg_read(R.UC_ARM64_REG_PC),
                    sha=hashlib.sha256(bytes(uc.mem_read(SCRATCH_BASE, SCRATCH_SIZE))).digest())


def differences(test, case, host, ref):
    diffs = []
    for i in range(31):
        if host["x"][i] != ref["x"][i]:
            diffs.append(f"x{i}: inicial={case['x'][i]:#x} traduzido={host['x'][i]:#x} referencia={ref['x'][i]:#x}")
    for name in ("sp", "nzcv", "fpcr"):
        if host[name] != ref[name]:
            diffs.append(f"{name}: inicial={case[name]:#x} traduzido={host[name]:#x} referencia={ref[name]:#x}")
    for i in range(32):
        a, b = host["v"][i * 16:(i + 1) * 16], ref["v"][i * 16:(i + 1) * 16]
        if a != b:
            diffs.append(f"v{i}: inicial={case['v'][i * 16:(i + 1) * 16][::-1].hex()} traduzido={a[::-1].hex()} referencia={b[::-1].hex()}")
    exit_host = host["exit"]
    if exit_host == RETURNED and test["op"] == "ret":
        exit_host = case["x"][test["rn"]] if test["rn"] != 31 else 0
    if exit_host != ref["exit"]:
        diffs.append(f"saida: traduzido={host['exit']:#x} referencia={ref['exit']:#x}")
    if host["sha"] != ref["sha"]:
        diffs.append("memoria de rascunho diferente")
    return diffs


def describe(word, rva):
    try:
        import capstone
        md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
        for insn in md.disasm(struct.pack("<I", word), rva):
            return f"{insn.mnemonic} {insn.op_str}"
    except ImportError:
        pass
    return ""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("build_dir")
    parser.add_argument("main")
    parser.add_argument("--cases", type=int, default=8)
    parser.add_argument("--seed", type=int, default=20261024)
    parser.add_argument("--show", type=int, default=30)
    parser.add_argument("--no-build", action="store_true")
    args = parser.parse_args()

    exe = os.path.join(args.build_dir, "insn_difftest.exe" if os.name == "nt" else "insn_difftest")
    if not args.no_build:
        cmake = shutil.which("cmake")
        if not cmake:
            sys.exit("cmake nao encontrado no PATH (use o do WinLibs)")
        subprocess.run([cmake, "--build", args.build_dir, "--target", "insn_difftest"], check=True)
    insn_dir = os.path.join(args.build_dir, "insn")
    tests = []
    with open(os.path.join(insn_dir, "insn_tests.csv"), newline="") as f:
        for row in csv.DictReader(f):
            tests.append(dict(rva=int(row["rva"], 16), word=int(row["word"], 16), op=row["op"], shape=row["shape"], rd=int(row["rd"]),
                              rn=int(row["rn"]), rm=int(row["rm"]), ra=int(row["ra"]), mode=int(row["mode"]), memory=row["memory"] == "1"))

    rng = random.Random(args.seed)
    cases = [(test, make_case(test, rng)) for test in tests for _ in range(args.cases)]
    cases_path = os.path.join(insn_dir, "cases.bin")
    results_path = os.path.join(insn_dir, "results.bin")
    with open(cases_path, "wb") as f:
        for _, case in cases:
            f.write(pack_case(case))
    subprocess.run([exe, args.main, cases_path, results_path], check=True)
    with open(results_path, "rb") as f:
        raw = f.read()
    if len(raw) != RESULT.size * len(cases):
        sys.exit(f"resultados incompletos: {len(raw) // RESULT.size} de {len(cases)}")

    with open(os.path.join(insn_dir, "image.bin"), "rb") as f:
        reference = Reference(f.read(), scratch_pattern())
    failed_cases = 0
    failed_tests = collections.OrderedDict()
    by_op = collections.Counter()
    for index, (test, case) in enumerate(cases):
        host = unpack_result(raw[index * RESULT.size:(index + 1) * RESULT.size])
        try:
            ref = reference.run(case)
        except unicorn.UcError as error:
            diffs = [f"referencia falhou: {error}"]
        else:
            diffs = differences(test, case, host, ref)
        if diffs:
            failed_cases += 1
            by_op[test["op"]] += 1
            failed_tests.setdefault(test["rva"], (test, diffs))

    print(f"{len(tests)} instrucoes, {len(cases)} casos; divergentes: {failed_cases} casos em {len(failed_tests)} instrucoes")
    if by_op:
        print("casos divergentes por operacao:", ", ".join(f"{op}={n}" for op, n in by_op.most_common()))
    for rva, (test, diffs) in list(failed_tests.items())[:args.show]:
        print(f"  {rva:x} {test['word']:08x} {describe(test['word'], rva):32s} [{test['shape']}]")
        for line in diffs[:4]:
            print("      " + line)
    sys.exit(0 if failed_cases == 0 else 1)


if __name__ == "__main__":
    main()
