#!/usr/bin/env python3
"""Offline SVC/service census and bounded SDK direct-call reachability.

Writes only this task's report and ignored generated CSVs. No target execution,
image relocation, prior inventory changes, commit, push, or network at runtime.
"""
from __future__ import annotations

import bisect
import collections
import csv
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

sys.dont_write_bytecode = True
import capstone

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
OUT = HERE / 'generated'
REPORT = HERE / 'kernel_census_report.md'
DEPTH = 12
LIBNX_REV = 'b156b4774fb2df7b93402169509266da7547fc4c'
SVC_SOURCE = f'https://github.com/switchbrew/libnx/blob/{LIBNX_REV}/nx/source/kernel/svc.s'
SERVICE_SOURCE = f'https://github.com/switchbrew/libnx/tree/{LIBNX_REV}/nx/source/services'
# Number/name facts checked against the public libnx syscall wrappers. Unknown
# immediates remain explicit rather than inferred from nearby SDK exports.
SVC_NAMES = {
    0x01: 'SetHeapSize', 0x02: 'SetMemoryPermission', 0x03: 'SetMemoryAttribute',
    0x04: 'MapMemory', 0x05: 'UnmapMemory', 0x06: 'QueryMemory', 0x07: 'ExitProcess',
    0x08: 'CreateThread', 0x09: 'StartThread', 0x0a: 'ExitThread', 0x0b: 'SleepThread',
    0x0c: 'GetThreadPriority', 0x0d: 'SetThreadPriority', 0x0e: 'GetThreadCoreMask',
    0x0f: 'SetThreadCoreMask', 0x10: 'GetCurrentProcessorNumber', 0x11: 'SignalEvent',
    0x12: 'ClearEvent', 0x13: 'MapSharedMemory', 0x14: 'UnmapSharedMemory',
    0x15: 'CreateTransferMemory', 0x16: 'CloseHandle', 0x17: 'ResetSignal',
    0x18: 'WaitSynchronization', 0x19: 'CancelSynchronization', 0x1a: 'ArbitrateLock',
    0x1b: 'ArbitrateUnlock', 0x1c: 'WaitProcessWideKeyAtomic', 0x1d: 'SignalProcessWideKey',
    0x1f: 'ConnectToNamedPort', 0x21: 'SendSyncRequest', 0x22: 'SendSyncRequestWithUserBuffer',
    0x23: 'SendAsyncRequestWithUserBuffer', 0x25: 'GetThreadId', 0x26: 'Break',
    0x27: 'OutputDebugString', 0x28: 'ReturnFromException', 0x29: 'GetInfo',
    0x2a: 'FlushEntireDataCache', 0x2c: 'MapPhysicalMemory', 0x2d: 'UnmapPhysicalMemory',
    0x2f: 'GetLastThreadInfo', 0x32: 'SetThreadActivity', 0x33: 'GetThreadContext3',
    0x34: 'WaitForAddress', 0x35: 'SignalToAddress', 0x36: 'SynchronizePreemptionState',
    0x40: 'CreateSession', 0x41: 'AcceptSession', 0x43: 'ReplyAndReceive',
    0x44: 'ReplyAndReceiveWithUserBuffer', 0x45: 'CreateEvent', 0x4b: 'CreateCodeMemory',
    0x50: 'CreateSharedMemory', 0x51: 'MapTransferMemory', 0x52: 'UnmapTransferMemory',
    0x70: 'CreatePort', 0x72: 'ConnectToPort',
}
# 122 literal smGetService names in the pinned libnx service clients, plus
# service names explicitly supplied by CODEX_TASK.md (including named port sm:).
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
FORMAT = re.compile(r'[a-z0-9:-]{2,8}\Z')


def hx(value):
    return '' if value is None else f'0x{value:x}'


def number(value):
    return int(value, 16) if value.lower().startswith('0x') else int(value)


def read_csv(name, module='main'):
    directory = OUT if module == 'main' else OUT / module
    with (directory / name).open(encoding='utf-8-sig', newline='') as stream:
        return list(csv.DictReader(stream))


def write_csv(name, fields, rows):
    with (OUT / name).open('w', encoding='utf-8', newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(fields)
        writer.writerows(rows)


def branch_target(word, pc):
    offset = word & 0x3ffffff
    if offset & 0x2000000: offset -= 0x4000000
    return pc + offset * 4


def pc_address(word, pc):
    immediate = ((word >> 5) & 0x7ffff) * 4 + ((word >> 29) & 3)
    if immediate & 0x100000: immediate -= 0x200000
    return ((pc & ~0xfff) + immediate * 4096 if word & 0x80000000 else pc + immediate)


def demangle(names):
    names = sorted(set(names))
    executable = shutil.which('llvm-cxxfilt') or shutil.which('c++filt')
    fallback = Path('C:/Program Files/LLVM/bin/llvm-cxxfilt.exe')
    if executable is None and fallback.is_file(): executable = str(fallback)
    if executable is None: return dict(zip(names, names)), 'indisponivel; nomes originais'
    result = subprocess.run([executable], input='\n'.join(names) + '\n', capture_output=True,
                            text=True, encoding='utf-8', errors='replace')
    lines = result.stdout.splitlines()
    if result.returncode or len(lines) != len(names):
        raise ValueError('Demangler output/count mismatch')
    return dict(zip(names, lines)), Path(executable).name


class Module:
    def __init__(self, name):
        self.name = name
        directory = OUT if name == 'main' else OUT / name
        self.image = (directory / 'image.bin').read_bytes()
        self.layout = {r['nome']: (number(r['rva']), number(r['tamanho'])) for r in read_csv('layout.csv', name)}
        self.text_start, self.text_size = self.layout['.text']
        self.text_end = self.text_start + self.text_size
        if self.text_end > len(self.image): raise ValueError('Truncated image')
        self.exports = read_csv('exports.csv', name)
        self.by_name = collections.defaultdict(list)
        self.by_start = collections.defaultdict(list)
        for row in self.exports:
            if row['tipo'] == 'func':
                self.by_name[row['nome']].append(number(row['rva']))
                self.by_start[number(row['rva'])].append(row)
        self.export_starts = sorted(self.by_start)
        self.functions = sorted((number(r['rva']), number(r['tamanho'])) for r in read_csv('functions.csv', name))
        if name == 'main':
            self.functions += [(number(r['rva']), number(r['tamanho'])) for r in read_csv('gap_functions.csv')]
            self.functions.sort()
        self.starts = [s for s, n in self.functions]
        self.sizes = dict(self.functions)
        end = self.text_start
        for start, size in self.functions:
            if start < end or start % 4 or size <= 0 or size % 4 or start + size > self.text_end:
                raise ValueError(f'Invalid function interval {name}:{hx(start)}')
            end = start + size
        self.svc_sites = []
        self.service_strings = {}
        self.references = set()
        self.adjacency = collections.defaultdict(set)
        self.edges = []
        self.unresolved_edges = []
        self.indirect = collections.Counter()
        self.plt = {}
        self.plt_rows = []

    def owner(self, pc):
        i = bisect.bisect_right(self.starts, pc) - 1
        if i >= 0 and pc < self.starts[i] + self.functions[i][1]: return self.starts[i]
        return None

    def exact_export(self, start, labels):
        return '; '.join(sorted(labels[r['nome']] for r in self.by_start.get(start, [])))

    def export_label(self, owner, origin, labels):
        if owner is not None and owner in self.by_start:
            return self.exact_export(owner, labels), 'funcao_exata', 0
        i = bisect.bisect_right(self.export_starts, origin) - 1
        if i < 0: return '', 'ausente', None
        start = self.export_starts[i]
        entries = self.by_start[start]
        contained = [r for r in entries if origin < start + number(r['tamanho'])]
        chosen = contained or entries
        return ('; '.join(sorted(labels[r['nome']] for r in chosen)),
                'contida' if contained else 'proxima_anterior', origin - start)

    def find_strings(self):
        start, size = self.layout['.rodata']
        # Both ends must be NUL boundaries; never select a suffix of a longer
        # string. The upper-case names are accepted only from the known catalog.
        for m in re.finditer(rb'(?<![^\x00])([\x20-\x7e]{2,8})\x00', self.image[start:start + size]):
            name = m[1].decode('ascii')
            if name in KNOWN_SERVICES or FORMAT.fullmatch(name):
                self.service_strings[start + m.start(1)] = name

    def map_plt(self):
        slots = {number(r['rva']): r['simbolo'] for r in read_csv('relocations.csv', self.name)
                 if r['tipo'] == 'jump_slot'}
        image = memoryview(self.image)
        for pc in range(self.text_start, self.text_end - 12, 4):
            first = struct.unpack_from('<I', image, pc)[0]
            if first & 0x9f00001f != 0x90000010: continue
            load, add, branch = struct.unpack_from('<III', image, pc + 4)
            if load & 0xffc003ff != 0xf9400211 or add & 0xffc003ff != 0x91000210 or branch != 0xd61f0220:
                continue
            page = pc_address(first, pc)
            got = page + ((load >> 10) & 0xfff) * 8
            if got != page + ((add >> 10) & 0xfff): raise ValueError('PLT load/add mismatch')
            if got not in slots: continue  # PLT0 is a resolver, not a function slot.
            symbol = slots[got]
            targets = sorted(set(self.by_name.get(symbol, [])))
            if len(targets) > 1: raise ValueError('Ambiguous SDK export name')
            destination = self.owner(targets[0]) if targets else None
            self.plt[pc] = (destination, symbol)
            self.plt_rows.append((pc, got, symbol, targets[0] if targets else None, destination))
        if len(self.plt) != len(slots): raise ValueError('SDK PLT/relocation slot count mismatch')

    def scan(self, emit):
        self.find_strings()
        md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM)
        md.detail = True
        image = memoryview(self.image)
        for pc in range(self.text_start, self.text_end, 4):
            word = struct.unpack_from('<I', image, pc)[0]
            if word & 0xffe0001f == 0xd4000001:
                ins = next(md.disasm(bytes(image[pc:pc + 4]), pc, count=1), None)
                immediate = (word >> 5) & 0xffff
                if ins is None or ins.mnemonic != 'svc' or ins.operands[0].imm != immediate:
                    raise ValueError('Raw SVC/Capstone mismatch')
                self.svc_sites.append((pc, immediate, self.owner(pc)))
            if self.name == 'sdk':
                if word & 0xfffffc1f in (0xd61f0000, 0xd63f0000):
                    owner = self.owner(pc)
                    if owner is not None: self.indirect[owner] += 1
                if word & 0x7c000000 == 0x14000000:
                    owner = self.owner(pc)
                    if owner is not None:
                        target = branch_target(word, pc)
                        if word & 0x80000000 or not owner <= target < owner + self.sizes[owner]:
                            via, symbol = '', ''
                            if target in self.plt:
                                callee, symbol = self.plt[target]
                                via = 'plt'
                            else: callee = self.owner(target)
                            if callee is not None:
                                self.adjacency[owner].add(callee)
                                self.edges.append((pc, owner, target, callee, via, symbol))
                            else: self.unresolved_edges.append((pc, owner, target, symbol))
            kind = word & 0x9f000000
            if kind == 0x10000000:
                target = pc_address(word, pc)
                if target in self.service_strings:
                    self.references.add((pc, pc, target, self.owner(pc), 'ADR'))
            elif kind == 0x90000000:
                page = pc_address(word, pc)
                register = word & 31
                if register == 31: continue
                for other in range(pc + 4, min(pc + 13 * 4, self.text_end), 4):
                    add = struct.unpack_from('<I', image, other)[0]
                    if add & 0xff800000 != 0x91000000 or (add >> 5) & 31 != register: continue
                    target = page + (((add >> 10) & 0xfff) << (12 if add & (1 << 22) else 0))
                    if target not in self.service_strings: continue
                    owner = self.owner(pc)
                    if owner != self.owner(other): continue
                    valid = True
                    for ins in md.disasm(bytes(image[pc + 4:other]), pc + 4):
                        if any(g in ins.groups for g in (capstone.CS_GRP_JUMP, capstone.CS_GRP_CALL,
                                                         capstone.CS_GRP_RET, capstone.CS_GRP_INT)):
                            valid = False
                            break
                        if any(ins.reg_name(r) in (f'x{register}', f'w{register}',
                               'fp' if register == 29 else 'lr' if register == 30 else '')
                               for r in ins.regs_access()[1]):
                            valid = False
                            break
                    # Capstone must cover every intermediate word, including
                    # invalid words (disasm stops early without skipdata).
                    decoded = list(md.disasm(bytes(image[pc:other + 4]), pc)) if valid else []
                    if len(decoded) != (other - pc) // 4 + 1: continue
                    if decoded[0].mnemonic != 'adrp' or decoded[-1].mnemonic != 'add': continue
                    self.references.add((pc, other, target, owner, 'ADRP+ADD'))
        emit(f'{self.name}: SVC={len(self.svc_sites)}; strings candidatas brutas={len(self.service_strings)}; referencias={len(self.references)}')


def walk(root, adjacency, svc_by_function, services_by_function):
    distances, parents = {root: 0}, {root: None}
    queue = collections.deque([root])
    svc_hits, service_hits = {}, {}
    while queue:
        current = queue.popleft()
        for svc in sorted(svc_by_function.get(current, [])):
            svc_hits.setdefault(svc, current)
        for name in sorted(services_by_function.get(current, [])):
            service_hits.setdefault(name, current)
        if distances[current] == DEPTH: continue
        for target in sorted(adjacency.get(current, [])):
            if target not in distances:
                distances[target] = distances[current] + 1
                parents[target] = current
                queue.append(target)
    cut = sum(distances[node] == DEPTH and any(t not in distances for t in adjacency.get(node, []))
              for node in distances)
    return distances, parents, svc_hits, service_hits, cut


def path_to(target, parents):
    result = []
    while target is not None:
        result.append(target)
        target = parents[target]
    return ';'.join(hx(n) for n in reversed(result))


def self_check():
    assert branch_target(0x97ffffff, 0x100) == 0xfc
    assert branch_target(0x14000004, 0x100) == 0x110
    assert pc_address(0x90000000, 0x1234) == 0x1000
    adjacency = {i: {i + 1} for i in range(14)}
    dist, parents, svcs, services, cut = walk(0, adjacency, {12: {0x29}, 13: {0x21}}, {0: {'sm:'}})
    assert max(dist.values()) == 12 and set(svcs) == {0x29} and set(services) == {'sm:'} and cut == 1
    assert path_to(2, parents) == '0x0;0x1;0x2'
    # A backward cycle must not revisit a node or suppress a shallower path.
    assert len(walk(0, {0: {1, 2}, 1: {0, 2}}, {}, {})[0]) == 3


def run():
    self_check()
    output = []
    def emit(line):
        output.append(line)
        print(line, flush=True)
    emit(f'Capstone {capstone.__version__}; profundidade maxima SDK={DEPTH}')
    main, sdk = Module('main'), Module('sdk')
    imports = read_csv('imports.csv')
    labels, demangler = demangle([r['nome'] for m in (main, sdk) for r in m.exports if r['tipo'] == 'func'])
    for row in imports: labels.setdefault(row['nome'], row['demangled'])
    sdk.map_plt()
    emit(f'SDK PLT: {len(sdk.plt)} stubs; resolvidos internos={sum(d is not None for d, n in sdk.plt.values())}')
    main.scan(emit)
    sdk.scan(emit)
    svc_rows, service_rows, reference_rows = [], [], []
    svc_by_function, services_by_function = collections.defaultdict(set), collections.defaultdict(set)
    service_inventory = collections.Counter()
    for module in (main, sdk):
        for pc, immediate, owner in module.svc_sites:
            svc_rows.append((module.name, hx(pc), hx(immediate), SVC_NAMES.get(immediate, 'desconhecida'),
                             hx(owner), module.exact_export(owner, labels)))
            if module is sdk and owner is not None: svc_by_function[owner].add(immediate)
        referenced = collections.defaultdict(set)
        for origin, final, target, owner, method in sorted(module.references):
            name = module.service_strings[target]
            known = name in KNOWN_SERVICES
            export, evidence, distance = module.export_label(owner, origin, labels)
            referenced[target].add((owner, export))
            reference_rows.append((module.name, hx(target), name, hx(origin), hx(final), hx(owner),
                                   export, evidence, hx(distance), method, 'catalogo' if known else 'formato'))
            if module is sdk and owner is not None: services_by_function[owner].add(name)
        for target, name in sorted(module.service_strings.items()):
            known = name in KNOWN_SERVICES
            if not known and target not in referenced: continue
            service_inventory[module.name, 'catalogo' if known else 'formato'] += 1
            owners = referenced[target] or {(None, '')}
            for owner, export in sorted(owners, key=lambda x: (-1 if x[0] is None else x[0], x[1])):
                service_rows.append((module.name, hx(target), name, hx(owner), export))

    sdk_exports = sdk.by_name
    roots, unresolved_imports = {}, []
    for row in imports:
        if row['tipo'] != 'func': continue
        addresses = sorted(set(sdk_exports.get(row['nome'], [])))
        if len(addresses) > 1: raise ValueError('Ambiguous main import resolution')
        if addresses and sdk.owner(addresses[0]) is not None:
            roots[row['nome']] = sdk.owner(addresses[0])
        else: unresolved_imports.append(row['nome'])
    plt_main = read_csv('plt_map.csv')
    resolution = []
    for row in plt_main:
        addresses = sorted(set(sdk_exports.get(row['import'], [])))
        resolution.append((row['import'], row['stub_rva'], ';'.join(hx(a) for a in addresses),
                           hx(roots.get(row['import'])), 'resolvido' if row['import'] in roots else 'nao_resolvido'))
    if len(resolution) != 917: raise ValueError('Main PLT inventory is not 917')
    unresolved_by_fn = collections.Counter(r[1] for r in sdk.unresolved_edges)
    reach_rows, audit_rows, paths = [], [], []
    reaches = {}
    for name, root in sorted(roots.items()):
        distances, parents, svcs, services, cut = walk(root, sdk.adjacency, svc_by_function, services_by_function)
        known = set(services) & KNOWN_SERVICES
        heuristics = set(services) - KNOWN_SERVICES
        reaches[name] = (set(svcs), known, heuristics)
        reach_rows.append((name, ';'.join(hx(n) for n in sorted(svcs)), ';'.join(sorted(services)), len(distances)))
        audit_rows.append((name, hx(root), len(distances), len(svcs), ';'.join(sorted(known)),
                           ';'.join(sorted(heuristics)), cut, sum(sdk.indirect[n] for n in distances),
                           sum(unresolved_by_fn[n] for n in distances)))
        for kind, hits in (('svc', svcs), ('servico', services)):
            for event, function in hits.items():
                paths.append((name, kind, hx(event) if kind == 'svc' else event,
                              hx(function), distances[function], path_to(function, parents)))
    call_counts = {number(r['alvo_rva']): number(r['quantidade_de_chamadas']) for r in read_csv('call_targets.csv')}
    calls_by_import = collections.Counter()
    for row in plt_main: calls_by_import[row['import']] += call_counts.get(number(row['stub_rva']), 0)
    import_metadata = {r['nome']: r for r in imports}
    namespaces = collections.Counter()
    namespace_svc, namespace_known, namespace_heuristic = (collections.defaultdict(set) for i in range(3))
    for name, calls in calls_by_import.items():
        ns = import_metadata[name]['namespace']
        if not ns.startswith('nn::'): continue
        namespaces[ns] += calls
        if name in reaches:
            svcs, known, heuristics = reaches[name]
            namespace_svc[ns].update(svcs)
            namespace_known[ns].update(known)
            namespace_heuristic[ns].update(heuristics)

    write_csv('svc_sites.csv', ('modulo', 'rva', 'svc', 'nome_svc', 'funcao_rva', 'exportacao'), svc_rows)
    write_csv('service_names.csv', ('modulo', 'string_rva', 'nome', 'funcao_rva', 'exportacao'), service_rows)
    write_csv('service_name_evidence.csv', ('modulo', 'string_rva', 'nome', 'origem_rva', 'add_rva', 'funcao_rva',
              'exportacao', 'tipo_exportacao', 'distancia_exportacao', 'forma', 'classe'), reference_rows)
    write_csv('import_kernel_reach.csv', ('importacao', 'svcs', 'servicos', 'funcoes_visitadas'), reach_rows)
    write_csv('import_kernel_reach_audit.csv', ('importacao', 'raiz_sdk_rva', 'funcoes_visitadas', 'svcs_distintas',
              'servicos_catalogo', 'candidatos_formato', 'fronteiras_profundidade_12', 'sites_indiretos', 'chamadas_sem_destino_conhecido'), audit_rows)
    write_csv('import_kernel_paths.csv', ('importacao', 'tipo', 'destino', 'funcao_sdk_rva', 'profundidade', 'caminho_rvas'), paths)
    write_csv('main_plt_sdk_resolution.csv', ('importacao', 'stub_rva', 'exportacao_sdk_rva', 'funcao_sdk_rva', 'status'), resolution)
    write_csv('sdk_plt_resolution.csv', ('stub_rva', 'got_rva', 'simbolo', 'exportacao_sdk_rva', 'funcao_sdk_rva'),
              ((hx(pc), hx(got), symbol, hx(target), hx(owner)) for pc, got, symbol, target, owner in sdk.plt_rows))
    write_csv('sdk_direct_call_edges.csv', ('origem_rva', 'funcao_rva', 'alvo_original_rva', 'funcao_destino_rva', 'via', 'simbolo'),
              ((hx(pc), hx(owner), hx(target), hx(callee), via, symbol) for pc, owner, target, callee, via, symbol in sdk.edges))
    write_csv('sdk_unresolved_direct_edges.csv', ('origem_rva', 'funcao_rva', 'alvo_rva', 'simbolo'),
              ((hx(pc), hx(owner), hx(target), symbol) for pc, owner, target, symbol in sdk.unresolved_edges))
    write_csv('kernel_namespace_reach.csv', ('namespace', 'chamadas_diretas', 'svcs', 'servicos_catalogo', 'candidatos_formato'),
              ((ns, calls, ';'.join(hx(n) for n in sorted(namespace_svc[ns])), ';'.join(sorted(namespace_known[ns])),
                ';'.join(sorted(namespace_heuristic[ns]))) for ns, calls in namespaces.most_common()))

    exceptions = [r for r in svc_rows if not r[4]]
    unknown_svc = [r for r in svc_rows if r[3] == 'desconhecida']
    no_svc = [r for r in reach_rows if not r[1]]
    unmatched_plt = [r for r in resolution if r[4] == 'nao_resolvido']
    # Re-read every generated table used for the requested counts, not just the
    # in-memory arrays. Assertions cover both content and exact column order.
    expected = {'svc_sites.csv': (('modulo', 'rva', 'svc', 'nome_svc', 'funcao_rva', 'exportacao'), svc_rows),
                'service_names.csv': (('modulo', 'string_rva', 'nome', 'funcao_rva', 'exportacao'), service_rows),
                'import_kernel_reach.csv': (('importacao', 'svcs', 'servicos', 'funcoes_visitadas'), reach_rows),
                'main_plt_sdk_resolution.csv': (('importacao', 'stub_rva', 'exportacao_sdk_rva', 'funcao_sdk_rva', 'status'), resolution)}
    disk_ok = True
    for name, (fields, data) in expected.items():
        disk = read_csv(name)
        disk_ok &= (list(disk[0]) == list(fields) if disk else not data)
        disk_ok &= [tuple(r.values()) for r in disk] == [tuple(str(v) for v in r) for r in data]
    checks = [len(svc_rows) == len(main.svc_sites) + len(sdk.svc_sites),
              all(r[3] == SVC_NAMES.get(number(r[2]), 'desconhecida') for r in svc_rows),
              len(resolution) == 917 and len(unmatched_plt) + sum(r[4] == 'resolvido' for r in resolution) == 917,
              disk_ok and len(reach_rows) == len(roots)]
    lines = [f'SVCs: main={len(main.svc_sites)}; sdk={len(sdk.svc_sites)}; fora de funcao={len(exceptions)}; desconhecidas={len(unknown_svc)}',
             f'Servicos: linhas={len(service_rows)}; nomes distintos={len({r[2] for r in service_rows})}; referencias={len(reference_rows)}',
             f'PLT main: {917-len(unmatched_plt)} resolvidas no SDK; {len(unmatched_plt)} nao resolvidas',
             f'Grafo SDK: {len(sdk.edges)} sites diretos resolvidos; {sum(len(v) for v in sdk.adjacency.values())} arestas distintas; {len(sdk.unresolved_edges)} sites nao resolvidos',
             f'Importacoes funcao: {len(roots)} raizes resolvidas; {len(unresolved_imports)} nao resolvidas; sem SVC ate profundidade 12={len(no_svc)}']
    lines += [f'AUTOVERIFICACAO {i}: {"OK" if ok else "FALHOU"}' for i, ok in enumerate(checks, 1)]
    for line in lines: emit(line)
    safety = subprocess.run([sys.executable, '-I', str(ROOT / 'tools/check_repo_safety.py')], cwd=ROOT,
                            capture_output=True, text=True, encoding='utf-8', errors='replace')
    report = ['# Censo de kernel e serviços para R5', '',
              'Levantamento estático de `main` e `sdk`, sem execução ou relocação das imagens. RVAs são locais a cada módulo. '
              'Somente metadados estão neste relatório; evidências detalhadas ficam nos CSVs ignorados em `generated/`.', '',
              '## Resultado', '', *[f'- {line}' for line in lines[:5]], '',
              'O conjunto presente no SDK é uma capacidade do módulo; o conjunto alcançável pelas importações é uma '
              'aproximação estática por chamadas diretas. Nenhum dos dois demonstra execução efetiva no boot.', '',
              '## Método e fronteiras', '',
              f'- Capstone `{capstone.__version__}` confirma cada SVC identificado por máscara A64. '
              f'Demangler observado: `{demangler}`. Inventários de funções: main={len(main.functions)}, sdk={len(sdk.functions)}.',
              '- Funções conhecidas usam `.eh_frame`; no main incluem também `gap_functions.csv`. Exportações são associadas '
              'pelo RVA exato para as SVCs. Um wrapper sem símbolo fica sem exportação; não recebe um nome inventado `nn::svc`.',
              f'- Números e nomes públicos das SVCs: [wrappers libnx]({SVC_SOURCE}). '
              f'Catálogo de serviços: [clientes libnx]({SERVICE_SOURCE}) e nomes fornecidos na tarefa. '
              f'Revisão fixada: `{LIBNX_REV}`; {len(KNOWN_SERVICES)} nomes no catálogo local.',
              '- Busca em `.rodata`: ASCII de 2–8 bytes entre limites NUL. Nomes do catálogo incluem maiúsculas; '
              'nomes adicionais precisam corresponder a `[a-z0-9:-]{2,8}` e ter referência ADRP+ADD ou ADR. '
              'Esses adicionais são candidatos de formato, não serviços IPC confirmados. Strings conhecidas sem referência '
              'permanecem no inventário com função/exportação vazias e não alimentam o grafo.',
              '- Referências ADRP+ADD permitem registradores de destino diferentes e até 12 instruções, dentro da mesma função, '
              'sem sobrescrita do registrador de página ou transferência de controle intermediária. '
              'Nomes montados por constantes imediatas, tabelas de ponteiros, cópias ou memória mutável não são recuperados.',
              '- Para serviços, a exportação é exata quando coincide com a função; caso contrário é contida ou a anterior mais próxima. '
              '`service_name_evidence.csv` distingue esses casos e registra a distância. Nome próximo não é atribuição confirmada.',
              '- Grafo do SDK: BL e B que sai da função; alvos são funções conhecidas. Stubs da PLT do próprio SDK são '
              'resolvidos pelo GOT/JUMP_SLOT e pelo nome exato da exportação, sem consumir nível adicional. '
              'Chamadas sem destino conhecido são auditadas, não silenciosamente tratadas como código puro.',
              f'- Busca BFS por importação de função resolvida: raiz no nível 0, até {DEPTH} arestas. '
              'O nível 12 é inspecionado, sem expansão. BR/BLR, callbacks, vtables, condicionais entre funções e retorno '
              'do SDK para callbacks do main não são seguidos. O grafo não é sensível a argumentos nem a caminhos executáveis.',
              '- `servicos` no CSV de alcance contém os nomes de ambas as classes; o CSV de auditoria separa catálogo e formato. '
              'O relatório separa as classes para não promover strings genéricas a serviços reais.',
              '- Ausência de SVC neste grafo limitado não prova pureza nem que uma API dispense suporte nativo. '
              'A lista abaixo significa somente ausência de SVC nas arestas analisadas.', '',
              '## SVCs por número', '', '| SVC | Nome Horizon | Main | SDK | Exportações dos wrappers / RVAs sem símbolo |',
              '|---|---|---:|---:|---|']
    for immediate in sorted({number(r[2]) for r in svc_rows}):
        selected = [r for r in svc_rows if number(r[2]) == immediate]
        wrappers = '; '.join(f'{r[0]}:{r[5] or r[4] or r[1]}' for r in selected)
        report.append(f'| {hx(immediate)} | {SVC_NAMES.get(immediate,"desconhecida")} | '
                      f'{sum(r[0]=="main" for r in selected)} | {sum(r[0]=="sdk" for r in selected)} | {wrappers} |')
    report += ['', 'SVCs fora de funções conhecidas (exceções explícitas):', '']
    report += [f'- {r[0]}: {r[1]}, {r[2]}, {r[3]}.' for r in exceptions] or ['Nenhuma.']
    report += ['', '## Serviços encontrados', '', '| Módulo | Strings de catálogo (RVAs distintos) | Candidatos de formato referenciados |', '|---|---:|---:|']
    for name in ('main', 'sdk'):
        report.append(f'| {name} | {service_inventory[name,"catalogo"]} | {service_inventory[name,"formato"]} |')
    report += ['', 'Nomes do catálogo presentes e quantidade de funções que referenciam:', '', '| Nome | Main | SDK |', '|---|---:|---:|']
    for name in sorted({r[2] for r in service_rows if r[2] in KNOWN_SERVICES}):
        counts = [len({r[3] for r in service_rows if r[0] == module and r[2] == name and r[3]}) for module in ('main','sdk')]
        report.append(f'| {name} | {counts[0]} | {counts[1]} |')
    report += ['', '## 30 namespaces nn:: com mais chamadas diretas do main', '',
               'Prioridade contada por `call_targets.csv` + `plt_map.csv`; inclui namespaces com zero chamadas quando necessário. '
               'SVCs/nomes são a união das importações do namespace, não multiplicados pela contagem de chamadas.', '',
               '| Namespace | Chamadas | SVCs | Serviços do catálogo | Candidatos de formato |', '|---|---:|---|---|---|']
    for ns, calls in namespaces.most_common(30):
        report.append(f'| {ns} | {calls} | {", ".join(hx(n) for n in sorted(namespace_svc[ns])) or "—"} | '
                      f'{", ".join(sorted(namespace_known[ns])) or "—"} | {", ".join(sorted(namespace_heuristic[ns])) or "—"} |')
    report += ['', '## Importações sem SVC alcançada no grafo até nível 12', '',
               f'{len(no_svc)} importações; não é uma prova de pureza. A auditoria registra cortes de profundidade, '
               'chamadas indiretas e fronteiras não resolvidas por importação.', '']
    report += [f'- {import_metadata[r[0]]["demangled"]}' for r in no_svc]
    report += ['', '## Resolução das 917 importações com stub', '',
               f'{917-len(unmatched_plt)} resolvidas; {len(unmatched_plt)} não resolvidas pelo nome de exportação de função do SDK.', '']
    report += [f'- {r[0]} ({r[1]})' for r in unmatched_plt] or ['Nenhuma importação com stub ficou sem resolução.']
    report += ['', 'Importações de função sem raiz conhecida no SDK (incluindo as sem stub):', '']
    report += [f'- {name}' for name in unresolved_imports] or ['Nenhuma.']
    report += ['', '## Arquivos e autoverificações', '',
               '- `svc_sites.csv`, `service_names.csv`, `import_kernel_reach.csv`: entregas principais.',
               '- `service_name_evidence.csv`: referências, origem do nome e qualidade da exportação associada.',
               '- `main_plt_sdk_resolution.csv`, `sdk_plt_resolution.csv`: resolução por símbolo e GOT.',
               '- `sdk_direct_call_edges.csv`, `sdk_unresolved_direct_edges.csv`: arestas e fronteiras do grafo.',
               '- `import_kernel_reach_audit.csv`, `import_kernel_paths.csv`: limites e caminhos mínimos por evento.',
               '- `kernel_namespace_reach.csv`: prioridade e alcance por namespace.', '',
               '1. SVCs dentro de função conhecida ou listadas na seção de exceções; nenhuma exceção é ocultada.',
               '2. Todo número recebe nome público ou o marcador `desconhecida`.',
               '3. Resolução das 917 entradas da PLT contada e lista completa das não resolvidas.',
               '4. CSVs principais relidos e comparados integralmente, incluindo ordem das colunas e contagens. '
               'Testes sintéticos validam B/BL, ADRP, profundidade inclusiva 12, ciclos e caminhos mínimos.', '',
               '## Comandos de aceite', '', '```text', r'build\venv\Scripts\python.exe -I analysis\recomp\kernel_census.py',
               *output, 'Saidas: analysis/recomp/generated/*.csv e analysis/recomp/kernel_census_report.md',
               f'Exit code: {0 if all(checks) and safety.returncode == 0 else 1}', '```', '',
               '```text', r'python tools\check_repo_safety.py', safety.stdout.strip(), safety.stderr.strip(),
               f'Exit code: {safety.returncode}', '```', '',
               'A verificação acima é invocada pelo script com o mesmo Python em modo isolado; '
               'o comando independente deve ser repetido no aceite. Sem commit ou push.', '']
    REPORT.write_text('\n'.join(report), encoding='utf-8', newline='\n')
    print('Saidas: analysis/recomp/generated/*.csv e analysis/recomp/kernel_census_report.md', flush=True)
    return 0 if all(checks) and safety.returncode == 0 else 1


if __name__ == '__main__':
    raise SystemExit(run())
