#!/usr/bin/env python3
"""Recover local AArch64 switch destinations; detailed evidence stays ignored.

Only writes this task's report and CSVs in generated/. Does not run the target,
apply relocations to the image, or modify earlier inventories or the translator.
"""
from __future__ import annotations

import bisect
import collections
import csv
import functools
from pathlib import Path
import struct
import subprocess
import sys

sys.dont_write_bytecode = True
import capstone
from capstone import arm64_const as A

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
OUT = HERE / 'generated'
REPORT = HERE / 'jump_table_targets_report.md'
MASK = (1 << 64) - 1


def hx(value):
    return '' if value is None else f'0x{value:x}'


def number(value):
    return int(value, 16) if value.lower().startswith('0x') else int(value)


def read_csv(name):
    with (OUT / name).open(encoding='utf-8-sig', newline='') as stream:
        return list(csv.DictReader(stream))


def write_csv(name, fields, rows):
    with (OUT / name).open('w', encoding='utf-8', newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(fields)
        writer.writerows(rows)


def extend(value, bits, signed):
    value &= (1 << bits) - 1
    return value - (1 << bits) if signed and value & (1 << (bits - 1)) else value


def wrap(expr, bits):
    if expr is None:
        return None
    if expr[0] == 'constant':
        return ('constant', expr[1] & ((1 << bits) - 1))
    return ('wrap', expr, bits)


def evaluate(expr, raw):
    kind = expr[0]
    if kind == 'constant': return expr[1]
    if kind == 'load':
        return extend(raw, expr[6] * 8, expr[7]) & ((1 << expr[8]) - 1)
    if kind == 'add': return evaluate(expr[1], raw) + evaluate(expr[2], raw)
    if kind == 'shift': return evaluate(expr[1], raw) << expr[2]
    if kind == 'wrap': return evaluate(expr[1], raw) & ((1 << expr[2]) - 1)
    if kind == 'extend': return extend(evaluate(expr[1], raw), expr[2], expr[3])
    raise ValueError('Unsupported expression')


def strip_wrap(expr):
    while expr is not None and expr[0] == 'wrap' and expr[2] == 64:
        expr = expr[1]
    return expr


def loads(expr):
    if expr is None: return []
    if expr[0] == 'load': return [expr]
    if expr[0] == 'add': return loads(expr[1]) + loads(expr[2])
    if expr[0] in ('wrap', 'shift', 'extend'): return loads(expr[1])
    return []


def expression_text(expr):
    """Reproducible arithmetic, not a disassembly or executable code listing."""
    kind = expr[0]
    if kind == 'constant': return hx(expr[1])
    if kind == 'load': return f'{"s" if expr[7] else "u"}{expr[6] * 8}(T[i])'
    if kind == 'add': return f'({expression_text(expr[1])}+{expression_text(expr[2])})'
    if kind == 'shift': return f'({expression_text(expr[1])}<<{expr[2]})'
    if kind == 'wrap': return f'u{expr[2]}({expression_text(expr[1])})'
    if kind == 'extend': return f'{"s" if expr[3] else "u"}{expr[2]}({expression_text(expr[1])})'
    raise ValueError('Unsupported expression rendering')


class Functions:
    def __init__(self):
        self.rows = sorted((number(r['rva']), number(r['tamanho']))
                           for name in ('functions.csv', 'gap_functions.csv')
                           for r in read_csv(name))
        self.starts = [s for s, n in self.rows]
        end = 0
        for start, size in self.rows:
            if start % 4 or size <= 0 or size % 4 or start < end:
                raise ValueError(f'Invalid function interval {hx(start)}')
            end = start + size

    def containing(self, pc):
        i = bisect.bisect_right(self.starts, pc) - 1
        if i >= 0 and pc < sum(self.rows[i]): return self.rows[i]
        return None


class Slice:
    """Bounded backward register slice, fenced at known control-flow joins.

    Conditional exits may be crossed on their fallthrough edge. Unconditional
    transfers, calls, invalid instructions and incoming direct branch targets
    fence the slice. A second pass also fences recovered indirect destinations.
    No constants are guessed across a join or an unknown register definition.
    """
    def __init__(self, image, function, extra_labels=()):
        self.start, self.size = function
        self.md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
        self.md.detail = True
        self.md.skipdata = True
        self.ins = {i.address: i for i in self.md.disasm(
            image[self.start:self.start + self.size], self.start)}
        self.labels = set(extra_labels)
        for ins in self.ins.values():
            if ins.id and capstone.CS_GRP_JUMP in ins.groups:
                self.labels.update(o.imm for o in ins.operands
                                   if o.type == A.ARM64_OP_IMM
                                   and self.start <= o.imm < self.start + self.size)
        self.writes = {}
        self.failures = collections.Counter()

    def reg(self, register):
        name = self.md.reg_name(register)
        if name in ('fp', 'lr'): return 29 if name == 'fp' else 30
        if name in ('xzr', 'wzr'): return 32
        if name and name[0] in 'xw' and name[1:].isdigit(): return int(name[1:])
        return None

    def width(self, register):
        return 32 if self.md.reg_name(register).startswith('w') else 64

    def barrier(self, ins):
        return (not ins.id or ins.mnemonic in ('b', 'br', 'ret', 'eret', 'brk', 'hlt', 'udf')
                or capstone.CS_GRP_CALL in ins.groups
                or capstone.CS_GRP_RET in ins.groups
                or capstone.CS_GRP_INT in ins.groups)

    def modified(self, ins):
        if ins.address not in self.writes:
            self.writes[ins.address] = {self.reg(r) for r in ins.regs_access()[1]} - {None}
        return self.writes[ins.address]

    def transform(self, expr, operand):
        if expr is None: return None
        ext = operand.ext
        extensions = {A.ARM64_EXT_UXTB: (8, False), A.ARM64_EXT_UXTH: (16, False),
                      A.ARM64_EXT_UXTW: (32, False), A.ARM64_EXT_UXTX: (64, False),
                      A.ARM64_EXT_SXTB: (8, True), A.ARM64_EXT_SXTH: (16, True),
                      A.ARM64_EXT_SXTW: (32, True), A.ARM64_EXT_SXTX: (64, True)}
        if ext:
            if ext not in extensions: return None
            bits, signed = extensions[ext]
            expr = ('extend', expr, bits, signed)
        if operand.shift.type not in (0, A.ARM64_SFT_LSL): return None
        if operand.shift.value: expr = ('shift', expr, operand.shift.value)
        return expr

    @functools.lru_cache(maxsize=None)
    def value(self, register, before):
        key = self.reg(register)
        if key == 32: return ('constant', 0)
        if key is None: return None
        lower = max(self.start, before - 256 * 4)
        # State immediately before a known join has no unique linear predecessor.
        if before in self.labels:
            self.failures['join'] += 1
            return None
        for pc in range(before - 4, lower - 1, -4):
            ins = self.ins.get(pc)
            if ins is None or self.barrier(ins):
                self.failures['barreira'] += 1
                return None
            if key in self.modified(ins):
                expr = self.definition(ins)
                return wrap(expr, self.width(register))
            if pc in self.labels:
                self.failures['join'] += 1
                return None
        self.failures['sem_definicao_local'] += 1
        return None

    def constant(self, register, before):
        expr = self.value(register, before)
        if expr is not None and not loads(expr):
            return evaluate(expr, 0) & MASK
        return None

    def definition(self, ins):
        ops = ins.operands
        if len(ops) < 2 or ops[0].type != A.ARM64_OP_REG: return None
        dest_bits = self.width(ops[0].reg)
        expr = None
        if ins.mnemonic in ('adr', 'adrp') and ops[1].type == A.ARM64_OP_IMM:
            expr = ('constant', ops[1].imm)
        elif ins.mnemonic in ('mov', 'movz', 'movn') and len(ops) == 2:
            if ops[1].type == A.ARM64_OP_IMM:
                v = ops[1].imm << ops[1].shift.value
                expr = ('constant', ~v if ins.mnemonic == 'movn' else v)
            elif ops[1].type == A.ARM64_OP_REG:
                expr = self.value(ops[1].reg, ins.address)
        elif ins.mnemonic in ('add', 'sub') and len(ops) == 3:
            if ops[1].type != A.ARM64_OP_REG: return None
            left = self.value(ops[1].reg, ins.address)
            right = (('constant', ops[2].imm) if ops[2].type == A.ARM64_OP_IMM else
                     self.value(ops[2].reg, ins.address) if ops[2].type == A.ARM64_OP_REG else None)
            right = self.transform(right, ops[2])
            if left is not None and right is not None:
                if not loads(left) and not loads(right):
                    a, b = evaluate(left, 0), evaluate(right, 0)
                    expr = ('constant', a + b if ins.mnemonic == 'add' else a - b)
                elif ins.mnemonic == 'add':
                    expr = ('add', left, right)
        elif ins.mnemonic == 'lsl' and len(ops) == 3 and ops[2].type == A.ARM64_OP_IMM:
            source = self.value(ops[1].reg, ins.address)
            if source is not None: expr = ('shift', source, ops[2].imm)
        elif ins.mnemonic in ('sxtb', 'sxth', 'sxtw', 'uxtb', 'uxth', 'uxtw'):
            source = self.value(ops[1].reg, ins.address)
            if source is not None:
                bits = {'b': 8, 'h': 16, 'w': 32}[ins.mnemonic[-1]]
                expr = ('extend', source, bits, ins.mnemonic[0] == 's')
        elif ins.mnemonic in ('ldrb', 'ldrh', 'ldrsb', 'ldrsh', 'ldrsw', 'ldr'):
            if len(ops) != 2 or ops[1].type != A.ARM64_OP_MEM or ins.writeback: return None
            mem = ops[1].mem
            if not mem.base or not mem.index or self.reg(mem.index) is None: return None
            sizes = {'ldrb': 1, 'ldrsb': 1, 'ldrh': 2, 'ldrsh': 2, 'ldrsw': 4}
            size = sizes.get(ins.mnemonic, dest_bits // 8)
            if ops[1].shift.type not in (0, A.ARM64_SFT_LSL): return None
            if (1 << ops[1].shift.value) != size: return None
            if ops[1].ext not in (0, A.ARM64_EXT_UXTW, A.ARM64_EXT_UXTX): return None
            base = self.constant(mem.base, ins.address)
            if base is not None: base = (base + mem.disp) & MASK
            expr = ('load', ins.address, base, self.reg(mem.index),
                    ops[1].shift.value, ins.mnemonic, size,
                    ins.mnemonic in ('ldrsb', 'ldrsh', 'ldrsw'), dest_bits)
        return wrap(expr, dest_bits)

    def formula(self, pc):
        ins = self.ins.get(pc)
        if ins is None or ins.mnemonic != 'br': return None, 'BR nao confirmado'
        expr = strip_wrap(self.value(ins.operands[0].reg, pc))
        if expr is None: return None, 'definicao nao provada no trecho local (join/barreira/limite)'
        found = loads(expr)
        if len(found) != 1: return None, 'destino nao deriva de uma unica carga indexada'
        load = found[0]
        base = None
        if load[6] == 8:
            if strip_wrap(expr) != load: return None, 'ponteiro de 8 bytes com transformacao nao suportada'
            kind, base = 'ponteiros_8', None
        else:
            if expr[0] != 'add': return None, 'offset sem soma com base constante'
            left, right = expr[1], expr[2]
            if not loads(left): base, offset = evaluate(left, 0) & MASK, right
            elif not loads(right): base, offset = evaluate(right, 0) & MASK, left
            else: return None, 'base de destino nao constante'
            # Supported offset expressions contain only load, width/sign and LSL.
            node = offset
            while node[0] in ('wrap', 'extend', 'shift'): node = node[1]
            if node != load: return None, 'transformacao de offset nao suportada'
            sign = 's' if load[7] else 'u'
            kind = f'offset_{sign}{load[6] * 8}'
        return dict(expr=expr, load=load, table=load[2], base=base, kind=kind), ''


def calculate(formula, count, image, relocations, function, md):
    table, load = formula['table'], formula['load']
    if table is None: return [], [], [], 'base da tabela nao provada localmente'
    size = load[6]
    if table < 0 or table + count * size > len(image):
        return [], [], [], 'intervalo da tabela fora da imagem'
    good, bad, slots = [], [], []
    start, length = function
    for i in range(count):
        slot = table + i * size
        if size == 8:
            slots.append((i, slot, slot in relocations))
            if slot not in relocations:
                bad.append((i, None, 'slot_sem_relocacao_relativa'))
                continue
            raw = relocations[slot]
        else:
            raw = int.from_bytes(image[slot:slot + size], 'little')
        target = evaluate(formula['expr'], raw) & MASK
        if target % 4 or not start <= target < start + length:
            bad.append((i, target, 'fora_da_funcao_ou_desalinhado'))
            continue
        decoded = next(md.disasm(image[target:target + 4], target, count=1), None)
        if decoded is None or decoded.size != 4 or not decoded.id or decoded.mnemonic == 'udf':
            bad.append((i, target, 'instrucao_invalida'))
            continue
        good.append((i, target))
    return good, bad, slots, ''


def size_bin(n):
    if n is None: return 'sem_numero'
    if n == 1: return '1'
    if n <= 3: return '2-3'
    if n <= 7: return '4-7'
    if n <= 15: return '8-15'
    if n <= 63: return '16-63'
    return '64+'


def self_check():
    # Synthetic inputs exercise signed offsets and relocation-backed pointers.
    assert extend(0xff, 8, False) == 255 and extend(0xff, 8, True) == -1
    assert extend(0xffff, 16, True) == -1
    assert extend(0xfffffffe, 32, True) == -2
    # A signed byte loaded into W is zero-extended on a later X read; an
    # explicit SXTW on the ADD operand restores its negative interpretation.
    signed_w = ('load', 0, 16, 0, 0, 'ldrsb', 1, True, 32)
    assert evaluate(signed_w, 0xff) == 0xffffffff
    assert evaluate(('extend', signed_w, 32, True), 0xff) == -1
    load = ('load', 0, 16, 0, 0, 'ldrsw', 4, True, 64)
    expr = ('wrap', ('add', ('constant', 32), ('shift', load, 2)), 64)
    assert evaluate(expr, 0xfffffffe) == 24
    # Pointer raw bytes are deliberately zero; only the relocation provides 24.
    image = bytearray(64)
    struct.pack_into('<I', image, 24, 0xd503201f)  # Generic ISA NOP, no target bytes.
    pointer = ('load', 0, 16, 0, 3, 'ldr', 8, False, 64)
    formula = dict(table=16, load=pointer, expr=pointer)
    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
    assert calculate(formula, 1, image, {16: 24}, (0, 64), md)[0] == [(0, 24)]
    assert calculate(formula, 1, image, {}, (0, 64), md)[1][0][2] == 'slot_sem_relocacao_relativa'


def run():
    self_check()
    image = (OUT / 'image.bin').read_bytes()
    layout = {r['nome']: (number(r['rva']), number(r['tamanho'])) for r in read_csv('layout.csv')}
    if len(image) != max(s + n for s, n in layout.values()):
        raise ValueError('Image size disagrees with layout')
    functions = Functions()
    candidates = read_csv('jump_tables.csv')
    if len(candidates) != 4454 or len({r['br_rva'] for r in candidates}) != len(candidates):
        raise ValueError('Candidate count/uniqueness disagrees with census')
    relatives = {}
    for row in read_csv('relocations.csv'):
        if row['tipo'] in ('relr', 'relative'):
            slot, target = number(row['rva']), number(row['alvo_rva'])
            if slot in relatives: raise ValueError('Duplicate relative relocation')
            relatives[slot] = target
    indirect = read_csv('indirect_branches.csv')
    by_pc = {number(r['origem_rva']): r for r in indirect}
    br_counts = collections.Counter()
    for row in indirect:
        if row['mnemonico'] == 'br':
            owner = functions.containing(number(row['origem_rva']))
            if owner: br_counts[owner[0]] += 1
    grouped = collections.defaultdict(list)
    for row in candidates:
        pc = number(row['br_rva'])
        owner = functions.containing(pc)
        if owner is None: raise ValueError(f'BR without function {hx(pc)}')
        grouped[owner].append(row)
    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
    summaries, target_rows, diagnostics, formula_rows, slot_rows = [], [], [], [], []
    final_bases, iterations_hist = set(), collections.Counter()
    output = []
    def emit(line):
        output.append(line)
        print(line, flush=True)
    emit(f'Capstone {capstone.__version__}; candidatas={len(candidates)}; funcoes={len(grouped)}')
    for ordinal, (function, rows) in enumerate(sorted(grouped.items()), 1):
        labels = set()
        # A monotonic join fence prevents a recovered case entry from being
        # silently crossed as though it had a unique linear predecessor.
        for iteration in range(1, len(rows) + 3):
            sl = Slice(image, function, labels)
            results, discovered = [], set()
            for row in rows:
                pc = number(row['br_rva'])
                count = number(row['entradas']) if row['entradas'] else None
                formula, reason = sl.formula(pc)
                good, bad, slots = [], [], []
                if count is None:
                    reason = 'numero de entradas ausente no censo; nao inferido'
                elif count <= 0:
                    reason = 'numero de entradas nao positivo'
                elif formula is not None:
                    load = formula['load']
                    if load[6] != number(row['tamanho_entrada']):
                        reason = 'tamanho reconstruido difere do censo'
                    elif row['base_rva'] and formula['table'] is not None and number(row['base_rva']) != formula['table']:
                        reason = 'base reconstruida difere do censo'
                    elif (by_pc[pc]['base_destinos_rva'] and formula['base'] is not None
                          and number(by_pc[pc]['base_destinos_rva']) != formula['base']):
                        reason = 'base de destinos reconstruida difere do censo'
                    else:
                        good, bad, slots, reason = calculate(formula, count, image, relatives, function, md)
                        discovered.update(t for i, t in good)
                results.append((pc, count, formula, reason, good, bad, slots))
            extra = discovered - labels
            sl.value.cache_clear()
            if not extra: break
            labels.update(extra)
        else:
            raise ValueError('Join-fence iteration did not converge')
        iterations_hist[iteration] += 1
        for pc, count, formula, reason, good, bad, slots in results:
            status = ('resolvido' if count is not None and len(good) == count and not bad and not reason else
                      'parcial' if good else 'nao_resolvido')
            if bad:
                reason = '; '.join(f'{k}={n}' for k, n in sorted(collections.Counter(b[2] for b in bad).items()))
            kind = formula['kind'] if formula else 'nao_identificada'
            summaries.append((pc, function[0], kind, count, len({t for i, t in good}), status, reason))
            target_rows.extend((pc, function[0], i, t) for i, t in good)
            diagnostics.extend((pc, function[0], i, t, why) for i, t, why in bad)
            slot_rows.extend((pc, i, slot, int(present)) for i, slot, present in slots)
            if formula is not None:
                formula_rows.append((pc, function[0], formula['table'], formula['base'], formula['load'][1],
                                     formula['load'][6], formula['load'][5], status,
                                     expression_text(formula['expr'])))
                if status == 'resolvido' and formula['base'] is not None: final_bases.add(formula['base'])
        if ordinal % 1000 == 0: emit(f'Funcoes analisadas: {ordinal}/{len(grouped)}')

    summaries.sort(); target_rows.sort(); formula_rows.sort(); diagnostics.sort(); slot_rows.sort()
    write_csv('jump_table_targets.csv', ('br_rva', 'funcao_rva', 'indice', 'alvo_rva'),
              ((hx(pc), hx(fn), i, hx(t)) for pc, fn, i, t in target_rows))
    write_csv('jump_table_summary.csv', ('br_rva', 'funcao_rva', 'forma', 'entradas', 'destinos_distintos', 'status', 'motivo'),
              ((hx(pc), hx(fn), kind, '' if n is None else n, d, status, why)
               for pc, fn, kind, n, d, status, why in summaries))
    write_csv('jump_table_formulas.csv', ('br_rva', 'funcao_rva', 'tabela_rva', 'base_destinos_rva', 'carga_rva', 'tamanho_entrada', 'carga', 'status', 'formula'),
              ((hx(pc), hx(fn), hx(t), hx(b), hx(l), size, op, status, expr)
               for pc, fn, t, b, l, size, op, status, expr in formula_rows))
    write_csv('jump_table_rejected_targets.csv', ('br_rva', 'funcao_rva', 'indice', 'alvo_rva', 'motivo'),
              ((hx(pc), hx(fn), i, hx(t), why) for pc, fn, i, t, why in diagnostics))
    write_csv('jump_table_pointer_slots.csv', ('br_rva', 'indice', 'slot_rva', 'relativa'),
              ((hx(pc), i, hx(slot), present) for pc, i, slot, present in slot_rows))

    middle = [r for r in read_csv('code_text_addresses.csv') if r['classe'] == 'meio_funcao']
    covered = [r for r in middle if number(r['alvo_rva']) in final_bases]
    write_csv('jump_table_base_coverage.csv', ('origem_rva', 'alvo_rva', 'base_resolvida'),
              ((r['origem_rva'], r['alvo_rva'], int(number(r['alvo_rva']) in final_bases)) for r in middle))
    statuses = collections.Counter(r[5] for r in summaries)
    forms = collections.Counter((r[2], r[5]) for r in summaries)
    hist = collections.Counter((size_bin(r[3]), r[5]) for r in summaries)
    outside = [r for r in diagnostics if r[4] == 'fora_da_funcao_ou_desalinhado']
    invalid = [r for r in diagnostics if r[4] == 'instrucao_invalida']
    check1 = all(t % 4 == 0 and functions.containing(t) == functions.containing(pc) for pc, fn, i, t in target_rows)
    check2 = all((ins := next(md.disasm(image[t:t + 4], t, count=1), None)) is not None
                 and ins.id and ins.size == 4 and ins.mnemonic != 'udf' for pc, fn, i, t in target_rows)
    check3 = all(present for pc, i, slot, present in slot_rows)
    check4 = (sum(statuses.values()) == len(candidates) == 4454 and len(middle) == 4474
              and len({r[0] for r in summaries}) == 4454
              and sum(forms.values()) == sum(hist.values()) == 4454)
    checks = [check1, check2, check3, check4]
    lines = [f'Candidatas: {len(candidates)}; resolvido={statuses["resolvido"]}; parcial={statuses["parcial"]}; nao_resolvido={statuses["nao_resolvido"]}',
             f'Entradas exportadas: {len(target_rows)}; destinos distintos: {len({r[3] for r in target_rows})}',
             f'Bases B resolvidas distintas: {len(final_bases)}; enderecos meio_funcao cobertos: {len(covered)}/{len(middle)}',
             f'Alvos calculados rejeitados: fora/desalinhado={len(outside)}; instrucao invalida={len(invalid)}',
             f'Slots de ponteiros: {len(slot_rows)}; sem relativa: {sum(not r[3] for r in slot_rows)}']
    lines.extend(f'AUTOVERIFICACAO {i}: {"OK" if ok else "FALHOU"}' for i, ok in enumerate(checks, 1))
    for line in lines: emit(line)
    safety = subprocess.run([sys.executable, '-I', str(ROOT / 'tools' / 'check_repo_safety.py')],
                            cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    report = ['# Destinos das tabelas de salto para R4', '',
              'Análise estática da imagem sem relocação; nenhum executável do jogo foi iniciado. '
              'RVAs são offsets na imagem. O relatório contém metadados; os CSVs detalhados permanecem em `generated/` ignorado pelo Git.', '',
              '## Resultado', '', *[f'- {line}' for line in lines[:5]], '',
              'Somente tabelas `resolvido` têm uma lista completa validada estruturalmente para gerar o `switch` local. '
              'Linhas `parcial` exportam apenas entradas validadas e exigem tratamento dos índices restantes; '
              '`nao_resolvido` exige fallback ou análise adicional.', '',
              '## Método e limites', '',
              '- Entradas: `image.bin`, `layout.csv`, `functions.csv`, `gap_functions.csv`, `jump_tables.csv`, '
              '`relocations.csv`, `indirect_branches.csv` e `code_text_addresses.csv`. Os inventários anteriores não foram alterados.',
              '- A fatia reversa segue definições de registradores, aliases W/X, MOV, ADR/ADRP, ADD/SUB constantes, '
              'extensões e LSL; tem limite de 256 instruções por busca, dentro da mesma função. '
              'Não atravessa chamadas, transferências incondicionais, instruções inválidas ou joins conhecidos.',
              '- Saídas condicionais podem ser atravessadas pelo caminho de fallthrough. Alvos diretos internos e destinos '
              'indiretos recuperados viram cercas de join; a recuperação repete até essas cercas estabilizarem. '
              'Não há reconstrução completa do CFG nem prova global de todos os predecessores indiretos desconhecidos. '
              'Constantes não locais ficam explicitamente sem resolução.',
              '- Para offsets, a fórmula é `B + offset(T[i])` módulo 64 bits, preservando largura do registrador, '
              'sinal/zero-extensão e deslocamentos. B e T são recuperados separadamente. '
              'A escala de indexação da carga deve corresponder ao tamanho da entrada. Bases conhecidas e tamanho são confrontados com o censo.',
              '- O número de entradas vem exclusivamente de `entradas` no censo. Nenhum tamanho foi inferido por conteúdo ou por plausibilidade dos destinos.',
              '- Para ponteiros de 8 bytes, o valor vem exclusivamente de `alvo_rva` da relocação RELR/RELATIVE do slot. '
              'Ausência de relocação rejeita a entrada. A imagem nunca é relocada nem escrita.',
              '- Cada entrada exportada pertence à mesma função do BR, alinha a 4 e decodifica em uma instrução Capstone válida. '
              'Isso é uma condição estrutural, não prova isolada de alcançabilidade ou de que bytes jamais sejam dados. '
              'UDF é rejeitado adicionalmente.',
              f'- Versão observada do Capstone: `{capstone.__version__}`. Testes sintéticos cobrem extensão com sinal, '
              'offset negativo e uso obrigatório de relocação para ponteiros.',
              f'- Iterações de cercas por função: `{dict(sorted(iterations_hist.items()))}`.', '',
              '## Contagens por forma e status', '', '| Forma | Resolvido | Parcial | Não resolvido |', '|---|---:|---:|---:|']
    for kind in sorted({r[2] for r in summaries}):
        report.append(f'| {kind} | {forms[kind,"resolvido"]} | {forms[kind,"parcial"]} | {forms[kind,"nao_resolvido"]} |')
    report += ['', '## Histograma de entradas', '', '| Entradas | Resolvido | Parcial | Não resolvido |', '|---|---:|---:|---:|']
    for label in ('1', '2-3', '4-7', '8-15', '16-63', '64+', 'sem_numero'):
        report.append(f'| {label} | {hist[label,"resolvido"]} | {hist[label,"parcial"]} | {hist[label,"nao_resolvido"]} |')
    report += ['', '## Cobertura das bases B', '',
               f'Das {len(middle)} materializações classificadas como `meio_funcao`, {len(covered)} apontam para B de '
               f'alguma tabela resolvida. São {len({r["alvo_rva"] for r in covered})} RVAs de base distintos cobertos. '
               'A contagem principal é por linha/origem, para preservar o denominador 4.474; não por tabela nem por RVA distinto. '
               'B é a base de destinos em `.text`, não a base T dos dados da tabela.', '',
               '## 20 BR não resolvidos com mais BR na função', '',
               'Ordenação: quantidade total de BR na função, depois RVA crescente. Contagem de BR usa `indirect_branches.csv`.', '',
               '| BR RVA | Função RVA | BR na função | Motivo |', '|---|---|---:|---|']
    unresolved = sorted((r for r in summaries if r[5] == 'nao_resolvido'), key=lambda r: (-br_counts[r[1]], r[0]))[:20]
    for pc, fn, kind, count, distinct, status, why in unresolved:
        report.append(f'| {hx(pc)} | {hx(fn)} | {br_counts[fn]} | {why} |')
    report += ['', 'Motivos agregados para as candidatas não resolvidas:', '']
    report.extend(f'- {why}: {n}.' for why, n in sorted(collections.Counter(
        r[6] for r in summaries if r[5] == 'nao_resolvido').items()))
    gap_starts = {number(r['rva']) for r in read_csv('gap_functions.csv')}
    gap_rejections = sum(fn in gap_starts for pc, fn, i, t, why in outside)
    report += ['', f'Dos {len(outside)} cálculos rejeitados fora/desalinhados, {gap_rejections} têm o BR em '
               'função proposta por `gap_functions.csv`. Esses destinos cruzam os limites fornecidos; '
               'isso exige revisar os limites ou confirmar uma transferência não local antes de gerar gotos. '
               'Não demonstra, por si só, que a fórmula esteja errada. Os arquivos anteriores permanecem intactos.']
    report += ['', '## Rejeições e autoverificações', '',
               f'1. Exportados dentro da mesma função e alinhados: {"OK" if check1 else "FALHOU"}. '
               f'Cálculos rejeitados fora/desalinhados: {len(outside)}.',
               f'2. Exportados decodificam: {"OK" if check2 else "FALHOU"}. Cálculos rejeitados por instrução inválida: {len(invalid)}.',
               f'3. Slots de ponteiro com relocação relativa: {"OK" if check3 else "FALHOU"}; '
               f'{len(slot_rows)} slots. Se zero, a verificação no binário é vacuamente verdadeira; o caminho é exercitado sinteticamente.',
               f'4. Cobertura e somas: {"OK" if check4 else "FALHOU"}; {sum(statuses.values())}/4454 candidatas, '
               f'{len(covered)}/4474 materializações cobertas.', '',
               'RVAs dos cálculos fora/desalinhados (BR, função, destino; sem bytes ou listagem):', '']
    report.extend(f'- {hx(pc)}, {hx(fn)}, {hx(t)}' for pc, fn, i, t, why in outside)
    if not outside: report.append('Nenhum.')
    report += ['', '## Saídas', '',
               '- `generated/jump_table_targets.csv`: quatro colunas pedidas; uma linha por entrada validada.',
               '- `generated/jump_table_summary.csv`: sete colunas pedidas; uma linha por candidata.',
               '- `generated/jump_table_formulas.csv`: T, B, carga e expressão aritmética completa, sem listagem de instruções.',
               '- `generated/jump_table_rejected_targets.csv`: índice e motivo de cada rejeição calculada.',
               '- `generated/jump_table_pointer_slots.csv`: auditoria de relocação para os slots de 8 bytes.',
               '- `generated/jump_table_base_coverage.csv`: cobertura por materialização original.', '',
               '## Comandos de aceite', '',
               '```text', r'build\venv\Scripts\python.exe -I analysis\recomp\jump_table_targets.py',
               *output,
               'Saidas: analysis/recomp/generated/*.csv e analysis/recomp/jump_table_targets_report.md',
               f'Exit code: {0 if all(checks) and safety.returncode == 0 else 1}', '```', '',
               '```text', r'python tools\check_repo_safety.py', safety.stdout.strip(), safety.stderr.strip(),
               f'Exit code: {safety.returncode}', '```', '',
               'A verificação de segurança acima é executada pelo script com o mesmo interpretador, em modo isolado. '
               'O comando independente também deve ser executado no aceite. Sem commit ou push.', '']
    REPORT.write_text('\n'.join(report), encoding='utf-8', newline='\n')
    print('Saidas: analysis/recomp/generated/*.csv e analysis/recomp/jump_table_targets_report.md', flush=True)
    return 0 if all(checks) and safety.returncode == 0 else 1


if __name__ == '__main__':
    raise SystemExit(run())
