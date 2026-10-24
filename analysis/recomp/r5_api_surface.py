#!/usr/bin/env python3
"""R5 API surface and bounded literal FS arguments, from offline inventories.

Writes only this task's report and ignored generated CSVs. Does not modify or
run prior census scripts, translate code, execute the target, commit, or push.
"""
from __future__ import annotations

import bisect
import collections
import csv
import functools
from pathlib import Path
import re
import struct
import subprocess
import sys

sys.dont_write_bytecode = True
import capstone
from capstone import arm64_const as A

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
OUT = HERE / 'generated'
REPORT = HERE / 'r5_api_surface_report.md'
AREAS = ('nn::fs', 'nn::hid', 'nn::oe + nn::applet', 'nn::account', 'nn::time',
         'nn::settings', 'nn::err', 'nn::nifm', 'nn::aoc', 'nn::pl', 'nn::vi', 'nn::audio', 'nn::os')
# Public service-name facts, not claims that every matching string is an IPC
# request. Other strings from the kernel census remain separate candidates.
KNOWN_SERVICES = set('''
acc:su acc:u0 acc:u1 apm appletAE appletOE aud:a aud:d audctl audin:u audout:a
audout:d audout:u audrec:u audren:u avm bt btdrv btm btm:sys btm:u capmtp caps:a
caps:c caps:dc caps:sc caps:su caps:u clkrst csrng ectx:r fan friend:a friend:m
friend:s friend:u friend:v fsp-ldr fsp-pr fsp-srv gpio grc:d hid hid:dbg hid:sys
hidbus hwopus i2c idle:sys ins:r ins:s irs lbl ldn:m ldn:s ldn:u ldr: ldr:ro lr
mii:e mii:u miiimg mm:u ncm nfc:mf:u nfc:sys nfc:user nfp:dbg nfp:sys nfp:user
nifm:a nifm:s nifm:u nim notif:a notif:s ns:am ns:dev ns:su ns:vm nvdrv nvdrv:a
nvdrv:s nvdrv:t omm ovln:rcv ovln:snd pctl pctl:a pctl:r pctl:s pcv pdm:qry pgl
pl:s pl:u pm: psc:m psm ro:1 ro:dmnt set set:cal set:fd set:sys sm:m spl: spsm
tc time:a time:r time:s time:su time:u ts uart usb:ds usb:hs vi:m vi:s vi:u wlan:inf
sm: bsd:u ssl aoc:u prepo:u bcat:u
'''.split())
REV = 'b156b4774fb2df7b93402169509266da7547fc4c'
PUBLIC_HID = f'https://github.com/switchbrew/libnx/blob/{REV}/nx/source/services/hid.c'
PUBLIC_FS = f'https://github.com/switchbrew/libnx/blob/{REV}/nx/source/services/fs.c'


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


def tokens(value):
    return set(filter(None, value.split(';')))


def area_of(row):
    namespace = row['namespace']
    if namespace in ('nn::oe', 'nn::applet'): return 'nn::oe + nn::applet'
    return namespace if namespace in AREAS else None


def branch_target(word, pc):
    offset = word & 0x3ffffff
    if offset & 0x2000000: offset -= 0x4000000
    return pc + 4 * offset


def signed(value, bits):
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def path_prefix(value):
    slashes = [i for i, char in enumerate(value) if char == '/']
    return value[:slashes[1] + 1] if len(slashes) >= 2 else value


def fs_argument(demangled):
    if not demangled.startswith('nn::fs::'): return None
    method = demangled[len('nn::fs::'):].split('(', 1)[0]
    if method.startswith('Mount') or method.startswith('Commit') or method in ('CreateFile', 'DeleteFile'):
        # All observed overloads in these families have const char* first.
        if not demangled.split('(', 1)[1].startswith('char const*'):
            raise ValueError('FS first-argument signature needs explicit handling')
        return 0, method, 'montagem' if method.startswith(('Mount', 'Commit')) else 'caminho'
    if method in ('OpenFile', 'OpenDirectory', 'GetEntryType'):
        if ', char const*' not in demangled: raise ValueError('FS path signature mismatch')
        return 1, method, 'caminho'
    return None


class Functions:
    def __init__(self, text_start, text_end):
        self.rows = sorted((number(r['rva']), number(r['tamanho']), source)
                           for name, source in (('functions.csv', 'fde'), ('gap_functions.csv', 'lacuna'))
                           for r in read_csv(name))
        self.starts = [s for s, n, source in self.rows]
        end = text_start
        for start, size, source in self.rows:
            if start < end or start % 4 or size <= 0 or size % 4 or start + size > text_end:
                raise ValueError(f'Invalid function interval {hx(start)}')
            end = start + size

    def owner(self, pc):
        i = bisect.bisect_right(self.starts, pc) - 1
        if i >= 0 and pc < self.starts[i] + self.rows[i][1]: return self.rows[i]
        return None


class StringArgument:
    """16-instruction register slice; unknown definitions and joins are fences."""
    def __init__(self, image, rodata, call, function_start, labels):
        self.image, self.rodata, self.call = image, rodata, call
        self.lower = max(function_start, call - 16 * 4)
        self.md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
        self.md.detail = True
        decoded = list(self.md.disasm(image[self.lower:call], self.lower))
        self.ins = {i.address: i for i in decoded}
        self.complete = len(decoded) * 4 == call - self.lower
        self.labels = labels
        self.reason = 'sem_definicao_local'

    def key(self, register):
        name = self.md.reg_name(register)
        if name in ('fp', 'lr'): return 29 if name == 'fp' else 30
        if name and name[0] in 'xw' and name[1:].isdigit(): return int(name[1:])
        return None

    def is64(self, register):
        return self.md.reg_name(register).startswith('x') or self.md.reg_name(register) in ('fp', 'lr')

    @functools.lru_cache(maxsize=None)
    def resolve(self, key, before):
        if before in self.labels:
            self.reason = 'join_direto'
            return None
        for pc in range(before - 4, self.lower - 1, -4):
            ins = self.ins.get(pc)
            if ins is None:
                self.reason = 'instrucao_invalida'
                return None
            if any(g in ins.groups for g in (capstone.CS_GRP_JUMP, capstone.CS_GRP_CALL,
                                             capstone.CS_GRP_RET, capstone.CS_GRP_INT)):
                self.reason = 'barreira_controle'
                return None
            writes = {self.key(r) for r in ins.regs_access()[1]}
            if key in writes:
                ops = ins.operands
                if not ops or ops[0].type != A.ARM64_OP_REG or not self.is64(ops[0].reg):
                    self.reason = 'sobrescrita_sem_ponteiro_64'
                    return None
                if ins.mnemonic in ('adr', 'adrp') and len(ops) == 2 and ops[1].type == A.ARM64_OP_IMM:
                    return ops[1].imm, ins.mnemonic, (pc,)
                if (ins.mnemonic == 'mov' and len(ops) == 2 and ops[1].type == A.ARM64_OP_REG
                        and self.is64(ops[1].reg)):
                    value = self.resolve(self.key(ops[1].reg), pc)
                    return (value[0], value[1], value[2] + (pc,)) if value else None
                if (ins.mnemonic == 'add' and len(ops) == 3 and ops[1].type == A.ARM64_OP_REG
                        and self.is64(ops[1].reg) and ops[2].type == A.ARM64_OP_IMM
                        and ops[2].shift.type in (0, A.ARM64_SFT_LSL)):
                    value = self.resolve(self.key(ops[1].reg), pc)
                    if value:
                        method = 'ADRP+ADD' if value[1] == 'adrp' else value[1]
                        return value[0] + (ops[2].imm << ops[2].shift.value), method, value[2] + (pc,)
                self.reason = f'definicao_{ins.mnemonic}_nao_suportada'
                return None
            if pc in self.labels:
                self.reason = 'join_direto'
                return None
        self.reason = 'sem_definicao_na_janela_16'
        return None

    def recover(self, argument):
        if not self.complete: return None, 'instrucao_invalida_na_janela'
        value = self.resolve(argument, self.call)
        if value is None: return None, self.reason
        address, method, origins = value
        if method == 'adrp': return None, 'ADRP_sem_ADD'
        start, size = self.rodata
        if not start <= address < start + size: return None, 'endereco_fora_rodata'
        end = self.image.find(b'\0', address, min(address + 4097, start + size))
        if end < 0: return None, 'sem_NUL_em_4096_bytes'
        raw = self.image[address:end]
        if not raw or any(c < 32 or c > 126 for c in raw): return None, 'nao_ASCII_imprimivel_ou_vazia'
        return (address, raw.decode('ascii'), method, origins), ''


def recommendation(area, api_count):
    # Explicit planning heuristics; these are not runtime implementation choices.
    if area == 'nn::os':
        return 'ipc', 'referencia de volume: manter o SDK e implementar a fronteira SVC para preservar objetos e sincronizacao; aqui ipc designa manter o SDK, nao um protocolo de servico nn::os'
    if area == 'nn::hid':
        return 'ipc', f'{api_count} APIs observadas: preservar polling e estado do SDK usando servico HID e memoria compartilhada reduz a superficie de overrides'
    criteria = {
        'nn::fs': 'backend de arquivos nativo e montagem por nome oferecem uma fronteira menor que reproduzir sessoes, dominios e objetos do protocolo FS',
        'nn::oe + nn::applet': 'superficie de ciclo de vida delimitada permite mapear o estado de aplicacao e janela do host sem reproduzir o protocolo completo de applet',
        'nn::account': 'centralizar usuarios e handles em uma implementacao nativa das APIs observadas limita o protocolo de conta necessario para R5',
        'nn::time': 'mapear relogios e conversoes observados para o host evita construir a infraestrutura de servico de horario nesta etapa',
        'nn::settings': 'poucas APIs observadas de configuracao favorecem overrides de valores e formatos documentados pelos argumentos',
        'nn::err': 'mapear construcao e apresentacao dos erros observados para o host evita depender de UI de applet para diagnosticos',
        'nn::nifm': 'superficie pequena permite modelar o estado de rede do host por API antes de implementar sessoes IPC de rede',
        'nn::aoc': 'superficie pequena permite consultar o catalogo de conteudo instalado por API; ausencia de servico no grafo nao e prova de ausencia de IPC',
        'nn::pl': 'poucas APIs de fontes permitem fornecer buffers nativos com ABI e vida util verificadas sem reproduzir o servico completo',
        'nn::vi': 'poucas APIs de display podem compartilhar janela e handles com o backend grafico nativo',
        'nn::audio': 'mapear as APIs observadas para o backend de audio nativo evita reproduzir o protocolo do renderer; requer cobrir callbacks e estado alem das importacoes diretas',
    }
    return 'api', f'{api_count} APIs observadas: ' + criteria[area]


def self_check():
    assert branch_target(0x97ffffff, 0x100) == 0xfc
    assert path_prefix('rom:/data/a/b') == 'rom:/data/'
    assert path_prefix('save:/file') == 'save:/file'
    assert fs_argument('nn::fs::OpenFile(nn::fs::FileHandle*, char const*, int)')[0] == 1
    # Synthetic ISA: ADR X0 to 0x20; then optional MOV W0,#0 clobbers the pointer.
    image = bytearray(128)
    struct.pack_into('<I', image, 0, 0x10000100)
    image[32:37] = b'rom:\0'
    one = StringArgument(bytes(image), (32, 64), 4, 0, set())
    assert one.recover(0)[0][1] == 'rom:'
    one.resolve.cache_clear()
    struct.pack_into('<I', image, 4, 0x52800000)
    two = StringArgument(bytes(image), (32, 64), 8, 0, set())
    assert two.recover(0)[0] is None
    two.resolve.cache_clear()
    join = StringArgument(bytes(image), (32, 64), 4, 0, {4})
    assert join.recover(0)[0] is None
    join.resolve.cache_clear()
    # Synthetic ADRP X8 / ADD X8,X8,#32 / MOV X0,X8: the page register need
    # not be the argument register, but its definition must remain intact.
    struct.pack_into('<III', image, 0, 0x90000008, 0x91008108, 0xaa0803e0)
    page = StringArgument(bytes(image), (32, 64), 12, 0, set())
    assert page.recover(0)[0][:3] == (32, 'rom:', 'ADRP+ADD')
    page.resolve.cache_clear()
    struct.pack_into('<I', image, 4, 0x52800008)  # A W8 write destroys X8.
    clobber = StringArgument(bytes(image), (32, 64), 12, 0, set())
    assert clobber.recover(0)[0] is None
    clobber.resolve.cache_clear()


def run():
    self_check()
    output = []
    def emit(line):
        output.append(line)
        print(line, flush=True)
    imports = read_csv('imports.csv')
    selected = {r['nome']: r for r in imports if r['tipo'] == 'func' and area_of(r)}
    plt_rows = read_csv('plt_map.csv')
    stubs = {number(r['stub_rva']): r['import'] for r in plt_rows if r['import'] in selected}
    imports_to_stubs = collections.defaultdict(set)
    for pc, name in stubs.items(): imports_to_stubs[name].add(pc)
    target_counts = {number(r['alvo_rva']): number(r['quantidade_de_chamadas']) for r in read_csv('call_targets.csv')}
    counts = {name: sum(target_counts.get(pc, 0) for pc in imports_to_stubs[name]) for name in selected}
    reach = {r['importacao']: r for r in read_csv('import_kernel_reach.csv')}
    sdk_exports = {r['nome'] for r in read_csv('sdk/exports.csv') if r['tipo'] == 'func'}
    service_names = {r['nome'] for r in read_csv('service_names.csv')}
    svc_names = {r['svc']: r['nome_svc'] for r in read_csv('svc_sites.csv')}
    layout = {r['nome']: (number(r['rva']), number(r['tamanho'])) for r in read_csv('layout.csv')}
    image = (OUT / 'image.bin').read_bytes()
    text_start, text_size = layout['.text']
    text_end = text_start + text_size
    if text_end > len(image): raise ValueError('Truncated main image')
    functions = Functions(text_start, text_end)
    emit(f'Capstone {capstone.__version__}; areas={len(AREAS)}; importacoes de funcao selecionadas={len(selected)}')
    sites, labels = [], set()
    for pc in range(text_start, text_end, 4):
        word = struct.unpack_from('<I', image, pc)[0]
        if word & 0x7c000000 == 0x14000000:
            target = branch_target(word, pc)
            if not word & 0x80000000 and text_start <= target < text_end: labels.add(target)
            if target in stubs:
                owner = functions.owner(pc)
                if owner is not None and (word & 0x80000000 or not owner[0] <= target < owner[0] + owner[1]):
                    sites.append((pc, stubs[target], owner[0], owner[2], 'BL' if word & 0x80000000 else 'B'))
                elif owner is None:
                    sites.append((pc, stubs[target], None, 'sem_funcao', 'BL' if word & 0x80000000 else 'B'))
        elif word & 0xff000010 == 0x54000000 or word & 0x7e000000 == 0x34000000:
            labels.add(pc + signed((word >> 5) & 0x7ffff, 19) * 4)
        elif word & 0x7e000000 == 0x36000000:
            labels.add(pc + signed((word >> 5) & 0x3fff, 14) * 4)
    by_import = collections.defaultdict(list)
    for site in sites: by_import[site[1]].append(site)
    imports_rows, area_rows = [], []
    for name, row in sorted(selected.items(), key=lambda item: (AREAS.index(area_of(item[1])), -counts[item[0]], item[1]['demangled'])):
        current = by_import[name]
        raw_services = tokens(reach.get(name, {}).get('servicos', ''))
        raw_svcs = tokens(reach.get(name, {}).get('svcs', ''))
        if raw_services - service_names or raw_svcs - svc_names.keys():
            raise ValueError('Kernel reach references missing service/SVC inventory entries')
        known = raw_services & KNOWN_SERVICES
        candidates = raw_services - KNOWN_SERVICES
        imports_rows.append((area_of(row), name, row['demangled'], counts[name],
                             len({s[2] for s in current if s[2] is not None}), len(current),
                             sum(s[3] == 'lacuna' for s in current),
                             ';'.join(hx(s) for s in sorted(imports_to_stubs[name])),
                             'com_stub' if imports_to_stubs[name] else 'sem_stub',
                             ';'.join(sorted(raw_services)), ';'.join(sorted(known)), ';'.join(sorted(candidates)),
                             reach.get(name, {}).get('svcs', ''), 'disponivel' if name in reach else 'sem_alcance',
                             'funcao_sdk' if name in sdk_exports else 'sem_exportacao_sdk'))
    for area in AREAS:
        names = {name for name, row in selected.items() if area_of(row) == area}
        current = [site for name in names for site in by_import[name]]
        used = sum(bool(by_import[name]) for name in names)
        known = {v for name in names for v in tokens(reach.get(name, {}).get('servicos', '')) if v in KNOWN_SERVICES}
        svcs = {v for name in names for v in tokens(reach.get(name, {}).get('svcs', ''))}
        choice, criterion = recommendation(area, used)
        area_rows.append((area, sum(counts[name] for name in names), len(current), len(names), used,
                          len({s[2] for s in current if s[2] is not None}), ';'.join(sorted(known)),
                          ';'.join(sorted(svcs, key=number)), choice, criterion))
    fs_rows, fs_audit = [], []
    for pc, name, owner, origin, mnemonic in sites:
        info = fs_argument(selected[name]['demangled'])
        if info is None: continue
        argument, method, category = info
        value, reason = None, 'chamada_sem_funcao'
        if owner is not None:
            scanner = StringArgument(image, layout['.rodata'], pc, owner, labels)
            value, reason = scanner.recover(argument)
            scanner.resolve.cache_clear()
        if value:
            address, string, construction, sources = value
            fs_rows.append((hx(pc), name, f'x{argument}', string))
            fs_audit.append((hx(pc), name, hx(owner), origin, method, category, f'x{argument}',
                             'recuperada', '', hx(address), construction, ';'.join(hx(s) for s in sources), string))
        else:
            fs_audit.append((hx(pc), name, hx(owner), origin, method, category, f'x{argument}',
                             'sem_string', reason, '', '', '', ''))
    write_csv('r5_imports.csv', ('area', 'importacao', 'demangled', 'chamadas_diretas', 'funcoes_main_distintas',
              'chamadas_imagem', 'chamadas_lacunas', 'stubs_rva', 'status_stub', 'servicos', 'servicos_catalogo',
              'candidatos_formato', 'svcs', 'status_alcance', 'status_sdk'), imports_rows)
    write_csv('r5_callers.csv', ('chamada_rva', 'importacao', 'funcao_main_rva', 'origem_funcao', 'mnemonico'),
              ((hx(pc), name, hx(owner), origin, mnemonic) for pc, name, owner, origin, mnemonic in sites))
    write_csv('r5_fs_strings.csv', ('chamada_rva', 'importacao', 'registrador', 'string'), fs_rows)
    write_csv('r5_fs_string_audit.csv', ('chamada_rva', 'importacao', 'funcao_rva', 'origem_funcao', 'metodo', 'categoria',
              'registrador', 'status', 'motivo', 'string_rva', 'construcao', 'origens_rva', 'string'), fs_audit)
    write_csv('r5_area_recommendations.csv', ('area', 'chamadas_censo', 'chamadas_imagem', 'apis_importadas',
              'apis_observadas', 'funcoes_main_distintas', 'servicos_catalogo', 'svcs', 'recomendacao', 'criterio'), area_rows)
    mounts = collections.Counter(r[12] for r in fs_audit if r[7] == 'recuperada' and r[5] == 'montagem')
    prefixes = collections.Counter(path_prefix(r[12]) for r in fs_audit if r[7] == 'recuperada'
                                   and r[5] == 'caminho' and ('/' in r[12] or ':' in r[12]))
    write_csv('r5_fs_prefixes.csv', ('tipo', 'prefixo', 'chamadas'),
              [('montagem', s, n) for s, n in mounts.most_common()] + [('caminho', s, n) for s, n in prefixes.most_common()])
    check1 = all(name in selected and (imports_to_stubs[name] or r[8] == 'sem_stub') for r in imports_rows for name in [r[1]])
    fde_counts = collections.Counter(name for pc, name, owner, origin, mnemonic in sites if origin == 'fde')
    check2 = (all(fde_counts[name] == counts[name] for name in selected)
              and sum(r[1] for r in area_rows) == sum(counts.values())
              and sum(r[3] for r in imports_rows) == sum(counts.values()))
    ro_start, ro_size = layout['.rodata']
    check3 = all(ro_start <= number(r[9]) and number(r[9]) + len(r[12]) < ro_start + ro_size
                 and image[number(r[9]):number(r[9]) + len(r[12]) + 1] == r[12].encode('ascii') + b'\0'
                 for r in fs_audit if r[7] == 'recuperada')
    checks = [check1, check2, check3]
    extra = [s for s in sites if s[3] != 'fde']
    missing = len(fs_audit) - len(fs_rows)
    missing_reach = [r[2] for r in imports_rows if r[13] != 'disponivel']
    lines = [f'Importacoes: {len(imports_rows)}; sem stub: {sum(r[8] == "sem_stub" for r in imports_rows)}; sem alcance: {len(missing_reach)}',
             f'Chamadas do censo: {sum(counts.values())}; sites na imagem: {len(sites)}; adicionais fora dos FDEs: {len(extra)}',
             f'FS selecionadas: {len(fs_audit)} chamadas; strings recuperadas: {len(fs_rows)}; sem string: {missing}',
             f'Montagens literais distintas: {len(mounts)}; prefixos de caminho distintos: {len(prefixes)}']
    lines += [f'AUTOVERIFICACAO {i}: {"OK" if ok else "FALHOU"}' for i, ok in enumerate(checks, 1)]
    for line in lines: emit(line)
    safety = subprocess.run([sys.executable, '-I', str(ROOT / 'tools/check_repo_safety.py')], cwd=ROOT,
                            capture_output=True, text=True, encoding='utf-8', errors='replace')
    report = ['# Superfície de API para planejamento da R5', '',
              'Análise estática de importações e callsites; recomendações são insumos para decisão do Claude Code. '
              'Nenhum runtime ou inventário anterior foi alterado. O SDK continua sendo recompilado conforme o manifesto.', '',
              '## Resultado e critérios de contagem', '', *[f'- {line}' for line in lines[:4]], '',
              '- `Chamadas` nas tabelas é exatamente a soma em `call_targets.csv` para os stubs da importação. '
              'O censo original só contava funções FDE. `Imagem` inclui também as funções propostas em `gap_functions.csv`; '
              'esses totais permanecem separados. Funções chamadoras distintas usam ambos os inventários.',
              '- API observada significa uma importação com pelo menos um callsite na imagem; API importada pode ter zero '
              'chamadas diretas e ainda ser usada por ponteiro. Chamadores de uma área são uma união, não a soma dos chamadores de cada API.',
              '- Alcance de serviços/SVCs é herdado de `import_kernel_reach.csv`, limitado ao grafo direto do SDK até profundidade 12. '
              'Não prova execução nem pureza; chamadas indiretas, callbacks e caminhos dependentes de argumentos não estão completos.',
              '- As tabelas mostram nomes do catálogo público de serviços; os campos `servicos` e `candidatos_formato` do CSV '
              'preservam também as strings heurísticas do censo. Uma string de formato compatível não é automaticamente serviço IPC. '
              'Todos os nomes e números de alcance foram confrontados com `service_names.csv` e `svc_sites.csv`.',
              '- Referências públicas usadas como apoio ao critério: [cliente FS libnx](' + PUBLIC_FS + ') usa sessões/domínios; '
              '[cliente HID libnx](' + PUBLIC_HID + ') combina serviço e memória compartilhada. '
              'Esses clientes não demonstram que os layouts desta versão do SDK sejam idênticos.', '',
              '## Recomendação por área', '',
              'Regra de planejamento: overrides de APIs quando a superfície observada é delimitada e mapeia diretamente ao backend '
              'do host; manter o SDK quando há muitas APIs relacionadas a estado/layout compartilhado. '
              'Para `nn::os`, a opção `ipc` é somente a categoria manter SDK/fronteira SVC; não há afirmação de um serviço IPC nn::os. '
              'A recomendação cobre as importações observadas; chamadas internas do SDK podem exigir cobertura adicional.', '',
              '| Área | Chamadas | Imagem | APIs observadas/importadas | Chamadores | Serviços do catálogo | Recomendação | Critério |',
              '|---|---:|---:|---:|---:|---|---|---|']
    for area, calls, raw, imported, used, callers, services, svcs, choice, criterion in area_rows:
        report.append(f'| {area} | {calls} | {raw} | {used}/{imported} | {callers} | '
                      f'{services.replace(";", ", ") or "—"} | {choice} | {criterion} |')
    report += ['', '## Importações por área', '',
               '`Chamadores` conta funções distintas do main; `+lacunas` são sites adicionais fora da contagem FDE. '
               'SVCs são números locais do Horizon, identificados em `svc_sites.csv`.', '']
    for area in AREAS:
        report += [f'### {area}', '', '| Importação (demangled) | Chamadas | +lacunas | Chamadores | Serviços do catálogo | SVCs |',
                   '|---|---:|---:|---:|---|---|']
        for r in imports_rows:
            if r[0] != area: continue
            report.append(f'| {r[2]} | {r[3]} | {r[6]} | {r[4]} | {r[10].replace(";", ", ") or "—"} | {r[12].replace(";", ", ") or "—"} |')
        report.append('')
    report += ['## Argumentos de montagem e caminho', '',
               'Para Mount*/Commit*/CreateFile/DeleteFile, o argumento de string observado nas assinaturas é x0; '
               'para OpenFile/OpenDirectory/GetEntryType é x1. Só esse argumento é inspecionado, evitando tratar ponteiros de saída como caminhos.', '',
               'A fatia reversa fica dentro da mesma função, nas 16 instruções anteriores à chamada BL ou tail B. '
               'Aceita ADR, ADRP+ADD, ADD imediato e cópias MOV de 64 bits; bloqueia sobrescritas, chamadas, '
               'transferências de controle e joins diretos conhecidos. Não segue LDR, argumentos de chamadores, '
               'strings formatadas, heap ou pilha; uma escrita W invalida um ponteiro X. '
               'Só exporta ASCII imprimível não vazio com NUL no `.rodata`, de até 4096 bytes.', '',
               f'Recuperadas: {len(fs_rows)}/{len(fs_audit)} chamadas selecionadas; sem string: {missing}. '
               'A ausência de literal local não indica ausência de arquivo ou montagem em runtime.', '',
               '### Nomes de montagem distintos', '',
               'Valores exatos recuperados em Mount* e Commit*; não normalizados nem inferidos de caminhos.', '',
               '| Nome literal | Chamadas |', '|---|---:|']
    report += [f'| `{name}` | {count} |' for name, count in mounts.most_common()] or ['Nenhum literal de montagem recuperado.']
    report += ['', '### Até 30 prefixos de caminho mais frequentes', '',
               'Prefixo termina no segundo `/`, inclusive; quando há menos de dois, mantém o literal inteiro. '
               'Contagem por callsite recuperado, sem inventariar strings globais não passadas às APIs.', '',
               '| Prefixo | Chamadas |', '|---|---:|']
    report += [f'| `{name}` | {count} |' for name, count in prefixes.most_common(30)] or ['Nenhum caminho literal recuperado nesta janela.']
    report += ['', '### Callsites com string recuperada', '',
               '| Chamada RVA | Importação (demangled) | Argumento | Literal |', '|---|---|---|---|']
    for pc, name, register, value in fs_rows:
        # Only short path/mount strings are permitted in the tracked report.
        shown = value if len(value) <= 160 else value[:157] + '...'
        report.append(f'| {pc} | {selected[name]["demangled"]} | {register} | `{shown}` |')
    report += ['', '### Chamadas sem string por motivo', '', '| Motivo | Chamadas |', '|---|---:|']
    report += [f'| {reason} | {count} |' for reason, count in collections.Counter(r[8] for r in fs_audit if r[7] != 'recuperada').most_common()]
    report += ['', '## Sites adicionais fora dos FDEs', '',
               'Conciliação explícita com `call_targets.csv`; o script não altera o censo anterior.', '',
               '| Chamada RVA | Função RVA | Origem | Importação (demangled) |', '|---|---|---|---|']
    report += [f'| {hx(pc)} | {hx(owner)} | {origin} | {selected[name]["demangled"]} |' for pc, name, owner, origin, mnemonic in extra]
    report += ['', '## Limites e cobertura de alcance', '',
               f'Importações selecionadas sem alcance no censo: {len(missing_reach)}. '
               f'Nomes heurísticos distintos que não pertencem ao catálogo local: '
               + ', '.join(sorted({name for r in imports_rows for name in tokens(r[11])})) + '.', '',
               'As chamadas diretas não cobrem usos por GOT, vtables ou dispatchers resolvidos em runtime. '
               'Recomendações api exigem ABI, valores de retorno, handles e vida útil corretos; as contagens não justificam stubs de sucesso vazio.', '',
               '## Saídas e autoverificações', '',
               '- `r5_imports.csv`: superfície por API, contagens FDE/imagem, chamadores, alcance e status.',
               '- `r5_callers.csv`: cada callsite e a função que o contém.',
               '- `r5_fs_strings.csv`: exatamente chamada, importação, registrador e string.',
               '- `r5_fs_string_audit.csv`: todas as tentativas, fontes do endereço e motivos de não recuperação.',
               '- `r5_fs_prefixes.csv`, `r5_area_recommendations.csv`: agregações e critérios de planejamento.', '',
               '1. Importações existem no inventário e têm PLT ou marcador sem_stub.',
               '2. Contagens por API e área conferem com call_targets; a varredura independente dos FDEs também confere.',
               '3. Todas as strings recuperadas foram verificadas por bytes, NUL e limites do rodata. '
               'Testes sintéticos cobrem ADRP+ADD com cópia para outro registrador, destruição do ponteiro por escrita W e joins diretos.', '',
               '## Comandos de aceite', '', '```text', r'build\venv\Scripts\python.exe -I analysis\recomp\r5_api_surface.py',
               *output, 'Saidas: analysis/recomp/generated/r5_*.csv e analysis/recomp/r5_api_surface_report.md',
               f'Exit code: {0 if all(checks) and safety.returncode == 0 else 1}', '```', '',
               '```text', r'python tools\check_repo_safety.py', safety.stdout.strip(), safety.stderr.strip(),
               f'Exit code: {safety.returncode}', '```', '',
               'A verificação acima é invocada pelo script com o mesmo Python em modo isolado; o comando independente '
               'também deve ser executado no aceite. Sem commit ou push.', '']
    REPORT.write_text('\n'.join(report), encoding='utf-8', newline='\n')
    print('Saidas: analysis/recomp/generated/r5_*.csv e analysis/recomp/r5_api_surface_report.md', flush=True)
    return 0 if all(checks) and safety.returncode == 0 else 1


if __name__ == '__main__':
    raise SystemExit(run())
