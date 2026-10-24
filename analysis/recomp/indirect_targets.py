#!/usr/bin/env python3
"""Inventory static .text address candidates for the R4 dispatch table.

Writes only this task's report and detailed CSVs in generated/. Does not execute
the target or modify any input inventory, translator, runtime, or prior script.
"""
from __future__ import annotations

import bisect
import collections
import csv
import importlib.util
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
REPORT = HERE / 'indirect_targets_report.md'
CLASSES = ('inicio_funcao', 'stub_plt', 'meio_funcao', 'fora')
RELATIVE = {'relr', 'relative'}
EXPECTED_TYPES = {'relr': 346629, 'relative': 0, 'abs64': 3303, 'jump_slot': 917, 'glob_dat': 55}
EXPECTED_RELOCATIONS = 350904
EXPECTED_INIT = 1912


def hx(value):
    return '' if value is None else f'0x{value:x}'


def read_csv(name):
    with (OUT/name).open(encoding='utf-8-sig', newline='') as stream:
        return list(csv.DictReader(stream))


def write_csv(name, columns, rows):
    with (OUT/name).open('w',encoding='utf-8',newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(columns)
        writer.writerows(rows)


def load_census():
    spec = importlib.util.spec_from_file_location('indirect_census_helpers',HERE/'control_flow_census.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class FunctionIndex:
    def __init__(self, functions, plt):
        self.functions = sorted(functions)
        self.starts = [s for s,n,origin in self.functions]
        self.entries = set(self.starts)
        self.plt = set(plt)
        previous = 0
        for start,size,origin in self.functions:
            if start % 4 or size <= 0 or size % 4 or start < previous:
                raise ValueError(f'Invalid or overlapping function interval at {hx(start)}')
            previous = start+size

    def containing(self, target):
        i = bisect.bisect_right(self.starts,target)-1
        if i >= 0:
            start,size,origin = self.functions[i]
            if target < start+size:
                return start,size,origin
        return None

    def classify(self, target):
        if target in self.entries:
            return 'inicio_funcao'
        if target in self.plt:
            return 'stub_plt'
        return 'meio_funcao' if self.containing(target) is not None else 'fora'


def maximal_runs(slots):
    """Input (slot RVA, segment) pairs; output maximal runs including singletons."""
    runs = []
    for slot,segment in sorted(set(slots),key=lambda x:(x[1],x[0])):
        if runs and runs[-1][2] == segment and runs[-1][0]+runs[-1][1]*8 == slot:
            start,count,seg = runs[-1]
            runs[-1] = start,count+1,seg
        else:
            runs.append((slot,1,segment))
    return runs


def size_bin(count):
    if count == 1: return '1'
    if count <= 3: return '2-3'
    if count <= 7: return '4-7'
    if count <= 15: return '8-15'
    if count <= 63: return '16-63'
    return '64+'


class CodeAddressFinder:
    def __init__(self,text,text_start):
        self.text = text
        self.text_start = text_start
        self.text_end = text_start+len(text)
        self.md = capstone.Cs(capstone.CS_ARCH_ARM64,capstone.CS_MODE_ARM)
        self.md.detail = True
        self.md.skipdata = True

    def word(self, rva):
        return struct.unpack_from('<I',self.text,rva-self.text_start)[0]

    def decode(self,start,end):
        return self.md.disasm(self.text[start-self.text_start:end-self.text_start],start)

    def in_text(self,target):
        return self.text_start <= target < self.text_end

    def reg_key(self,reg):
        if not reg:
            return None
        name = self.md.reg_name(reg)
        if name in ('fp','lr'):
            return 29 if name == 'fp' else 30
        if name.startswith(('x','w')) and name[1:].isdigit():
            return int(name[1:])
        return None

    def flow(self,ins):
        return (not ins.id or ins.mnemonic in ('udf','brk','hlt') or
                any(group in ins.groups for group in (capstone.CS_GRP_JUMP,
                    capstone.CS_GRP_CALL,capstone.CS_GRP_RET,capstone.CS_GRP_INT)))

    def access(self,ins):
        reads,writes = ins.regs_access()
        return ({self.reg_key(r) for r in reads}-{None},
                {self.reg_key(r) for r in writes}-{None})

    def only_load_base(self,ins,reg):
        if not ins.mnemonic.startswith('ld'):
            return False
        found = False
        for op in ins.operands:
            if op.type == A.ARM64_OP_MEM:
                if self.reg_key(op.mem.index) == reg:
                    return False
                found |= self.reg_key(op.mem.base) == reg
            elif (op.type == A.ARM64_OP_REG and self.reg_key(op.reg) == reg
                    and op.access & capstone.CS_AC_READ):
                return False
        return found

    def load_base_exclusion(self,source,reg,end):
        """Exclude only when a near load-only use is proven up to clobber.

        First use must be among the next four instructions. Within twelve
        instructions all reads must be load-base uses and a write must end the
        original value's lifetime. A live value at a control boundary is kept.
        """
        seen_load = False
        first_use = None
        for step,ins in enumerate(self.decode(source+4,min(end,source+4+12*4)),1):
            if self.flow(ins):
                return False
            reads,writes = self.access(ins)
            if reg in reads:
                if first_use is None:
                    first_use = step
                    if step > 4:
                        return False
                if not self.only_load_base(ins,reg):
                    return False
                seen_load = True
            if reg in writes:
                return seen_load
        return False

    def scan(self,functions,index):
        accepted,excluded = {},{}
        pair_checks = 0
        for start,size,origin in functions:
            end = start+size
            for offset,(word,) in enumerate(struct.iter_unpack('<I',self.text[start-self.text_start:end-self.text_start])):
                tag = word & 0x9f000000
                if tag not in (0x10000000,0x90000000):
                    continue
                reg = word & 31
                if reg == 31:
                    continue  # Discarded architectural destination, not a pointer value.
                pc = start+offset*4
                immediate = ((word >> 5)&0x7ffff)*4+((word >> 29)&3)
                if immediate & 0x100000:
                    immediate -= 0x200000
                if tag == 0x10000000:
                    target = pc+immediate
                    if not self.in_text(target):
                        continue
                    source,method = pc,'adr'
                else:
                    page = (pc & ~0xfff)+immediate*4096
                    # Raw prefilter only saves decoder work; the detailed scan
                    # below still verifies clobbers and every intervening transfer.
                    pattern = 0x91000000 | (reg<<5) | reg
                    matches = []
                    for q in range(pc+4,min(end,pc+4+12*4),4):
                        add = self.word(q)
                        if add & 0xff8003ff == pattern:
                            value = page+(((add>>10)&0xfff) << (12 if add & (1<<22) else 0))
                            if self.in_text(value):
                                matches.append((q,value))
                    if not matches:
                        continue
                    pair_checks += 1
                    selected = None
                    possibilities = dict(matches)
                    for ins in self.decode(pc+4,min(end,pc+4+12*4)):
                        if self.flow(ins):
                            break
                        if ins.address in possibilities:
                            ops = ins.operands
                            if (ins.mnemonic == 'add' and len(ops)==3
                                    and ops[0].type==A.ARM64_OP_REG and ops[1].type==A.ARM64_OP_REG
                                    and ops[2].type==A.ARM64_OP_IMM
                                    and self.reg_key(ops[0].reg)==reg and self.reg_key(ops[1].reg)==reg
                                    and self.md.reg_name(ops[0].reg).startswith('x')):
                                selected = ins.address,possibilities[ins.address]
                                break
                        if reg in self.access(ins)[1]:
                            break
                    if selected is None:
                        continue
                    source,target = selected
                    method = 'adrp_add'
                record = dict(source=source,target=target,category=index.classify(target),
                              method=method,adrp=pc if method=='adrp_add' else None,
                              register=reg,function=start)
                key = source,target
                if self.load_base_exclusion(source,reg,end):
                    excluded[key] = record
                else:
                    accepted[key] = record
        return sorted(accepted.values(),key=lambda r:(r['source'],r['target'])), \
               sorted(excluded.values(),key=lambda r:(r['source'],r['target'])),pair_checks


def synthetic_checks():
    index = FunctionIndex([(0x20,0x10,'eh_frame'),(0x40,8,'gap')],{0x60})
    assert [index.classify(x) for x in (0x20,0x24,0x30,0x40,0x60)] == [
        'inicio_funcao','meio_funcao','fora','inicio_funcao','stub_plt']
    assert maximal_runs([(0x100,'data'),(0x108,'data'),(0x118,'data')]) == [
        (0x100,2,'data'),(0x118,1,'data')]
    # Generic ISA fixtures: prove load-base exclusion does not drop a value
    # subsequently stored as a callback. No instructions are taken from the game.
    sample = struct.pack('<II',0xd503201f,0xf9400108)
    assert CodeAddressFinder(sample,0).load_base_exclusion(0,8,len(sample))
    sample = struct.pack('<III',0xd503201f,0xf9400109,0xf9000008)
    assert not CodeAddressFinder(sample,0).load_base_exclusion(0,8,len(sample))


def run():
    census = load_census()
    text = (ROOT/'analysis/boot/text.bin').read_bytes()
    layout_rows = read_csv('layout.csv')
    layout = {r['nome']:(int(r['rva'],0),int(r['tamanho'])) for r in layout_rows}
    for name in ('.text','.rodata','.data','.bss','init_array'):
        if name not in layout:
            raise ValueError(f'Missing layout entry {name}')
    text_start,text_size = layout['.text']
    if text_start != 0 or text_size != len(text):
        raise ValueError('text.bin and .text layout disagree')
    segments = sorted((s,s+n,name) for name,(s,n) in layout.items() if name.startswith('.'))
    if any(segments[i][1]>segments[i+1][0] for i in range(len(segments)-1)):
        raise ValueError('Overlapping image segments')
    image_start,image_end = segments[0][0],segments[-1][1]

    def segment_of(address):
        for start,end,name in segments:
            if start<=address<end:
                return name
        return 'padding_imagem' if image_start<=address<image_end else 'fora_imagem'

    fde = [(s,n,'eh_frame') for s,n in census.read_functions(len(text))]
    gaps = [(int(r['rva'],0),int(r['tamanho']),'gap') for r in read_csv('gap_functions.csv')]
    plt = {int(r['stub_rva'],0) for r in read_csv('plt_map.csv')}
    index = FunctionIndex(fde+gaps,plt)
    if any(s+n>text_start+text_size for s,n,o in index.functions):
        raise ValueError('Function interval outside .text')
    synthetic_checks()
    raw = read_csv('relocations.csv')
    type_counts = collections.Counter(r['tipo'] for r in raw)
    relative = []
    relative_by_slot = collections.defaultdict(list)
    targets_by_segment = collections.Counter()
    out_of_image = []
    for row in raw:
        if row['tipo'] not in RELATIVE:
            continue
        if not row['alvo_rva']:
            raise ValueError(f'Relative relocation without target at {row["rva"]}')
        slot,target = int(row['rva'],0),int(row['alvo_rva'],0)
        record = dict(slot=slot,target=target,segment=row['segmento'],kind=row['tipo'])
        relative.append(record)
        relative_by_slot[slot].append(record)
        target_segment = segment_of(target)
        targets_by_segment[target_segment] += 1
        if target_segment=='fora_imagem':
            out_of_image.append(record)
    text_pointers = [r for r in relative if text_start<=r['target']<text_start+text_size]
    pointer_counts = collections.Counter(r['target'] for r in text_pointers)
    pointer_classes = collections.Counter(index.classify(r['target']) for r in text_pointers)
    pointer_distinct_classes = collections.Counter(index.classify(t) for t in pointer_counts)
    slot_segments = collections.Counter(r['segment'] for r in text_pointers)
    write_csv('text_pointers.csv',('slot_rva','segmento','alvo_rva','classe'),
              ((hx(r['slot']),r['segment'],hx(r['target']),index.classify(r['target']))
               for r in sorted(text_pointers,key=lambda r:(r['slot'],r['target']))))

    init_start,init_size = layout['init_array']
    if init_start%8 or init_size%8:
        raise ValueError('Unaligned INIT_ARRAY layout')
    init_records = []
    for slot in range(init_start,init_start+init_size,8):
        relocations = relative_by_slot.get(slot,[])
        target = relocations[0]['target'] if len(relocations)==1 else None
        category = index.classify(target) if target is not None else 'sem_relativa_unica'
        init_records.append((slot,target,len(relocations),category))
    init_classes = collections.Counter(r[3] for r in init_records)
    init_good = sum(count==1 and category=='inicio_funcao' for slot,t,count,category in init_records)
    init_issues = [r for r in init_records if r[2]!=1 or r[3]!='inicio_funcao']
    write_csv('init_array_audit.csv',('slot_rva','alvo_rva','relativas_no_slot','classe'),
              ((hx(s),hx(t),n,c) for s,t,n,c in init_records))

    # Every relative pointer into .text participates, including non-entry targets.
    # The INIT_ARRAY is intentionally not removed from this structural census.
    eligible = {(r['slot'],segment_of(r['slot'])) for r in text_pointers
                if segment_of(r['slot']) in ('.rodata','.data') and r['slot']%8==0}
    runs = maximal_runs(eligible)
    hist = collections.Counter(size_bin(n) for s,n,seg in runs)
    write_csv('vtable_candidates.csv',('inicio_rva','entradas','segmento'),
              ((hx(s),n,seg) for s,n,seg in runs))
    largest = max(runs,key=lambda r:(r[1],-r[0])) if runs else None
    progress = [f'Capstone {capstone.__version__}; relocs={len(raw)}; ponteiros .text={len(text_pointers)}; varrendo codigo']
    print(progress[-1],flush=True)
    code,excluded,pair_checks = CodeAddressFinder(text,text_start).scan(index.functions,index)
    code_counts = collections.Counter(r['target'] for r in code)
    code_classes = collections.Counter(r['category'] for r in code)
    code_distinct_classes = collections.Counter(index.classify(t) for t in code_counts)
    code_methods = collections.Counter(r['method'] for r in code)
    write_csv('code_text_addresses.csv',('origem_rva','alvo_rva','classe'),
              ((hx(r['source']),hx(r['target']),r['category']) for r in code))
    write_csv('code_text_address_details.csv',('origem_rva','alvo_rva','classe','metodo','adrp_rva','registrador','funcao_rva'),
              ((hx(r['source']),hx(r['target']),r['category'],r['method'],hx(r['adrp']),
                f'x{r["register"]}',hx(r['function'])) for r in code))
    write_csv('excluded_text_addresses.csv',('origem_rva','alvo_rva','metodo','motivo'),
              ((hx(r['source']),hx(r['target']),r['method'],'base_de_load_ate_sobrescrita') for r in excluded))

    exports = [r for r in read_csv('exports.csv') if r['tipo']=='func']
    export_counts = collections.Counter(int(r['rva'],0) for r in exports)
    export_names = collections.defaultdict(list)
    for row in exports:
        export_names[int(row['rva'],0)].append(row['nome'])
    dispatch = set(pointer_counts)|set(code_counts)|set(export_counts)
    dispatch_classes = collections.Counter(index.classify(t) for t in dispatch)
    def origins(target):
        return ';'.join(name for name,values in [('reloc',pointer_counts),('codigo',code_counts),('export',export_counts)]
                        if target in values)
    write_csv('dispatch_targets.csv',('alvo_rva','classe','origens','referencias_reloc','referencias_codigo','exports'),
              ((hx(t),index.classify(t),origins(t),pointer_counts[t],code_counts[t],
                ';'.join(sorted(export_names[t]))) for t in sorted(dispatch)))
    write_csv('relative_target_segments.csv',('segmento_alvo','referencias'),sorted(targets_by_segment.items()))
    write_csv('relative_targets_outside_image.csv',('slot_rva','segmento','alvo_rva'),
              ((hx(r['slot']),r['segment'],hx(r['target'])) for r in out_of_image))

    check1 = len(raw)==EXPECTED_RELOCATIONS and all(type_counts[t]==n for t,n in EXPECTED_TYPES.items()) \
             and not(set(type_counts)-set(EXPECTED_TYPES))
    check2 = len(init_records)==EXPECTED_INIT and init_good==EXPECTED_INIT and not init_issues
    check3 = not out_of_image and sum(targets_by_segment.values())==len(relative)
    check4 = (sum(pointer_classes.values())==len(text_pointers)
              and sum(pointer_distinct_classes.values())==len(pointer_counts)
              and sum(slot_segments.values())==len(text_pointers)
              and sum(init_classes.values())==len(init_records)
              and sum(hist.values())==len(runs) and sum(n for s,n,seg in runs)==len(eligible)
              and sum(code_classes.values())==len(code)
              and sum(code_distinct_classes.values())==len(code_counts)
              and sum(dispatch_classes.values())==len(dispatch))
    middle = sorted((t for t in pointer_counts if index.classify(t)=='meio_funcao'),key=lambda t:(-pointer_counts[t],t))[:20]
    outside_functions = sorted(t for t in pointer_counts if index.classify(t)=='fora')
    proposal_middle = sum(index.containing(t)[2]=='gap' for t in pointer_counts if index.classify(t)=='meio_funcao')
    distinct_nonentries = len(dispatch)-dispatch_classes['inicio_funcao']
    result = [f'Relocacoes: {len(raw)}; relativas: {len(relative)}',
        f'Ponteiros .text: {len(text_pointers)}; alvos distintos: {len(pointer_counts)}',
        'Ponteiros por classe: '+'; '.join(f'{c}={pointer_classes[c]}' for c in CLASSES),
        f'INIT_ARRAY: {len(init_records)} slots; relativos e inicio_funcao: {init_good}',
        f'Candidatas vtable: {len(runs)}; maior: {largest[1] if largest else 0} entradas',
        f'Enderecos no codigo: {len(code)}; distintos: {len(code_counts)}; excluidos base-load: {len(excluded)}',
        'Codigo por classe: '+'; '.join(f'{c}={code_classes[c]}' for c in CLASSES),
        f'Despacho estimado: {len(dispatch)}; inicio_funcao: {dispatch_classes["inicio_funcao"]}; outros: {distinct_nonentries}',
        'Despacho por classe: '+'; '.join(f'{c}={dispatch_classes[c]}' for c in CLASSES),
        f'AUTOVERIFICACAO 1: {"OK" if check1 else "FALHA"}; total e tipos de relocacao',
        f'AUTOVERIFICACAO 2: {"OK" if check2 else "DIFERENCA"}; INIT_ARRAY {init_good}/{EXPECTED_INIT}',
        f'AUTOVERIFICACAO 3: {"OK" if check3 else "DIFERENCA"}; relativos fora da imagem: {len(out_of_image)}',
        f'AUTOVERIFICACAO 4: {"OK" if check4 else "FALHA"}; somas de classes, segmentos e sequencias',
        'Saidas: analysis/recomp/generated/*.csv e analysis/recomp/indirect_targets_report.md']
    md_lines = ['# Alvos indiretos para a tabela de despacho R4','',
        'Inventário estático, sem executar o jogo. Todos os endereços são RVAs. '
        'A união é uma estimativa estrutural; um endereço em `.text` não prova por si só um ponteiro de função.', '',
        '## Entradas e classificação','',
        f'- `.eh_frame`: **{len(fde)}** funções; propostas das lacunas: **{len(gaps)}**; stubs PLT: **{len(plt)}**.',
        f'- `.text`: **{hx(text_start)}** a **{hx(text_start+text_size)}**, limite final exclusivo.',
        f'- Imagem: **{hx(image_start)}** a **{hx(image_end)}**, limite final exclusivo.',
        '- Classes: início de função conhecida, stub PLT, interior de função e fora desses intervalos. '
        'As propostas das lacunas continuam tendo limites heurísticos.', '',
        '## Parte 1 — ponteiros relocados para .text','',
        f'- Relocações relativas totais: **{len(relative)}**.',
        f'- Referências para `.text`: **{len(text_pointers)}**; destinos distintos: **{len(pointer_counts)}**.', '',
        '| Classe | Referências | Alvos distintos |','|---|---:|---:|',
        *[f'| {c} | {pointer_classes[c]} | {pointer_distinct_classes[c]} |' for c in CLASSES], '',
        '| Segmento do slot, conforme CSV | Referências para .text |','|---|---:|',
        *[f'| {s} | {n} |' for s,n in sorted(slot_segments.items())], '',
        '### Vinte destinos interiores mais referenciados','',
        '| Alvo RVA | Função contendo, RVA | Deslocamento | Referências |','|---|---|---:|---:|',
        *[f'| {hx(t)} | {hx(index.containing(t)[0])} | {hx(t-index.containing(t)[0])} | {pointer_counts[t]} |' for t in middle], '',
        f'Dos destinos interiores distintos, **{proposal_middle}** estão em intervalos propostos das lacunas. '
        'Essa classificação pode sinalizar entradas adicionais/aliases dentro de um intervalo inferido; '
        'não autoriza promover todo endereço automaticamente.', '',
        '### Referências fora de funções e PLT','',
        '| Alvo RVA | Referências |','|---|---:|',
        *[f'| {hx(t)} | {pointer_counts[t]} |' for t in outside_functions], '',
        'Esses endereços são mantidos na união, com classe `fora`; limites de segmento e metadados '
        'também podem produzir ponteiros para `.text` sem representar código chamável.', '',
        '## Parte 2 — INIT_ARRAY','',
        f'- Início **{hx(init_start)}**, tamanho **{init_size} bytes**, **{len(init_records)} slots** de oito bytes.',
        f'- Slots com exatamente uma relocação relativa e destino `inicio_funcao`: **{init_good}**.',
        f'- Invariante de 1912 entradas: **{"OK" if check2 else "DIFERENÇA; ver abaixo e o CSV de auditoria"}**.', '',
        '## Parte 3 — candidatas a vtable','',
        f'- Sequências máximas: **{len(runs)}**, cobrindo **{len(eligible)} slots** em `.rodata`/`.data`.', '',
        '| Entradas por sequência | Sequências |','|---|---:|',
        *[f'| {bucket} | {hist[bucket]} |' for bucket in ('1','2-3','4-7','8-15','16-63','64+')], '',
        f'Maior sequência: **{hx(largest[0]) if largest else "nenhuma"}**, '
        f'**{largest[1] if largest else 0} entradas**, segmento **{largest[2] if largest else "nenhum"}**.', '',
        'As sequências incluem singletons e não excluem o INIT_ARRAY: o critério pedido é apenas '
        'contiguidade de slots relativos para `.text`. Assim, nem toda sequência é uma vtable semântica.', '',
        '## Parte 4 — endereços montados no código','',
        f'- Materializações aceitas: **{len(code)}**; destinos distintos: **{len(code_counts)}**.',
        f'- Métodos: **{code_methods["adr"]} ADR**, **{code_methods["adrp_add"]} ADRP/ADD**.',
        f'- Cadeias ADRP candidatas verificadas em detalhe: **{pair_checks}**.',
        f'- Excluídas por uso exclusivo como base de load até sobrescrita: **{len(excluded)}**.', '',
        '| Classe | Materializações | Alvos distintos |','|---|---:|---:|',
        *[f'| {c} | {code_classes[c]} | {code_distinct_classes[c]} |' for c in CLASSES], '',
        'Todas as funções conhecidas e propostas são percorridas. ADRP/ADD exige fonte e destino ADD '
        'iguais ao registrador ADRP, no máximo doze instruções adiante, sem sobrescrita ou transferência '
        'intermediária. A origem registrada é o ADR ou o ADD que completa o endereço.', '',
        'O filtro de loads exige primeiro uso nas quatro instruções seguintes, apenas usos como base '
        'de load e sobrescrita que encerre o valor em até doze instruções. Se houver armazenamento do '
        'ponteiro, outro uso, transferência de controle ou valor ainda vivo ao fim da janela, a candidata '
        'é conservada. Isso evita descartar callbacks só porque também são usados numa leitura.', '',
        'Endereços de labels e bases de destinos relativos podem permanecer como candidatos. '
        'A análise local não é prova de que cada constante será chamada por BR/BLR, nem cobre '
        'ponteiros construídos por outros padrões ou gravados dinamicamente.', '',
        '## Parte 5 — união para despacho','',
        f'- Exportações de função: **{len(exports)}** nomes, **{len(export_counts)}** destinos distintos.',
        f'- União de relocados, código e exports: **{len(dispatch)}** destinos.',
        f'- Inícios conhecidos: **{dispatch_classes["inicio_funcao"]}**; demais: **{distinct_nonentries}**.', '',
        '| Classe | Destinos na união |','|---|---:|',
        *[f'| {c} | {dispatch_classes[c]} |' for c in CLASSES], '',
        'O CSV de união preserva origens e contagens de evidência. Destinos interiores e `fora` não '
        'são silenciosamente promovidos a funções traduzidas. Relocações por símbolo para o SDK '
        'e endereços resolvidos em runtime não entram como relativos estáticos para o código do main.', '',
        '## Autoverificações','',
        '| Tipo de relocação | Linhas |','|---|---:|',
        *[f'| {t} | {type_counts[t]} |' for t in ('relr','relative','abs64','jump_slot','glob_dat')], '',
        '| Segmento de destino dos relativos | Referências |','|---|---:|',
        *[f'| {s} | {n} |' for s,n in sorted(targets_by_segment.items())], '',
        f'- Fora da imagem: **{len(out_of_image)}**; alvos em padding entre segmentos são contados separadamente.',
        '- As somas conferem referências, destinos distintos, INIT_ARRAY, segmentos e histograma de sequências.', '',
        '## Arquivos','',
        '- `generated/text_pointers.csv`: ponteiros relativos para `.text`.',
        '- `generated/init_array_audit.csv`: auditoria de cada slot inicializador.',
        '- `generated/vtable_candidates.csv`: sequências máximas de slots.',
        '- `generated/code_text_addresses.csv`: endereços materializados aceitos.',
        '- `generated/code_text_address_details.csv`: método, registrador e origem ADRP.',
        '- `generated/excluded_text_addresses.csv`: exclusões justificadas pelo filtro de loads.',
        '- `generated/dispatch_targets.csv`: união estimada para despacho.',
        '- `generated/relative_target_segments.csv`: contagem dos destinos relativos por segmento.',
        '- `generated/relative_targets_outside_image.csv`: exceções de limites de imagem.', '',
        '## Aceite','', '```text',
        r'build\venv\Scripts\python.exe -I analysis\recomp\indirect_targets.py',*progress,*result,'```','']
    if init_issues:
        md_lines += ['## Diferenças do INIT_ARRAY','',
            '| Slot RVA | Alvo RVA | Relativas no slot | Classe |','|---|---|---:|---|',
            *[f'| {hx(s)} | {hx(t)} | {n} | {c} |' for s,t,n,c in init_issues],'']
    if out_of_image:
        md_lines += ['## Destinos relativos fora da imagem','',
            '| Slot RVA | Alvo RVA |','|---|---|',
            *[f'| {hx(r["slot"])} | {hx(r["target"])} |' for r in out_of_image],'']
    REPORT.write_text('\n'.join(md_lines),encoding='utf-8',newline='\n')
    safety = subprocess.run([sys.executable,'-I',str(ROOT/'tools/check_repo_safety.py')],cwd=ROOT,
                            capture_output=True,text=True,encoding='utf-8',errors='replace')
    safety_output = (safety.stdout+safety.stderr).strip()
    md_lines += ['```text',r'python tools\check_repo_safety.py',safety_output,'```','',
                 f'Código de saída do verificador: **{safety.returncode}**. Nenhum commit ou push foi realizado.','']
    REPORT.write_text('\n'.join(md_lines),encoding='utf-8',newline='\n')
    for line in result:
        print(line)
    if safety.returncode:
        print(safety_output,file=sys.stderr)
    return 0 if all((check1,check2,check3,check4)) and safety.returncode==0 else 1


if __name__=='__main__':
    raise SystemExit(run())
