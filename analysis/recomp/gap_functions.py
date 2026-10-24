#!/usr/bin/env python3
"""Propose AArch64 code intervals outside the unwind inventory, without execution.

Only this report and CSV files under generated/ are written. The proposals are
operational translation units; they do not claim original source boundaries.
"""
from __future__ import annotations

import bisect
import collections
import csv
from dataclasses import dataclass, field
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
REPORT = HERE / 'gap_functions_report.md'
PRIORITY = {'alvo_bl': 0, 'export': 1, 'sequencia': 2, 'adr': 3, 'propagado': 4}
PADDING = {'udf', 'nop'}
TERMINATORS = {'ret', 'b', 'br', 'udf', 'brk'}


def load_census():
    spec = importlib.util.spec_from_file_location('census_helpers', HERE / 'control_flow_census.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def csv_read(name):
    with (OUT / name).open(encoding='utf-8-sig', newline='') as stream:
        return list(csv.DictReader(stream))


def hx(value):
    return '' if value is None else f'0x{value:x}'


def padding_word(word):
    return word < 0x10000 or word == 0xd503201f


def merged_intervals(intervals):
    result = []
    for start, end in sorted(intervals):
        if result and start <= result[-1][1]:
            result[-1] = (result[-1][0], max(end, result[-1][1]))
        else:
            result.append((start, end))
    return result


@dataclass
class Gap:
    start: int
    end: int
    category: str = ''
    instructions: list = field(default_factory=list)
    targets: int = 0
    exports: int = 0
    invalid: int = 0

    @property
    def size(self):
        return self.end - self.start


class GapIndex:
    def __init__(self, gaps):
        self.gaps = gaps
        self.starts = [gap.start for gap in gaps]

    def find(self, address):
        index = bisect.bisect_right(self.starts, address) - 1
        if index >= 0 and address < self.gaps[index].end:
            return self.gaps[index]
        return None


def classify_gap(gap, text, md):
    if all(padding_word(word) for word, in struct.iter_unpack('<I', text[gap.start:gap.end])):
        gap.category = 'zeros/padding'
        return
    instructions = list(md.disasm_lite(text[gap.start:gap.end], gap.start))
    if (sum(row[1] for row in instructions) != gap.size
            or any(row[1] != 4 for row in instructions)):
        raise ValueError(f'Decoder coverage mismatch at {hx(gap.start)}')
    gap.invalid = sum(row[2] == '.byte' for row in instructions)
    terminates = any(row[2] in ('ret', 'b', 'br') for row in instructions)
    gap.category = 'dados' if gap.invalid * 2 > len(instructions) or not terminates else 'codigo'
    if gap.category == 'codigo':
        gap.instructions = instructions


def typical_prologue(row, text):
    pc, _, mnemonic, _ = row
    word = struct.unpack_from('<I', text, pc)[0]
    if mnemonic == 'sub' and word & 0xffc003ff == 0xd10003ff:
        return True  # Stack allocation with the same source/destination SP.
    return mnemonic == 'stp' and (word >> 5) & 31 == 31 and (word >> 23) & 3 == 3


def add_seed(seeds, evidence, index, address, origin, source=None):
    gap = index.find(address)
    if address % 4 or gap is None or gap.category != 'codigo':
        return False
    old = seeds.get(address)
    evidence.add((address, origin, source))
    if old is None or PRIORITY[origin] < PRIORITY[old]:
        seeds[address] = origin
        return old is None
    return False


def register_key(md, reg):
    name = md.reg_name(reg)
    if name in ('fp', 'lr'):
        return 29 if name == 'fp' else 30
    if name.startswith(('w', 'x')) and name[1:].isdigit():
        return int(name[1:])
    return None


def address_seeds(text, functions, index, seeds, evidence):
    """ADR or a short, unclobbered ADRP/ADD chain in original known code."""
    pages = set()
    for gap in index.gaps:
        if gap.category == 'codigo':
            pages.update(range(gap.start & ~0xfff, (gap.end - 1 & ~0xfff) + 1, 4096))
    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
    md.detail = True
    references = 0
    for start, size in functions:
        end = start + size
        for offset, (word,) in enumerate(struct.iter_unpack('<I', text[start:end])):
            tag = word & 0x9f000000
            if tag not in (0x10000000, 0x90000000):
                continue
            pc = start + offset * 4
            immediate = ((word >> 5) & 0x7ffff) * 4 + ((word >> 29) & 3)
            if immediate & 0x100000:
                immediate -= 0x200000
            if tag == 0x10000000:
                target = pc + immediate
                gap = index.find(target)
                if gap is not None and gap.category == 'codigo' and not target % 4:
                    references += 1
                    add_seed(seeds, evidence, index, target, 'adr', pc)
                continue
            page = (pc & ~0xfff) + immediate * 4096
            if page not in pages:
                continue
            reg = word & 31
            # Stop at control transfers and writes of the page register. This
            # deliberately does not pretend to solve general constant propagation.
            for ins in md.disasm(text[pc+4:min(end, pc+4+12*4)], pc+4):
                ops = ins.operands
                if (ins.mnemonic == 'add' and len(ops) == 3
                        and ops[0].type == A.ARM64_OP_REG and ops[1].type == A.ARM64_OP_REG
                        and ops[2].type == A.ARM64_OP_IMM
                        and register_key(md, ops[1].reg) == reg
                        and md.reg_name(ops[0].reg).startswith('x')
                        and ops[2].shift.type in (0, A.ARM64_SFT_LSL)):
                    target = page + (ops[2].imm << ops[2].shift.value)
                    gap = index.find(target)
                    if gap is not None and gap.category == 'codigo' and not target % 4:
                        references += 1
                        add_seed(seeds, evidence, index, target, 'adr', ins.address)
                if ins.mnemonic in ('b', 'br', 'ret', 'bl', 'blr') or ins.mnemonic.startswith('b.'):
                    break
                if reg in {register_key(md, r) for r in ins.regs_access()[1]}:
                    break
    return references


def proposals(gaps, seeds):
    rows = []
    all_starts = sorted(seeds)
    for gap in gaps:
        if gap.category != 'codigo':
            continue
        first = bisect.bisect_left(all_starts, gap.start)
        last = bisect.bisect_left(all_starts, gap.end)
        starts = all_starts[first:last]
        for i, start in enumerate(starts):
            boundary = starts[i+1] if i+1 < len(starts) else gap.end
            end = boundary
            trimmed = boundary
            while trimmed > start and gap.instructions[(trimmed-gap.start)//4-1][2] in PADDING:
                trimmed -= 4
            # A non-terminal suffix needs the aligned padding in order to fall
            # through to the next start. Only remove padding after a terminator.
            if trimmed > start and gap.instructions[(trimmed-gap.start)//4-1][2] in TERMINATORS:
                end = trimmed
            elif trimmed == start:
                if gap.instructions[(start-gap.start)//4][2] == 'udf':
                    end = start + 4
            final = gap.instructions[(end-gap.start)//4-1][2]
            rows.append(dict(start=start, end=end, origin=seeds[start], gap=gap,
                             last=final, boundary=boundary,
                             removed_padding=boundary-end,
                             retained_padding=boundary-trimmed if end == boundary else 0))
    return sorted(rows, key=lambda row: row['start'])


def proposed_calls(rows, text, direct_target):
    for row in rows:
        gap = row['gap']
        lo = (row['start']-gap.start)//4
        hi = (row['end']-gap.start)//4
        for pc, _, mnemonic, _ in gap.instructions[lo:hi]:
            if mnemonic not in ('bl', 'b'):
                continue
            target = direct_target(struct.unpack_from('<I', text, pc)[0], pc)
            if mnemonic == 'bl' or not row['start'] <= target < row['end']:
                yield pc, target, mnemonic


def synthetic_checks(md):
    assert padding_word(0) and padding_word(0xd503201f) and not padding_word(0xd65f03c0)
    sample = struct.pack('<II', 0xd65f03c0, 0xd503201f)
    gap = Gap(0, 8)
    classify_gap(gap, sample, md)
    assert gap.category == 'codigo'
    row = proposals([gap], {0: 'sequencia'})[0]
    assert row['end'] == 4 and row['last'] == 'ret' and row['removed_padding'] == 4
    gap = Gap(0, 8, 'codigo', [(0,4,'mov',''), (4,4,'nop','')])
    row = proposals([gap], {0: 'sequencia'})[0]
    assert row['end'] == 8 and row['retained_padding'] == 4


def run():
    census = load_census()
    text = (ROOT / 'analysis/boot/text.bin').read_bytes()
    functions = census.read_functions(len(text))
    imports, slots = census.read_imports()
    plt, unknown_plt, resolver_bodies, _ = census.find_plt(text, slots)
    recorded_plt = {int(row['stub_rva'],0): (int(row['got_rva'],0), row['import'])
                    for row in csv_read('plt_map.csv')}
    if plt != recorded_plt:
        raise ValueError('PLT census differs from plt_map.csv; refresh the inputs')
    plt_intervals = [(start, start+16) for start in plt]
    plt_intervals += [(start, start+16) for start, _ in unknown_plt]
    for body, _ in resolver_bodies:
        begin, end = body-4, body+28
        if end > len(text) or any(struct.unpack_from('<I',text,p)[0] != 0xd503201f
                                  for p in range(body+16,end,4)):
            raise ValueError('Unexpected resolver PLT0 suffix')
        plt_intervals.append((begin,end))
    excluded = merged_intervals([(s,s+n) for s,n in functions] + plt_intervals)
    gaps = []
    end = 0
    for start, stop in excluded:
        if start > end:
            gaps.append(Gap(end,start))
        end = stop
    if end < len(text):
        gaps.append(Gap(end,len(text)))
    assert sum(g.size for g in gaps) + sum(e-s for s,e in excluded) == len(text)

    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
    md.skipdata = True
    synthetic_checks(md)
    for gap in gaps:
        classify_gap(gap,text,md)
    index = GapIndex(gaps)
    old_targets = csv_read('call_targets.csv')
    original_gap_targets = [int(row['alvo_rva'],0) for row in old_targets if row['classe']=='lacuna']
    exports = csv_read('exports.csv')
    seeds, evidence = {}, set()
    for row in old_targets:
        if row['classe'] == 'lacuna':
            address = int(row['alvo_rva'],0)
            gap = index.find(address)
            if gap is not None:
                gap.targets += 1
            add_seed(seeds,evidence,index,address,'alvo_bl',int(row['exemplo_de_origem_rva'],0))
    for row in exports:
        address = int(row['rva'],0)
        gap = index.find(address)
        if gap is not None:
            gap.exports += 1
            if row['tipo'] == 'func':
                add_seed(seeds,evidence,index,address,'export',address)
    for gap in gaps:
        if gap.category != 'codigo':
            continue
        previous = None
        for row in gap.instructions:
            pc, _, mnemonic, _ = row
            if mnemonic not in PADDING:
                if previous is None or previous in PADDING:
                    add_seed(seeds,evidence,index,pc,'sequencia',pc)
                elif previous in ('ret','br','b') and typical_prologue(row,text):
                    add_seed(seeds,evidence,index,pc,'sequencia',pc)
            previous = mnemonic
    progress = [f'Capstone {capstone.__version__}; lacunas={len(gaps)}; descobrindo referencias ADR']
    print(progress[-1],flush=True)
    address_references = address_seeds(text,functions,index,seeds,evidence)
    rounds = 0
    while True:
        rounds += 1
        rows = proposals(gaps,seeds)
        additions = 0
        for source, target, _ in proposed_calls(rows,text,census.direct_target):
            additions += add_seed(seeds,evidence,index,target,'propagado',source)
        progress.append(f'Propagacao {rounds}: {additions} novos inicios; total={len(seeds)}')
        print(progress[-1],flush=True)
        if not additions:
            break
    rows = proposals(gaps,seeds)

    mandatory_missing = sorted(set(original_gap_targets)-{r['start'] for r in rows})
    combined = sorted([(s,s+n,'eh_frame') for s,n in functions]
                      + [(r['start'],r['end'],'proposta') for r in rows])
    combined_starts = [s for s,e,o in combined]
    combined_entries = set(combined_starts)
    overlap = []
    last_end = 0
    for start, end, origin in combined:
        if start % 4 or end % 4 or start < last_end or end <= start:
            overlap.append((start,end,origin))
        last_end = max(last_end,end)
    for row in rows:
        if any(row['start'] < end and start < row['end'] for start,end in plt_intervals):
            overlap.append((row['start'],row['end'],'plt'))
    bad_ends = []
    fallthrough = 0
    for row in rows:
        if row['last'] in TERMINATORS:
            row['end_kind'] = 'terminador'
        elif row['end'] in combined_entries:
            row['end_kind'] = 'fallthrough'
            fallthrough += 1
        else:
            row['end_kind'] = 'nao_resolvido'
            bad_ends.append(row['start'])

    counts = collections.Counter({int(row['alvo_rva'],0): int(row['quantidade_de_chamadas'])
                                  for row in old_targets})
    examples = {int(row['alvo_rva'],0): int(row['exemplo_de_origem_rva'],0) for row in old_targets}
    new_calls = collections.Counter()
    for source, target, mnemonic in proposed_calls(rows,text,census.direct_target):
        counts[target] += 1
        examples.setdefault(target,source)
        new_calls[mnemonic] += 1

    def classify(target):
        if target in combined_entries:
            return 'inicio_funcao'
        if target in plt or any(target == start for start,_ in unknown_plt):
            return 'stub_plt'
        i = bisect.bisect_right(combined_starts,target)-1
        if i >= 0 and target < combined[i][1]:
            return 'meio_funcao'
        return 'lacuna'

    target_classes = collections.Counter(classify(target) for target in counts)
    remaining = sorted(t for t in counts if classify(t)=='lacuna')
    reasons = []
    for target in remaining:
        gap = index.find(target)
        reason = ('fora_text' if not 0 <= target < len(text) else
                  'plt_resolver' if any(s<=target<e for s,e in plt_intervals) else
                  f'lacuna_{gap.category}' if gap else 'limite_nao_resolvido')
        reasons.append((target,reason))
    checks = [len(original_gap_targets)==17 and not mandatory_missing,
              not overlap, not bad_ends, not remaining]
    stat = collections.Counter(g.category for g in gaps)
    sizes = collections.Counter()
    for gap in gaps:
        sizes[gap.category] += gap.size
    origins = collections.Counter(r['origin'] for r in rows)

    census.csv_write('gaps.csv', ('inicio','tamanho','classe','alvos_de_chamada','exports_dentro'),
                     ((hx(g.start),g.size,g.category,g.targets,g.exports) for g in gaps))
    census.csv_write('gap_functions.csv', ('rva','tamanho','origem'),
                     ((hx(r['start']),r['end']-r['start'],r['origin']) for r in rows))
    census.csv_write('gap_function_details.csv', ('rva','fim_rva','origem','tipo_fim','ultimo_mnemonico',
                      'lacuna_inicio','padding_removido','padding_conservado'),
                     ((hx(r['start']),hx(r['end']),r['origin'],r['end_kind'],r['last'],hx(r['gap'].start),
                       r['removed_padding'],r['retained_padding']) for r in rows))
    census.csv_write('gap_seed_evidence.csv', ('rva','origem','fonte_rva'),
                     ((hx(a),o,hx(s)) for a,o,s in sorted(evidence,key=lambda x:(x[0],PRIORITY[x[1]],x[2] or 0))))
    census.csv_write('call_targets_after_gaps.csv', ('alvo_rva','classe','quantidade_de_chamadas','exemplo_de_origem_rva'),
                     ((hx(t),classify(t),counts[t],hx(examples[t])) for t in sorted(counts)))
    census.csv_write('gap_unresolved.csv', ('alvo_rva','referencias','motivo'),
                     ((hx(t),counts[t],reason) for t,reason in reasons))

    result = [f'Lacunas: {len(gaps)}; bytes={sum(g.size for g in gaps)}; PLT excluida',
              'Classes: '+ '; '.join(f'{c}={stat[c]}/{sizes[c]} bytes' for c in ('zeros/padding','dados','codigo')),
              f'Propostas: {len(rows)}; bytes={sum(r["end"]-r["start"] for r in rows)}; rodadas={rounds}',
              f'AUTOVERIFICACAO 1: {"OK" if checks[0] else "FALHA"}; alvos originais={len(original_gap_targets)}; ausentes={len(mandatory_missing)}',
              f'AUTOVERIFICACAO 2: {"OK" if checks[1] else "FALHA"}; desalinhamentos/sobreposicoes={len(overlap)}',
              f'AUTOVERIFICACAO 3: {"OK" if checks[2] else "FALHA"}; finais nao resolvidos={len(bad_ends)}; fall-through={fallthrough}',
              f'AUTOVERIFICACAO 4: {"OK" if checks[3] else "FALHA"}; alvos lacuna apos uniao={len(remaining)}',
              'Alvos apos uniao: '+ '; '.join(f'{c}={target_classes[c]}' for c in census.CLASSES),
              'Saidas: analysis/recomp/generated/*.csv e analysis/recomp/gap_functions_report.md']
    largest = sorted((g for g in gaps if g.category=='codigo'),key=lambda g:(-g.size,g.start))[:10]
    md_lines = ['# Descoberta de funções nas lacunas do unwind', '',
        'Levantamento estático para R4, sem executar o jogo. Todos os endereços são RVAs. '
        'Os limites propostos são unidades de tradução inferidas, não limites originais comprovados.', '',
        '## Inventário', '',
        f'- Funções conhecidas: **{len(functions)}**; `.text`: **{len(text)} bytes**.',
        f'- Lacunas excluindo a PLT: **{len(gaps)}**, **{sum(g.size for g in gaps)} bytes**.',
        f'- Stubs PLT excluídos: **{len(plt)}**; corpos PLT0 excluídos: **{len(resolver_bodies)}**.', '',
        '| Classe | Lacunas | Bytes |', '|---|---:|---:|',
        *[f'| {c} | {stat[c]} | {sizes[c]} |' for c in ('zeros/padding','dados','codigo')], '',
        'Zeros/padding exige somente palavras zero, `UDF` ou `NOP`. Dados significa maioria de palavras '
        'não decodificáveis, ou ausência de `RET`, `B` e `BR`. As demais regiões decodificáveis com esses '
        'terminadores são classificadas como código. O prefixo e o sufixo de `.text` também são inventariados.', '',
        '### Até dez maiores lacunas de código', '',
        f'Foram encontradas {stat["codigo"]} lacunas dessa classe; são exibidas {len(largest)}.', '',
        '| Início RVA | Tamanho em bytes |', '|---|---:|',
        *[f'| {hx(g.start)} | {g.size} |' for g in largest], '',
        '## Inícios e limites propostos', '',
        f'- Propostas: **{len(rows)}**, cobrindo **{sum(r["end"]-r["start"] for r in rows)} bytes**.',
        f'- Referências ADR ou ADRP/ADD em código conhecido: **{address_references}**.',
        f'- Propagação até ponto fixo: **{rounds} rodadas**; novas referências contadas: '
        f'**{new_calls["bl"]} BL**, **{new_calls["b"]} B externos**.', '',
        '| Origem prioritária | Propostas |', '|---|---:|',
        *[f'| {origin} | {origins[origin]} |' for origin in PRIORITY], '',
        'Prioridade preservada: alvo do censo, exportação de função, sequência, referência de endereço e '
        'propagação. Exportações de objetos contam no inventário, mas não geram inícios de função. '
        'Referências de endereço precisam estar alinhadas e cair numa lacuna classificada como código.', '',
        'Sequência considera o primeiro código da lacuna, o primeiro endereço após padding e um prólogo de pilha '
        'típico após `RET`, `BR` ou `B`. Não se presume que toda instrução após um retorno seja uma função nova.', '',
        'ADR é resolvido diretamente; ADRP/ADD usa até doze instruções seguintes, interrompendo a busca '
        'quando o registrador é sobrescrito ou há transferência de controle. Esses endereços podem ser labels '
        'internos e sua origem continua identificada no CSV.', '',
        'Cada fim usa o início seguinte ou a borda da lacuna. Padding posterior a terminador é removido. '
        'Quando existe fall-through real até o próximo início, seu padding é conservado para não criar '
        'uma borda intermediária sem terminador. Os bytes removidos/conservados constam no CSV detalhado.', '',
        'Todo BL em propostas gera início se seu alvo estiver numa lacuna de código. B só gera início quando '
        'sai da proposta corrente; branches internos não são todos promovidos a funções. Isso também permite '
        'descobrir chamadas a outras funções dentro da mesma lacuna física. O processo é repetido até estabilizar.', '',
        '## Quatro autoverificações', '',
        '| Verificação | Resultado |', '|---|---|',
        f'| 1. Todos os 17 alvos do censo são inícios | {"OK" if checks[0] else "FALHA"}; ausentes={len(mandatory_missing)} |',
        f'| 2. Alinhamento e ausência de sobreposição | {"OK" if checks[1] else "FALHA"}; problemas={len(overlap)} |',
        f'| 3. Terminador ou fall-through declarado | {"OK" if checks[2] else "FALHA"}; fall-through={fallthrough}; outros={len(bad_ends)} |',
        f'| 4. Reclassificação incluindo as propostas | {"OK" if checks[3] else "FALHA"}; lacunas restantes={len(remaining)} |', '',
        '| Classe final dos alvos | Quantidade distinta |', '|---|---:|',
        *[f'| {c} | {target_classes[c]} |' for c in census.CLASSES], '',
        'A reclassificação inclui as referências originais do censo e os BL/B externos das propostas. '
        'O censo original e seus CSVs não são alterados.', '',
        '## Limites da inferência', '',
        'Um intervalo pode conter mais de uma função original ou representar um fragmento. '
        'Padding, prólogos e endereços materializados são evidência heurística; indirect calls, '
        'vtables em segmentos de dados e branches condicionais não promovem automaticamente todos '
        'os seus destinos. As verificações demonstram cobertura dos alvos diretos e coerência dos '
        'intervalos propostos, não equivalência de execução nem recuperação completa do CFG.', '',
        '## Arquivos', '',
        '- `generated/gaps.csv`: todas as lacunas e classificação.',
        '- `generated/gap_functions.csv`: rva, tamanho e origem para o recompilador.',
        '- `generated/gap_function_details.csv`: fins, terminadores e fall-through.',
        '- `generated/gap_seed_evidence.csv`: origem e endereço de evidência de cada início.',
        '- `generated/call_targets_after_gaps.csv`: reclassificação após a união.',
        '- `generated/gap_unresolved.csv`: alvos ainda em lacunas e motivo.', '',
        '## Aceite', '', '```text',
        r'build\venv\Scripts\python.exe -I analysis\recomp\gap_functions.py', *progress, *result, '```', '']
    if remaining or mandatory_missing or bad_ends:
        md_lines += ['## Pendências', '']
        for target,reason in reasons:
            md_lines.append(f'- {hx(target)}: {reason}; referências={counts[target]}.')
        for target in mandatory_missing:
            md_lines.append(f'- Início obrigatório ausente: {hx(target)}.')
        for target in bad_ends:
            md_lines.append(f'- Fim sem terminador/fall-through: {hx(target)}.')
        md_lines.append('')
    REPORT.write_text('\n'.join(md_lines),encoding='utf-8',newline='\n')
    safety = subprocess.run([sys.executable,'-I',str(ROOT/'tools/check_repo_safety.py')],cwd=ROOT,
                            capture_output=True,text=True,encoding='utf-8',errors='replace')
    safety_output = (safety.stdout+safety.stderr).strip()
    md_lines += ['```text',r'python tools\check_repo_safety.py',safety_output,'```','',
                 f'Código de saída do verificador: **{safety.returncode}**. Nenhum commit ou push foi realizado.', '']
    REPORT.write_text('\n'.join(md_lines),encoding='utf-8',newline='\n')
    for line in result:
        print(line)
    if safety.returncode:
        print(safety_output,file=sys.stderr)
    return 0 if all(checks) and safety.returncode==0 else 1


if __name__ == '__main__':
    raise SystemExit(run())
