#!/usr/bin/env python3
"""Static AArch64 control-flow census; detailed evidence stays in generated CSVs.

Run from any directory with the isolated Python interpreter described in CODEX_TASK.md.
No target execution, source generation, database mutation, commit, or push occurs.
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

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'analysis' / 'recomp' / 'generated'
REPORT = ROOT / 'analysis' / 'recomp' / 'control_flow_report.md'
CLASSES = ('inicio_funcao', 'stub_plt', 'meio_funcao', 'lacuna')
EXPECTED_JUMP_SLOTS = 917


def rva(value: int | None) -> str:
    return '' if value is None else f'0x{value:x}'


def number(value: str) -> int:
    return int(value, 16) if value.lower().startswith('0x') else int(value)


def csv_write(name: str, columns: tuple[str, ...], rows) -> None:
    with (OUT / name).open('w', encoding='utf-8', newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(columns)
        writer.writerows(rows)


def read_functions(text_size: int):
    with (OUT / 'functions.csv').open(encoding='utf-8-sig', newline='') as stream:
        rows = [(number(row['rva']), number(row['tamanho'])) for row in csv.DictReader(stream)]
    rows.sort()
    previous_end = 0
    for start, size in rows:
        if start % 4 or size <= 0 or size % 4 or start < previous_end or start + size > text_size:
            raise ValueError(f'Invalid or overlapping function interval at {rva(start)}')
        previous_end = start + size
    if not rows:
        raise ValueError('Empty function inventory')
    return rows


def read_imports():
    slots: dict[int, str] = {}
    with (OUT / 'imports.csv').open(encoding='utf-8-sig', newline='') as stream:
        rows = list(csv.DictReader(stream))
    for row in rows:
        # One symbol may have ABS64 slots as well as a JUMP_SLOT. Match addresses,
        # not the number of import rows or the first address in the cell.
        for item in row['got'].split(';'):
            if not item.strip():
                continue
            address = number(item.strip())
            if address in slots and slots[address] != row['nome']:
                raise ValueError(f'Ambiguous import GOT slot {rva(address)}')
            slots[address] = row['nome']
    return rows, slots


def direct_target(word: int, pc: int) -> int:
    displacement = word & 0x3ffffff
    if displacement & 0x2000000:
        displacement -= 0x4000000
    return (pc + displacement * 4) & 0xffffffffffffffff


def find_plt(text: bytes, got_imports: dict[int, str]):
    mapped, unknown, resolver_bodies = {}, [], []
    pattern_count = 0
    words = memoryview(text).cast('I')
    for index, first in enumerate(words):
        if first & 0x9f00001f != 0x90000010 or index + 3 >= len(words):
            continue
        load, add, branch = words[index + 1:index + 4]
        if (load & 0xffc003ff != 0xf9400211 or add & 0xffc003ff != 0x91000210
                or branch != 0xd61f0220):
            continue
        pattern_count += 1
        pc = index * 4
        immediate = ((first >> 5) & 0x7ffff) * 4 + ((first >> 29) & 3)
        if immediate & 0x100000:
            immediate -= 0x200000
        page = (pc & ~0xfff) + immediate * 4096
        got = page + ((load >> 10) & 0xfff) * 8
        add_address = page + ((add >> 10) & 0xfff)
        if got != add_address:
            raise ValueError(f'PLT load/add address disagreement at {rva(pc)}')
        name = got_imports.get(got)
        if name is not None:
            mapped[pc] = (got, name)
        elif index > 0 and words[index - 1] == 0xa9bf7bf0:
            # PLT0 has a leading stack save before its resolver body. It is
            # independently recognized, rather than dropped to force a count.
            resolver_bodies.append((pc, got))
        else:
            unknown.append((pc, got))
    return mapped, unknown, resolver_bodies, pattern_count


class SwitchRecognizer:
    """Conservative local data-flow recognizer, not a complete CFG recovery."""
    def __init__(self, text, functions):
        self.text = text
        self.functions = functions
        self.md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
        self.md.detail = True
        self.md.skipdata = True

    def key(self, register):
        if not register:
            return None
        name = self.md.reg_name(register)
        if name in ('fp', 'lr'):
            return 29 if name == 'fp' else 30
        if name in ('xzr', 'wzr'):
            return 32
        if name in ('sp', 'wsp'):
            return 31
        if name and name[0] in 'xw' and name[1:].isdigit():
            return int(name[1:])
        return None

    def width(self, register):
        return 32 if self.md.reg_name(register).startswith('w') else 64

    def inspect(self, pc, owner):
        lower = self.functions[owner][0] if owner is not None else 0
        begin = max(lower, pc - 64 * 4)
        instructions = list(self.md.disasm(self.text[begin:pc + 4], begin))
        if not instructions or instructions[-1].address != pc or instructions[-1].mnemonic != 'br':
            return None
        writes = []
        for ins in instructions:
            if not ins.id:
                writes.append(set())
            else:
                writes.append({self.key(reg) for reg in ins.regs_access()[1]} - {None})

        def barrier(ins):
            return not ins.id or ins.mnemonic in ('b', 'br', 'ret', 'bl', 'blr')

        def last_writer(reg, before):
            wanted = self.key(reg)
            for i in range(before - 1, -1, -1):
                if barrier(instructions[i]):
                    return None
                if wanted in writes[i]:
                    return i
            return None

        @functools.lru_cache(None)
        def constant(reg, before):
            if self.key(reg) == 32:
                return 0
            index = last_writer(reg, before)
            if index is None:
                return None
            ins = instructions[index]
            ops = ins.operands
            value = None
            if ins.mnemonic in ('adr', 'adrp') and len(ops) == 2 and ops[1].type == A.ARM64_OP_IMM:
                value = ops[1].imm
            elif ins.mnemonic in ('mov', 'movz', 'movn') and len(ops) == 2:
                if ops[1].type == A.ARM64_OP_IMM:
                    value = ops[1].imm << ops[1].shift.value
                    if ins.mnemonic == 'movn':
                        value = ~value
                elif ops[1].type == A.ARM64_OP_REG:
                    value = constant(ops[1].reg, index)
            elif ins.mnemonic in ('add', 'sub') and len(ops) == 3 and ops[1].type == A.ARM64_OP_REG:
                left = constant(ops[1].reg, index)
                right = (ops[2].imm if ops[2].type == A.ARM64_OP_IMM else
                         constant(ops[2].reg, index) if ops[2].type == A.ARM64_OP_REG else None)
                if left is not None and right is not None:
                    if ops[2].shift.type not in (0, A.ARM64_SFT_LSL):
                        return None
                    right <<= ops[2].shift.value
                    value = left + right if ins.mnemonic == 'add' else left - right
            if value is not None:
                value &= (1 << self.width(reg)) - 1
            return value

        def value_writer(reg, before):
            for _ in range(8):
                index = last_writer(reg, before)
                if index is None:
                    return None
                ins = instructions[index]
                if (ins.mnemonic == 'mov' and len(ins.operands) == 2
                        and ins.operands[1].type == A.ARM64_OP_REG):
                    reg, before = ins.operands[1].reg, index
                else:
                    return index
            return None

        def indexed_load(index):
            ins = instructions[index]
            if ins.mnemonic not in ('ldrb', 'ldrh', 'ldrsb', 'ldrsh', 'ldrsw', 'ldr'):
                return None
            ops = ins.operands
            if len(ops) != 2 or ops[0].type != A.ARM64_OP_REG or ops[1].type != A.ARM64_OP_MEM:
                return None
            mem = ops[1].mem
            if not mem.base or not mem.index:
                return None
            sizes = {'ldrb': 1, 'ldrsb': 1, 'ldrh': 2, 'ldrsh': 2, 'ldrsw': 4}
            size = sizes.get(ins.mnemonic, self.width(ops[0].reg) // 8)
            if self.key(ops[0].reg) is None:
                return None
            base = constant(mem.base, index)
            if base is not None:
                base += mem.disp
                if not 0 <= base < 0x100000000:
                    base = None
            return dict(load_index=index, base=base, size=size, index_register=mem.index)

        def same_index(reg, compare_reg, cmp_index, load_index):
            current = self.key(reg)
            for i in range(load_index - 1, cmp_index, -1):
                ins = instructions[i]
                if barrier(ins):
                    return False
                if current in writes[i]:
                    if ins.mnemonic == 'mov' and len(ins.operands) == 2 and ins.operands[1].type == A.ARM64_OP_REG:
                        current = self.key(ins.operands[1].reg)
                    else:
                        return False
            return current == self.key(compare_reg)

        def bound(load):
            stop = load['load_index']
            for i in range(stop - 1, -1, -1):
                ins = instructions[i]
                if barrier(ins):
                    break
                if ins.mnemonic not in ('b.hi', 'b.hs', 'b.cs'):
                    continue
                if not ins.operands or ins.operands[0].type != A.ARM64_OP_IMM:
                    continue
                target = ins.operands[0].imm
                if ins.address < target <= pc:
                    continue  # Not an out-of-range exit from this dispatch suffix.
                for j in range(i - 1, -1, -1):
                    cmp = instructions[j]
                    if barrier(cmp):
                        break
                    if cmp.update_flags:
                        ops = cmp.operands
                        if (cmp.mnemonic == 'cmp' and len(ops) == 2
                                and ops[0].type == A.ARM64_OP_REG and ops[1].type == A.ARM64_OP_IMM
                                and 0 <= ops[1].imm < 0x1000000
                                and same_index(load['index_register'], ops[0].reg, j, stop)):
                            count = ops[1].imm + (1 if ins.mnemonic == 'b.hi' else 0)
                            if count:
                                return count, cmp.address
                        break  # A nearer flags writer invalidates an older compare.
            return None, None

        end = len(instructions) - 1
        target_register = instructions[-1].operands[0].reg
        writer = value_writer(target_register, end)
        if writer is None:
            return None
        ins = instructions[writer]
        load = None
        branch_base = None
        scale = None
        if (ins.mnemonic == 'add' and len(ins.operands) == 3
                and ins.operands[1].type == A.ARM64_OP_REG and ins.operands[2].type == A.ARM64_OP_REG):
            ops = ins.operands
            load_writer = value_writer(ops[2].reg, writer)
            if load_writer is not None:
                load = indexed_load(load_writer)
            if load is None or load['size'] > 4 or ops[2].shift.type not in (0, A.ARM64_SFT_LSL):
                return None
            branch_base = constant(ops[1].reg, writer)
            scale = 1 << ops[2].shift.value
        else:
            # Absolute pointer tables require both a constant base and an index
            # guard; an indexed virtual-method load alone is not a switch.
            load = indexed_load(writer)
            if load is None or load['size'] != 8 or load['base'] is None:
                return None
        count, guard = bound(load)
        if branch_base is None and scale is None and count is None:
            return None
        return dict(base=load['base'], entries=count, entry_size=load['size'],
                    branch_base=branch_base, destination_scale=scale, guard=guard,
                    load_rva=instructions[load['load_index']].address)


def self_check(md):
    # Synthetic ISA checks, independent of executable contents.
    for pc, word, expected in [(0x100, 0x94000004, 0x110),
                               (0x100, 0x97fffffc, 0xf0),
                               (0x100, 0x17ffffff, 0xfc)]:
        decoded = list(md.disasm_lite(struct.pack('<I', word), pc))
        assert len(decoded) == 1 and number(decoded[0][3].lstrip('#')) == expected
        assert direct_target(word, pc) == expected


def run():
    if sys.byteorder != 'little':
        raise RuntimeError('This raw-word scanner requires a little-endian host')
    text = (ROOT / 'analysis' / 'boot' / 'text.bin').read_bytes()
    if len(text) % 4:
        raise ValueError('Unaligned text input')
    functions = read_functions(len(text))
    starts = [start for start, _ in functions]
    ends = [start + size for start, size in functions]
    entries = set(starts)
    import_rows, slots = read_imports()
    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
    md.skipdata = True
    self_check(md)
    plt, unmapped_plt, resolvers, patterns = find_plt(text, slots)
    plt_br = {pc + 12 for pc in plt} | {pc + 12 for pc, _ in unmapped_plt + resolvers}
    csv_write('plt_map.csv', ('stub_rva', 'got_rva', 'import'),
              ((rva(pc), rva(got), name) for pc, (got, name) in sorted(plt.items())))
    csv_write('plt_unmapped.csv', ('stub_rva', 'got_rva'),
              ((rva(pc), rva(got)) for pc, got in unmapped_plt))

    counts = collections.Counter()
    examples = {}
    kinds = collections.Counter()
    sites = []
    per_function = [[0, 0, 0, 0, 0] for _ in functions]  # BR, BLR, switch, PLT, other
    owner = 0
    words_decoded = invalid = 0
    progress = [f'Capstone {capstone.__version__}; funcoes={len(functions)}; iniciando censo']
    print(progress[-1], flush=True)
    # Bounded batches prevent Capstone from allocating a whole-image instruction array.
    for begin in range(0, len(text), 0x10000):
        chunk = text[begin:begin + 0x10000]
        consumed = 0
        for pc, size, mnemonic, operands in md.disasm_lite(chunk, begin):
            if size != 4 or pc != begin + consumed:
                raise ValueError(f'Unexpected instruction coverage at {rva(pc)}')
            consumed += size
            words_decoded += 1
            invalid += mnemonic == '.byte'
            while owner < len(functions) and pc >= ends[owner]:
                owner += 1
            own = owner if owner < len(functions) and pc >= starts[owner] else None
            if mnemonic in ('bl', 'b') and own is not None:
                word = struct.unpack_from('<I', text, pc)[0]
                target = direct_target(word, pc)
                if mnemonic == 'bl' or not starts[own] <= target < ends[own]:
                    kinds['bl' if mnemonic == 'bl' else 'tail'] += 1
                    counts[target] += 1
                    examples.setdefault(target, pc)
            if mnemonic in ('br', 'blr'):
                sites.append((pc, mnemonic, own))
                kinds[mnemonic] += 1
                if own is not None:
                    per_function[own][0 if mnemonic == 'br' else 1] += 1
        if consumed != len(chunk):
            raise ValueError(f'Capstone did not cover the batch at {rva(begin)}')
    assert words_decoded * 4 == len(text)

    def classify(target):
        if target in entries:
            return 'inicio_funcao'
        if target in plt or any(target == pc for pc, _ in unmapped_plt):
            return 'stub_plt'
        index = bisect.bisect_right(starts, target) - 1
        return 'meio_funcao' if index >= 0 and target < ends[index] else 'lacuna'

    classes = collections.Counter(classify(target) for target in counts)
    csv_write('call_targets.csv', ('alvo_rva', 'classe', 'quantidade_de_chamadas', 'exemplo_de_origem_rva'),
              ((rva(target), classify(target), counts[target], rva(examples[target])) for target in sorted(counts)))
    assert sum(counts.values()) == kinds['bl'] + kinds['tail']
    progress.append(f'Censo direto concluido; BR={kinds["br"]}; BLR={kinds["blr"]}; analisando tabelas')
    print(progress[-1], flush=True)

    recognizer = SwitchRecognizer(text, functions)
    branch_classes = collections.Counter()
    jump_tables, details = [], []
    for pc, mnemonic, own in sites:
        table = None
        category = ''
        if mnemonic == 'br':
            if pc in plt_br:
                category = 'x17_plt'
            else:
                table = recognizer.inspect(pc, own)
                category = 'tabela_de_salto' if table is not None else 'outro'
            branch_classes[category] += 1
            if own is not None:
                per_function[own][{'tabela_de_salto': 2, 'x17_plt': 3, 'outro': 4}[category]] += 1
            if table is not None:
                jump_tables.append((rva(pc), rva(table['base']), table['entries'] or '', table['entry_size']))
        details.append((rva(pc), mnemonic, rva(starts[own]) if own is not None else '', category,
                        rva(table['load_rva']) if table else '', rva(table['branch_base']) if table else '',
                        table['destination_scale'] if table and table['destination_scale'] else '',
                        rva(table['guard']) if table else ''))
    csv_write('jump_tables.csv', ('br_rva', 'base_rva', 'entradas', 'tamanho_entrada'), jump_tables)
    csv_write('indirect_branches.csv', ('origem_rva', 'mnemonico', 'funcao_rva', 'classe', 'carga_rva',
                                       'base_destinos_rva', 'escala_destinos', 'comparacao_rva'), details)
    csv_write('indirect_by_function.csv', ('funcao_rva', 'tamanho', 'br', 'blr', 'tabela_de_salto', 'x17_plt', 'outro'),
              ((rva(start), size, *per_function[i]) for i, (start, size) in enumerate(functions)))
    assert sum(branch_classes.values()) == kinds['br']
    assert sum(row[0] for row in per_function) + sum(m == 'br' and o is None for _, m, o in sites) == kinds['br']
    assert sum(row[1] for row in per_function) + sum(m == 'blr' and o is None for _, m, o in sites) == kinds['blr']

    base_known = sum(bool(row[1]) for row in jump_tables)
    count_known = sum(bool(row[2]) for row in jump_tables)
    complete = sum(bool(row[1]) and bool(row[2]) for row in jump_tables)
    gaps = sorted((target for target in counts if classify(target) == 'lacuna'), key=lambda t: (-counts[t], t))[:20]
    coverage = sum(size for _, size in functions)
    output = [f'Funcoes: {len(functions)}; bytes cobertos: {coverage}/{len(text)}',
              f'PLT: {len(plt)} mapeados; {len(unmapped_plt)} stubs sem import; {len(resolvers)} corpo PLT0 separado',
              f'BL: {kinds["bl"]}; tail calls B: {kinds["tail"]}; alvos distintos: {len(counts)}',
              'Alvos: ' + '; '.join(f'{name}={classes[name]}' for name in CLASSES),
              f'BR: {kinds["br"]}; BLR: {kinds["blr"]}',
              'BR classes: ' + '; '.join(f'{name}={branch_classes[name]}' for name in ('tabela_de_salto','x17_plt','outro')),
              f'Tabelas: {len(jump_tables)}; base={base_known}; entradas={count_known}; ambas={complete}',
              'Autoverificacoes: OK',
              'Saidas: analysis/recomp/generated/*.csv e analysis/recomp/control_flow_report.md']
    invariant = len(plt) == EXPECTED_JUMP_SLOTS and not unmapped_plt
    output.append(f'Invariante PLT 917: {"OK" if invariant else "DIVERGENTE (ver relatorio)"}')
    plt_range = f'{rva(min(plt))} a {rva(max(plt))}' if plt else 'vazia'
    resolver_text = '; '.join(f'corpo {rva(pc)}, GOT {rva(got)}' for pc, got in resolvers) or 'nenhum'
    md_lines = [
        '# Censo de fluxo de controle AArch64', '',
        'Análise estática para R3/R4. Endereços são RVAs; nenhuma execução do jogo.', '',
        '## Entradas e cobertura', '',
        f'- Funções do `.eh_frame`: **{len(functions):,}**; intervalos alinhados, positivos, sem sobreposição.',
        f'- `.text`: **{len(text):,} bytes**, **{words_decoded:,}** palavras decodificadas em passos de quatro bytes.',
        f'- Cobertura dos intervalos: **{coverage:,} bytes ({coverage/len(text):.4%})**.',
        f'- Importações: **{len(import_rows):,}** linhas; todos os slots separados por ponto e vírgula são considerados.',
        f'- Disassembler: **Capstone {capstone.__version__}**, venv isolado; **{invalid}** palavras tratadas como dados não decodificados.', '',
        '## Parte 1 — PLT', '',
        f'- Padrões completos encontrados: **{patterns}**; stubs comuns mapeados: **{len(plt)}**.',
        f'- Faixa dos inícios de stubs mapeados: **{plt_range}**; tamanho por stub: **16 bytes**.',
        f'- Stubs comuns sem importação: **{len(unmapped_plt)}**; ver `generated/plt_unmapped.csv`.',
        f'- Resolver PLT0 separado: **{len(resolvers)}** ({resolver_text}).',
        '- O corpo de PLT0 também contém o padrão de quatro mnemônicos, mas é precedido pelo salvamento de pilha do resolver.',
        '- A exclusão de PLT0 exige esse prefixo específico e ausência de importação para seu GOT; não ajusta a contagem esperada.',
        f'- Invariante de 917 JUMP_SLOT: **{"OK" if invariant else "DIVERGENTE"}**. A contagem obtida é {len(plt)}, diferença {len(plt)-EXPECTED_JUMP_SLOTS:+d}.',
        '- `generated/plt_map.csv` contém somente stubs de importação; o `BR` do resolver também entra na classe `x17_plt`.', '',
        '## Parte 2 — chamadas e tail calls', '',
        f'- `BL` dentro dos intervalos conhecidos: **{kinds["bl"]:,}**.',
        f'- `B` que sai do próprio intervalo: **{kinds["tail"]:,}**.',
        f'- Alvos distintos combinados: **{len(counts):,}**.', '',
        '| Classe do alvo | Alvos distintos |', '|---|---:|',
        *[f'| {name} | {classes[name]:,} |' for name in CLASSES], '',
        'A classificação prioriza início de função, depois stub PLT, interior de intervalo e lacuna. '
        'Um salto entre dois intervalos do `.eh_frame` é contado como tail call operacionalmente; '
        'pode ser uma divisão de função, não necessariamente uma chamada de função em nível de linguagem.', '',
        f'Os {classes["stub_plt"]} alvos PLT desta parte são somente os referenciados por chamadas diretas '
        f'com origem nos intervalos conhecidos; isso não altera os {len(plt)} stubs mapeados na Parte 1.', '',
        '### Até vinte lacunas com mais referências', '',
        f'Existem {classes["lacuna"]} alvos distintos nesta classe; a tabela lista {len(gaps)}.', '',
        '| Alvo RVA | BL + tail calls |', '|---|---:|',
        *[f'| {rva(target)} | {counts[target]} |' for target in gaps], '',
        '## Parte 3 — indiretos', '',
        f'- `.text` inteiro: **{kinds["br"]:,} BR** e **{kinds["blr"]:,} BLR**; `RET` excluído.',
        f'- Fora dos intervalos conhecidos: **{sum(m=="br" and o is None for _,m,o in sites):,} BR**, '
        f'**{sum(m=="blr" and o is None for _,m,o in sites):,} BLR**.', '',
        '| Classe de BR | Sites |', '|---|---:|',
        *[f'| {name} | {branch_classes[name]:,} |' for name in ('tabela_de_salto','x17_plt','outro')], '',
        f'- Tabelas candidatas: **{len(jump_tables):,}**; base resolvida **{base_known:,}**; '
        f'número de entradas resolvido **{count_known:,}**; ambos **{complete:,}**.',
        '- A base em `jump_tables.csv` é a base dos dados lidos pela carga indexada, não a base de destinos relativos.',
        '- Célula vazia significa não resolvido; não é substituída por zero.',
        '- `indirect_by_function.csv` inclui todas as funções, mesmo com contagem zero. '
        '`indirect_branches.csv` permite conferir os sites e os casos fora de funções.', '',
        '### Método e limites das tabelas', '',
        'Reconhecimento local conservador em até 64 instruções anteriores, limitado ao início da função. '
        'Exige fluxo da carga indexada para o registrador de `BR`; offsets relativos de 1, 2 ou 4 bytes '
        'alimentam `ADD`. Tabelas de ponteiros de 8 bytes exigem base constante e guarda de índice. '
        'Uma carga de método virtual isolada não é classificada como switch.', '',
        'Bases constantes são propagadas por `ADR`, `ADRP`, `MOV`, `ADD` e `SUB` suportados. '
        'O número de entradas vem de `CMP` seguido de saída unsigned por `B.HI` ou `B.HS`, '
        'com conferência do registrador de índice e cópias por `MOV`. Normalizações diferentes, '
        'valores carregados de memória, definições distantes e guardas de outro formato podem ficar não resolvidos. '
        'A quantidade recuperada é o limite de índices permitido pela guarda, incluindo possíveis casos default.', '',
        'O censo linear inclui PLT e lacunas. Palavras de dados alinhadas dentro de `.text` podem ter '
        'decodificação válida como instruções; as contagens não demonstram execução nem recuperam todo o CFG. '
        'A classe de switch é estrutural e não deve ser tratada como uma lista exaustiva ou como prova de destinos.', '',
        '## Aceite', '',
        'Comando do censo e saída:', '', '```text',
        r'build\venv\Scripts\python.exe -I analysis\recomp\control_flow_census.py', *progress, *output, '```', '',
    ]
    # Write the actual report before running the repository-wide check. The final
    # separate acceptance command checks it again after its result is included.
    REPORT.write_text('\n'.join(md_lines), encoding='utf-8', newline='\n')
    safety = subprocess.run([sys.executable, '-I', str(ROOT/'tools/check_repo_safety.py')],
                            cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    safety_output = (safety.stdout + safety.stderr).strip()
    md_lines += ['Comando de segurança e saída observada:', '', '```text',
                 'python tools\\check_repo_safety.py', safety_output, '```', '',
                 f'Código de saída do verificador: **{safety.returncode}**.', '',
                 'Nenhum commit ou push foi realizado. Os CSVs ficam somente em `generated/`.', '']
    REPORT.write_text('\n'.join(md_lines), encoding='utf-8', newline='\n')
    for line in output:
        print(line)
    if safety.returncode:
        print(safety_output, file=sys.stderr)
    return safety.returncode


if __name__ == '__main__':
    raise SystemExit(run())
