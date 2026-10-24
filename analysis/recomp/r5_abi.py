#!/usr/bin/env python3
"""AAPCS64 worksheets from imports and pinned public cleanroom declarations.

Only writes this task's report and generated/r5_abi*.csv, r5_types.csv and
generated/r5_public_abi.csv. Never edits or executes preceding task scripts.
Public sources are fetched once per run at immutable revisions, without retry.
"""
from __future__ import annotations

import collections
import concurrent.futures
import csv
import hashlib
import math
from pathlib import Path
import re
import subprocess
import sys
import urllib.request

sys.dont_write_bytecode = True
import capstone
from capstone import arm64_const as A

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
OUT = HERE / 'generated'
REPORT = HERE / 'r5_abi_report.md'
NNREV = '246c3b482d045934ed004f50c5ada37f3b10b703'
RUSTREV = 'd23f131663ec72a87c84914e6e738999f3ab403d'
AAPCS = 'https://github.com/ARM-software/abi-aa/blob/2025Q1/aapcs64/aapcs64.rst'
CPPABI = 'https://github.com/ARM-software/abi-aa/blob/2025Q1/cppabi64/cppabi64.rst'
AREAS = ['fs', 'hid', 'oe', 'account', 'time', 'settings', 'err', 'nifm', 'aoc', 'pl', 'vi', 'audio']
BITFLAG = 'nn::util::BitFlagSet<32, nn::hid::NpadStyleTag>'
PRIMITIVES = {'bool': (1, 1), 'char': (1, 1), 'signed char': (1, 1), 'unsigned char': (1, 1),
              'short': (2, 2), 'unsigned short': (2, 2), 'int': (4, 4), 'unsigned int': (4, 4),
              'long': (8, 8), 'unsigned long': (8, 8), 'long long': (8, 8), 'unsigned long long': (8, 8),
              'float': (4, 4), 'double': (8, 8), '__int128': (16, 16)}
ALIASES = {'u8': 'unsigned char', 's8': 'signed char', 'u16': 'unsigned short', 's16': 'short',
           'u32': 'unsigned int', 's32': 'int', 'u64': 'unsigned long', 's64': 'long',
           'f32': 'float', 'f64': 'double', 'size_t': 'unsigned long', 'ulong': 'unsigned long',
           'Result': 'nn::Result', 'ApplicationId': 'nn::ApplicationId', 'NpadStyleSet': BITFLAG}


def hx(value):
    return '' if value is None else f'0x{value:x}'


def read_csv(name):
    with (OUT / name).open(encoding='utf-8-sig', newline='') as stream:
        return list(csv.DictReader(stream))


def write_csv(name, columns, rows):
    with (OUT / name).open('w', encoding='utf-8', newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(columns)
        writer.writerows(rows)


def matching(text, start, opening='(', closing=')'):
    depth = 0
    for i in range(start, len(text)):
        if text[i] == opening: depth += 1
        elif text[i] == closing:
            depth -= 1
            if depth == 0: return i
    raise ValueError('Unbalanced signature/source delimiters')


def split_types(text):
    parts, start, stack = [], 0, []
    pairs = {')': '(', '>': '<', ']': '['}
    for i, char in enumerate(text):
        if char in '(<[': stack.append(char)
        elif char in ')>]':
            if not stack or stack.pop() != pairs[char]: raise ValueError('Unbalanced argument list')
        elif char == ',' and not stack:
            parts.append(text[start:i].strip()); start = i + 1
    if stack: raise ValueError('Unbalanced argument list')
    if text[start:].strip(): parts.append(text[start:].strip())
    return parts


def signature(demangled):
    depth = 0
    for i, char in enumerate(demangled):
        if char == '<': depth += 1
        elif char == '>': depth -= 1
        elif char == '(' and depth == 0:
            end = matching(demangled, i)
            return demangled[:i], split_types(demangled[i + 1:end]), demangled[end + 1:].strip()
    raise ValueError('Missing function argument list')


def scope_name(full):
    if '::operator ' in full:
        scope, name = full.split('::operator ', 1)
        return scope, 'operator ' + name
    return full.rsplit('::', 1)


def canonical(text, area='', remove_name=False):
    text = re.sub(r'\s+', ' ', text.strip()).split('=', 1)[0].strip()
    if remove_name:
        # No callback declarations are matched through this C++ fallback.
        m = re.search(r'\s+([A-Za-z_]\w*)$', text)
        if m and m[1] not in ('const', 'int', 'long', 'short', 'char', 'double', 'float'):
            if not text[:m.start()].endswith('::'): text = text[:m.start()].strip()
    text = re.sub(r'\bconst\s+([A-Za-z_:][\w:]*(?:<[^>]+>)?)', r'\1 const', text)
    text = re.sub(r'(?<![:\w])(?:u8|s8|u16|s16|u32|s32|u64|s64|f32|f64|size_t|ulong|Result|ApplicationId|NpadStyleSet)\b',
                  lambda m: ALIASES[m[0]], text)
    text = re.sub(r'\s*([*&])\s*', r'\1', text)
    if area:
        # Qualify bare named types after primitive/alias normalization.
        text = re.sub(r'(?<![:\w])([A-Z][A-Za-z_0-9]*)(?![\w:])', lambda m: f'nn::{area}::{m[1]}', text)
    text = text.replace('nn::hid::nn::util', 'nn::util')
    return re.sub(r'\s+', ' ', text).strip()


def source_url(repo, rev, path):
    return f'https://github.com/{repo}/blob/{rev}/{path}'


def public_sources():
    requests = []
    paths = ['include/nn/' + a + '.h' for a in AREAS] + ['include/nn/nn.h', 'include/nn/os.h',
             'include/nn/fs/fs_types.h', 'include/nn/util/util_BitFlagSet.h']
    paths += ['include/nn/fs/' + a + '.h' for a in ('fs_files', 'fs_directories', 'fs_mount', 'fs_rom', 'fs_save')]
    requests += [('open-ead/nnheaders', NNREV, p) for p in paths]
    requests += [('ultimate-research/nnsdk-rs', RUSTREV, 'src/nn/' + a + '.rs')
                 for a in AREAS if a != 'pl']
    requests += [('ultimate-research/nnsdk-rs', RUSTREV, 'src/common_types.rs')]
    requests += [('ultimate-research/nnsdk-rs', RUSTREV, 'src/nn/os.rs')]
    def fetch(item):
        repo, rev, path = item
        req = urllib.request.Request(f'https://raw.githubusercontent.com/{repo}/{rev}/{path}',
                                     headers={'User-Agent': 'R5-ABI-public-evidence'})
        with urllib.request.urlopen(req, timeout=30) as stream: raw = stream.read()
        return source_url(repo, rev, path), raw.decode('utf-8', errors='replace'), hashlib.sha256(raw).hexdigest()
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        return list(pool.map(fetch, requests))


def public_functions(sources, selected):
    result = collections.defaultdict(list)
    for url, raw, digest in sources:
        if url.endswith('.rs'):
            pattern = r'#\[link_name\s*=\s*"([^"]+)"\]\s*pub fn\s+(\w+)\s*\('
            for m in re.finditer(pattern, raw):
                name = m[1].replace(r'\u{1}', '')
                if name not in selected: continue
                close = matching(raw, m.end() - 1)
                tail = re.match(r'\s*(?:->\s*([^;]+))?;', raw[close + 1:])
                if not tail: continue
                params = split_types(raw[m.end():close])
                return_rust = re.sub(r'\s+', ' ', tail[1] or 'void').strip()
                area = selected[name]['area'].split('::')[1].split()[0]
                ret = return_rust.replace('root::nn::', 'nn::').replace('root::', '')
                rust_aliases = {'u64': 'unsigned long', 's64': 'long', 'u32': 'unsigned int', 's32': 'int',
                                'i32': 'int', 'f32': 'float', 'f64': 'double', 'Result': 'nn::Result',
                                'NpadStyleSet': BITFLAG}
                ret = rust_aliases.get(ret, ret)
                if ret.startswith('*mut ') or ret.startswith('*const '):
                    ret = ret.split(' ', 1)[1] + '*'
                ret = canonical(ret, area)
                this = bool(params and re.match(r'(?:this|_self)\s*:', params[0]))
                # Some cleanroom bindings use arg1 for this: parameter count
                # supplies an independent check against the C++ mangled args.
                cpp_params = signature(selected[name]['demangled'])[1]
                if len(params) == len(cpp_params) + 1: this = True
                elif len(params) != len(cpp_params): continue
                line = raw.count('\n', 0, m.start()) + 1
                result[name].append((ret, 'instancia' if this else 'sem_this', url + f'#L{line}', 'rust_link_exato'))
        else:
            source = re.sub(r'/\*.*?\*/|//[^\n]*', '', raw, flags=re.S)
            area_match = re.search(r'/include/nn/(\w+)(?:/|\.h)', url)
            area = area_match[1]
            classes = []
            for m in re.finditer(r'\b(?:class|struct)\s+(\w+)\b[^;{}]*\{', source):
                classes.append((m.start(), matching(source, m.end() - 1, '{', '}'), m[1]))
            for name, row in selected.items():
                full, params, suffix = signature(row['demangled'])
                scope, method = scope_name(full)
                if not scope.startswith('nn::' + area) or 'operator' in method or '(' in method: continue
                if re.search(r'(?:C[123]|D[012])E', name): continue
                for m in re.finditer(r'\b' + re.escape(method) + r'\s*\(', source):
                    close = matching(source, m.end() - 1)
                    if not re.match(r'\s*(?:const\s*)?;', source[close + 1:]): continue
                    parent = [r for r in classes if r[0] < m.start() < r[1]]
                    parent_name = parent[-1][2] if parent else None
                    wanted_parent = scope.split('::')[-1] if scope.count('::') > 1 else None
                    if wanted_parent != parent_name: continue
                    head = re.search(r'((?:static\s+)?[\w:]+(?:\s*[*&])?)\s*$', source[:m.start()])
                    if not head: continue
                    src_params = split_types(source[m.end():close])
                    if src_params == ['void']: src_params = []
                    try: normalized = [canonical(t, area, True) for t in src_params]
                    except ValueError: continue
                    if normalized != [canonical(t) for t in params]: continue
                    ret = canonical(re.sub(r'^static\s+', '', head[1]), area)
                    this = 'instancia' if parent_name and not head[1].startswith('static ') else 'sem_this'
                    result[name].append((ret, this, url, 'cpp_assinatura_compativel'))
    return result


def type_catalog(sources):
    urls = {url.rsplit('/', 1)[-1]: url for url, text, digest in sources if url.endswith('.h')}
    types = {}
    def add(name, size, alignment, fields, file, kind='agregado', **extra):
        types[name] = dict(size=size, align=alignment, fields=fields, source=urls[file], kind=kind,
                           trivial=True, status='publico', **extra)
    add('nn::fs::FileHandle', 8, 8, '0x00: armazenamento u64', 'fs_types.h')
    add('nn::fs::DirectoryHandle', 8, 8, '0x00: armazenamento u64', 'fs_types.h')
    add('nn::fs::WriteOption', 4, 4, '0x00: flags int', 'fs_types.h')
    add('nn::fs::DirectoryEntry', 0x310, 8,
        '0x000: char[0x301]; 0x301: padding[3]; 0x304: tipo u32; 0x308: tamanho s64', 'fs_types.h')
    add('nn::ApplicationId', 8, 8, '0x00: identificador u64', 'nn.h')
    add('nn::account::Uid', 16, 8, '0x00: u64[2]', 'account.h')
    add('nn::account::UserHandle', 24, 8, '0x00: Uid (16); 0x10: ponteiro (8)', 'account.h')
    add('nn::account::NetworkServiceAccountId', 8, 8, '0x00: identificador u64', 'account.h')
    add('nn::account::Nickname', 0x21, 1, '0x00: char[0x21]', 'account.h')
    add('nn::time::CalendarTime', 8, 2, '0x00: s16 ano; 0x02/03/04/05/06: s8 mes/dia/hora/minuto/segundo; 0x07: padding', 'time.h')
    add('nn::time::PosixTime', 8, 8, '0x00: contador de 64 bits (fonte C++ usa u64)', 'time.h')
    add('nn::time::CalendarAdditionalInfo', 24, 4,
        '0x00: dia da semana int; 0x04: dia do ano s32; 0x08: char[8]; 0x10: bool; 0x11: padding[3]; 0x14: desvio UTC s32', 'time.h')
    add('nn::TimeSpan', 8, 8, '0x00: nanossegundos u64 na fonte publica', 'time.h')
    add('nn::settings::LanguageCode', 8, 1, '0x00: char[8]', 'settings.h')
    add(BITFLAG, 4, 4, '0x00: uint32_t _storage[1]', 'util_BitFlagSet.h')
    # The task explicitly supplies Result's four-byte ABI. The public C carrier
    # is cited as representation corroboration, not a full C++ class definition.
    result_url = next(u for u, text, digest in sources if u.endswith('/common_types.rs'))
    types['nn::Result'] = dict(size=4, align=4, fields='0x00: resultado de 32 bits; classe C++ completa nao reconstruida',
                              source=result_url, kind='resultado', trivial=True, status='publico_e_tarefa')
    for suffix in ('FullKey', 'Handheld', 'JoyDual', 'JoyLeft', 'JoyRight'):
        add(f'nn::hid::Npad{suffix}State', 40, 8,
            '0x00: sampling u64; 0x08: botoes u64; 0x10/0x18: analogicos s32[2]; 0x20: atributos u32; 0x24: padding', 'hid.h')
    add('nn::hid::SixAxisSensorState', 96, 8,
        '0x00/08: delta/sampling u64; 0x10/1c/28: aceleracao/velocidade/angulo float[3]; 0x34: matriz float[3][3]; 0x58: atributos u32; 0x5c: padding', 'hid.h')
    # Named enum definitions are public evidence of the integer classification.
    # Default AAPCS64 C++ enum containers are int-sized for these small values.
    for url, raw, digest in sources:
        if url.endswith('.rs'):
            area_match = re.search(r'/src/nn/(\w+)\.rs', url)
            if area_match:
                area = area_match[1]
                for m in re.finditer(r'pub type (\w+)\s*=\s*(?:root::)?(u32|s32|i32);', raw):
                    name = f'nn::{area}::{m[1]}'
                    if name not in types:
                        types[name] = dict(size=4, align=4, fields='alias inteiro de 32 bits na fonte publica',
                                           source=url, kind='enum', trivial=True, status='publico')
                for m in re.finditer(r'#\[repr\(C\)\]\s*(?:#\[[^\]]*\]\s*)*pub enum (\w+)\s*\{', raw):
                    name = f'nn::{area}::{m[1]}'
                    if name not in types:
                        types[name] = dict(size=4, align=4, fields='enum repr(C); valores pequenos na fonte publica',
                                           source=url, kind='enum', trivial=True, status='publico')
        if not url.endswith('.h'): continue
        area_match = re.search(r'/include/nn/(\w+)(?:/|\.h)', url)
        area = area_match[1]
        clean = re.sub(r'/\*.*?\*/|//[^\n]*', '', raw, flags=re.S)
        for m in re.finditer(r'\benum\s+(?:class\s+)?(\w+)\s*(?::\s*(\w+))?\s*\{', clean):
            name = 'nn::' + area + '::' + m[1]
            underlying = canonical(m[2] or 'int')
            if underlying not in PRIMITIVES: continue
            size, align = PRIMITIVES[underlying]
            types[name] = dict(size=size, align=align, fields=f'enum; transporte {underlying}; valores na fonte',
                               source=url, kind='enum', trivial=True, status='publico')
    return types


def base_type(text):
    if '(*)' in text.replace(' ', '') or '(*' in text: return None
    return re.sub(r'\bconst\b|\bvolatile\b|[&*]', '', text).strip()


class Allocator:
    def __init__(self, types):
        self.types, self.x, self.v, self.stack, self.unknown = types, 0, 0, 0, False

    def argument(self, typ):
        pointer = '*' in typ or '&' in typ
        if pointer: kind, size, align, regs = ('referencia' if '&' in typ and '(*' not in typ else 'ponteiro'), 8, 8, 1
        elif typ in ('float', 'double'):
            if self.unknown: return 'flutuante', 'indeterminado_apos_tipo_desconhecido'
            if self.v < 8:
                local = f'v{self.v}'; self.v += 1
                return 'flutuante', local
            self.stack = (self.stack + 7) & ~7
            local = f'pilha+0x{self.stack:x}'; self.stack += 8
            return 'flutuante', local
        elif typ in PRIMITIVES:
            kind, (size, align) = 'inteiro', PRIMITIVES[typ]
            regs = math.ceil(size / 8)
        elif typ in self.types and self.types[typ]['size'] is not None:
            info = self.types[typ]; size, align = info['size'], info['align']
            if info.get('hfa'):
                count = info['hfa']
                if self.unknown: return 'hfa', 'indeterminado_apos_tipo_desconhecido'
                if self.v + count <= 8:
                    local = '+'.join(f'v{i}' for i in range(self.v, self.v + count)); self.v += count
                    return 'hfa', local
                self.v = 8
                self.stack = (self.stack + max(8, align) - 1) & ~(max(8, align) - 1)
                local = f'pilha+0x{self.stack:x}'; self.stack += (size + 7) & ~7
                return 'hfa', local
            kind = 'enum' if info['kind'] == 'enum' else 'agregado'
            if size > 16 or not info.get('trivial', False): kind, size, align = 'copia_indireta', 8, 8
            regs = math.ceil(size / 8)
        else:
            self.unknown = True
            return 'precisa_tamanho', 'indeterminado'
        if self.unknown: return kind, 'indeterminado_apos_tipo_desconhecido'
        if align >= 16: self.x = (self.x + 1) & ~1
        if self.x + regs <= 8:
            local = '+'.join(f'x{i}' for i in range(self.x, self.x + regs)); self.x += regs
            return kind, local
        self.x = 8
        self.stack = (self.stack + max(8, align) - 1) & ~(max(8, align) - 1)
        local = f'pilha+0x{self.stack:x}'; self.stack += (size + 7) & ~7
        return kind, local


def return_location(typ, types):
    if typ == 'void': return 'void', 'sem_resultado'
    if '*' in typ or '&' in typ: return 'ponteiro', 'x0'
    if typ in ('float', 'double'): return 'flutuante', 'v0'
    if typ in PRIMITIVES:
        size, align = PRIMITIVES[typ]
        return 'inteiro', 'w0' if size <= 4 else 'x0' if size <= 8 else 'x0+x1'
    if typ in types and types[typ]['size'] is not None:
        info = types[typ]
        if info.get('hfa'): return 'hfa', '+'.join(f'v{i}' for i in range(info['hfa']))
        if info['size'] > 16 or not info.get('trivial', False): return 'resultado_indireto', 'x8'
        if info['kind'] in ('enum', 'resultado'): return info['kind'], 'w0' if info['size'] <= 4 else 'x0'
        return 'agregado', 'x0' if info['size'] <= 8 else 'x0+x1'
    return 'precisa_tamanho', 'indeterminado'


def self_check():
    assert canonical('nn::Result') == 'nn::Result'
    assert canonical('nn::ApplicationId') == 'nn::ApplicationId'
    assert canonical(canonical(BITFLAG)) == BITFLAG
    assert split_types('void* (*)(unsigned long), nn::util::BitFlagSet<32, nn::hid::NpadStyleTag>') == [
        'void* (*)(unsigned long)', BITFLAG]
    assert scope_name('nn::err::ErrorResultVariant::operator nn::Result') == ('nn::err::ErrorResultVariant', 'operator nn::Result')
    a = Allocator({})
    assert [a.argument('int')[1] for i in range(10)] == [f'x{i}' for i in range(8)] + ['pilha+0x0', 'pilha+0x8']
    a = Allocator({})
    assert a.argument('void*')[1] == 'x0' and a.argument('float')[1] == 'v0' and a.argument('int')[1] == 'x1'
    assert [a.argument('double')[1] for i in range(8)][-1] == 'pilha+0x0'
    types = {'S': dict(size=16, align=16, trivial=True, kind='agregado'),
             'H': dict(size=32, align=8, trivial=True, kind='agregado', hfa=4),
             'N': dict(size=8, align=8, trivial=False, kind='agregado')}
    a = Allocator(types); a.argument('int')
    assert a.argument('S')[1] == 'x2+x3'
    assert return_location('H', types) == ('hfa', 'v0+v1+v2+v3')
    assert return_location('N', types) == ('resultado_indireto', 'x8')
    assert Allocator(types).argument('N') == ('copia_indireta', 'x0')
    a = Allocator(types)
    assert a.argument('Opaque')[0] == 'precisa_tamanho' and 'indeterminado' in a.argument('int')[1]


def binary_observations(selected):
    """Record transport accesses and incoming-x8 candidates, not guessed sizeof."""
    main = (OUT / 'image.bin').read_bytes()
    sdk = (OUT / 'sdk/image.bin').read_bytes()
    exports = {r['nome']: (int(r['rva'], 16), int(r['tamanho'])) for r in read_csv('sdk/exports.csv') if r['tipo'] == 'func'}
    callers = collections.defaultdict(list)
    for r in read_csv('r5_callers.csv'):
        if r['importacao'] in selected: callers[r['importacao']].append(int(r['chamada_rva'], 16))
    md = capstone.Cs(capstone.CS_ARCH_ARM64, capstone.CS_MODE_ARM); md.detail = True
    evidence, x8 = [], []
    for name, row in selected.items():
        full, params, suffix = signature(row['demangled'])
        for pc in callers[name]:
            post = list(md.disasm(main[pc + 4:pc + 20], pc + 4))
            if post and post[0].mnemonic == 'stp' and len(post[0].operands) == 3:
                regs = [post[0].reg_name(o.reg) for o in post[0].operands[:2]]
                if regs == ['x0', 'x1']:
                    evidence.append((name, hx(pc), 'main', 'retorno', 16, 'stp', 'transporte de x0+x1; nao prova tipo nominal ou sizeof completo'))
            if 'GetDesiredLanguage' in full and any(i.mnemonic == 'str' and i.op_str.startswith('x0,') for i in post):
                evidence.append((name, hx(pc), 'main', 'nn::settings::LanguageCode', 8, 'str', 'copia de retorno de 8 bytes; alinhamento e campos dependem da fonte publica'))
            if full == 'nn::hid::SetSupportedNpadStyleSet':
                previous = list(md.disasm(main[pc - 4:pc], pc - 4))
                if previous and previous[0].mnemonic == 'mov' and previous[0].op_str.startswith('w0,'):
                    evidence.append((name, hx(pc), 'main', BITFLAG, 4, 'mov', 'transporte no subregistrador w0; nao prova sizeof isoladamente'))
        if name not in exports: continue
        start, length = exports[name]
        overwritten = False
        for ins in md.disasm(sdk[start:start + min(length, 160)], start):
            # Capstone 5.0.7 reports false read/write flags for some aliases
            # (e.g. LSR's destination). Inspect architectural source operands.
            store = ins.mnemonic.startswith(('str', 'stur', 'stp'))
            reads_x8 = any((o.type == A.ARM64_OP_REG and ins.reg_name(o.reg) == 'x8' and (i > 0 or store))
                           or (o.type == A.ARM64_OP_MEM and any(ins.reg_name(r) == 'x8' for r in (o.mem.base, o.mem.index) if r))
                           for i, o in enumerate(ins.operands))
            if not overwritten and reads_x8:
                x8.append((name, hx(start), hx(ins.address), ins.mnemonic,
                           'entrada x8 lida antes de redefinicao; candidato, nao prova sret'))
                break
            if not store and ins.mnemonic not in ('cmp', 'tst', 'ccmp', 'ccmn'):
                destinations = ins.operands[:2] if ins.mnemonic == 'ldp' else ins.operands[:1]
                if any(o.type == A.ARM64_OP_REG and ins.reg_name(o.reg) in ('x8', 'w8') for o in destinations):
                    overwritten = True
            if any(g in ins.groups for g in (capstone.CS_GRP_JUMP, capstone.CS_GRP_CALL, capstone.CS_GRP_RET)): break
    return evidence, x8


def run():
    self_check()
    output = []
    def emit(line): output.append(line); print(line, flush=True)
    surface = read_csv('r5_imports.csv')
    selected = {r['importacao']: r for r in surface if r['area'] != 'nn::os'}
    imports = {r['nome']: r for r in read_csv('imports.csv')}
    if len(selected) != len(surface) - sum(r['area'] == 'nn::os' for r in surface): raise ValueError('Duplicate imports')
    if any(name not in imports or imports[name]['demangled'] != row['demangled'] for name, row in selected.items()):
        raise ValueError('Import signature mismatch')
    emit(f'Importacoes selecionadas: {len(selected)}; nn::os excluidas: {len(surface) - len(selected)}')
    sources = public_sources()
    emit(f'Fontes publicas fixadas: {len(sources)} arquivos; sem tentativas repetidas')
    facts = public_functions(sources, selected)
    types = type_catalog(sources)
    rows, function_meta, public_rows = [], [], []
    referenced_types, unknown_types = set(), set()
    for name, row in selected.items():
        full, args, qualifiers = signature(row['demangled'])
        scope, method = scope_name(full)
        values = facts.get(name, [])
        for ret, this, url, basis in values: public_rows.append((name, ret, this, basis, url))
        returns = {v[0] for v in values}
        this_options = {v[1] for v in values}
        ctor_dtor = bool(re.search(r'(?:C[123]|D[012])E', name))
        if ctor_dtor:
            ret, ret_status = 'void', 'regra_cpp_construtor_destrutor'
        elif method.startswith('operator '):
            ret, ret_status = method[len('operator '):], 'conversao_codificada_no_nome'
        elif len(returns) == 1:
            ret, ret_status = next(iter(returns)), 'declaracao_publica'
        elif (len(returns) > 1 and all(t in PRIMITIVES or
               (t in types and types[t]['kind'] == 'enum') for t in returns)
              and len({return_location(t, types) for t in returns}) == 2
              and len({return_location(t, types)[1] for t in returns}) == 1
              and any(t in types and types[t]['kind'] == 'enum' for t in returns)):
            # A Rust integer carrier and the C++ named enum can agree on the
            # ABI without agreeing on the spelling of the nominal type.
            ret = next(t for t in sorted(returns) if t in types and types[t]['kind'] == 'enum')
            ret_status = 'enum_cpp_com_carrier_inteiro_compativel'
        else:
            ret, ret_status = 'nao_declarado', 'conflito_fontes' if returns else 'sem_declaracao_publica_compativel'
        member_scope = scope.count('::') > 1
        if ctor_dtor or name.startswith('_ZNK') or method == 'operator=':
            this = 'instancia'
        elif len(this_options) == 1: this = next(iter(this_options))
        elif not member_scope: this = 'sem_this'
        else: this = 'nao_confirmado'
        allocator = Allocator(types)
        if this == 'instancia':
            this_type = scope + (' const*' if name.startswith('_ZNK') else '*')
            kind, local = allocator.argument(this_type)
            rows.append((name, row['demangled'], row['area'], 'this', this_type, 'this', local))
            referenced_types.add(scope)
        elif this == 'nao_confirmado':
            rows.append((name, row['demangled'], row['area'], 'this?', scope + '*', 'precisa_prototipo', 'indeterminado'))
            allocator.unknown = True
            referenced_types.add(scope)
        for i, typ in enumerate(args):
            typ = canonical(typ)
            kind, local = allocator.argument(typ)
            rows.append((name, row['demangled'], row['area'], str(i), typ, kind, local))
            base = base_type(typ)
            if base and base.startswith('nn::'): referenced_types.add(base)
            if kind == 'precisa_tamanho': unknown_types.add(typ)
        kind, local = return_location(ret, types)
        rows.append((name, row['demangled'], row['area'], 'retorno', ret, kind, local))
        base = base_type(ret)
        if base and (base.startswith('nn::') or ret == 'nao_declarado'): referenced_types.add(base)
        if kind == 'precisa_tamanho': unknown_types.add(ret)
        function_meta.append((name, row['demangled'], row['area'], this, ret, ret_status,
                              ';'.join(sorted({v[2] for v in values}))))
    for typ in referenced_types | unknown_types:
        if typ not in types:
            types[typ] = dict(size=None, align=None, fields='nao_confirmado; nao substitua por estrutura/enum de tamanho presumido',
                             source=CPPABI if typ == 'nao_declarado' else '', kind='desconhecido', trivial=False, status='nao_confirmado')
    evidence, x8_candidates = binary_observations(selected)
    evidence_by_type = collections.defaultdict(set)
    for name, pc, module, typ, size, mnemonic, note in evidence:
        if typ in types: evidence_by_type[typ].add(pc)
    relevant_types = sorted(referenced_types | unknown_types)
    type_rows = [(typ, '' if types[typ]['size'] is None else types[typ]['size'],
                  '' if types[typ]['align'] is None else types[typ]['align'], types[typ]['fields'], types[typ]['source'],
                  'nao' if not evidence_by_type[typ] else 'transporte_corrobora_sem_prova_sizeof',
                  ';'.join(sorted(evidence_by_type[typ], key=lambda v: int(v, 16)))) for typ in relevant_types]
    write_csv('r5_abi.csv', ('importacao_mangled', 'demangled', 'area', 'arg_indice', 'arg_tipo', 'classe', 'local'), rows)
    write_csv('r5_types.csv', ('tipo', 'tamanho', 'alinhamento', 'campos', 'fonte', 'confirmado_no_binario', 'evidencia_rva'), type_rows)
    write_csv('r5_public_abi.csv', ('importacao_mangled', 'retorno_publico', 'this_publico', 'criterio', 'fonte'), public_rows)
    write_csv('r5_abi_function_status.csv', ('importacao_mangled', 'demangled', 'area', 'this', 'retorno_tipo', 'status_retorno', 'fontes'), function_meta)
    write_csv('r5_abi_binary_evidence.csv', ('importacao_mangled', 'chamada_rva', 'modulo', 'tipo', 'bytes_observados', 'mnemonico', 'limite'), evidence)
    write_csv('r5_abi_x8_candidates.csv', ('importacao_mangled', 'funcao_sdk_rva', 'observacao_rva', 'mnemonico', 'status'), x8_candidates)
    write_csv('r5_abi_sources.csv', ('fonte', 'sha256'), ((url, digest) for url, text, digest in sources))
    disk = read_csv('r5_abi.csv')
    by_function = collections.defaultdict(list)
    for r in rows: by_function[r[0]].append(r)
    check1 = set(by_function) == set(selected) and len(by_function) == len(selected) and all(
        sum(r[3] == 'retorno' for r in group) == 1 for group in by_function.values())
    check2 = True
    for group in by_function.values():
        assigned = {'x': set(), 'v': set()}
        for r in group:
            if r[3] == 'retorno': continue
            for bank, index in re.findall(r'([xv])(\d+)', r[6]):
                check2 &= int(index) < 8
                assigned[bank].add(int(index))
        check2 &= all(len(s) <= 8 for s in assigned.values())
    check3 = unknown_types <= {r[0] for r in type_rows}
    check1 &= [tuple(r.values()) for r in disk] == [tuple(str(v) for v in r) for r in rows]
    classes = collections.Counter(r[5] for r in rows if r[3] != 'retorno')
    indirect = [r for r in rows if r[3] == 'retorno' and r[6] == 'x8']
    unresolved = {r[0] for r in rows if r[5] == 'precisa_tamanho'}
    unresolved_this = [r for r in function_meta if r[3] == 'nao_confirmado']
    checks = [check1, check2, check3]
    lines = [f'Funcoes: {len(by_function)}; linhas ABI: {len(rows)}; tipos documentados: {len(type_rows)}',
             'Argumentos por classe: ' + '; '.join(f'{kind}={n}' for kind, n in sorted(classes.items())),
             f'Retornos indiretos x8 confirmados por tipo: {len(indirect)}; candidatos x8 na entrada SDK: {len(x8_candidates)}',
             f'Funcoes com precisa_tamanho: {len(unresolved)}; this nao confirmado: {len(unresolved_this)}',
             f'Retornos sem declaracao: {sum(r[5] == "sem_declaracao_publica_compativel" for r in function_meta)}; conflitos publicos: {sum(r[5] == "conflito_fontes" for r in function_meta)}']
    lines += [f'AUTOVERIFICACAO {i}: {"OK" if ok else "FALHOU"}' for i, ok in enumerate(checks, 1)]
    for line in lines: emit(line)
    safety = subprocess.run([sys.executable, '-I', str(ROOT / 'tools/check_repo_safety.py')], cwd=ROOT,
                            capture_output=True, text=True, encoding='utf-8', errors='replace')
    report = ['# Ficha de ABI das APIs da R5', '',
              'Classificação mecânica de argumentos, declarações públicas e limites explícitos. '
              'Não é uma ficha integralmente confirmada: retornos ausentes/conflitantes e tipos sem tamanho continuam bloqueados. '
              'Nenhum arquivo de tarefa anterior foi alterado.', '', '## Totais', '',
              *[f'- {line}' for line in lines[:5]], '',
              '## Fontes e regras', '',
              f'- [AAPCS64 2025Q1]({AAPCS}) e [C++ ABI 2025Q1]({CPPABI}): bancos independentes x0–x7/v0–v7, pilha, alinhamento e x8.',
              f'- Cabeçalhos comunitários [nnheaders]({source_url("open-ead/nnheaders",NNREV,"README.md")}), revisão `{NNREV}`. '
              f'Bindings cleanroom [nnsdk-rs]({source_url("ultimate-research/nnsdk-rs",RUSTREV,"Cargo.toml")}), revisão `{RUSTREV}`. '
              'São reconstruções públicas de outras versões, não declarações originais deste SDK.',
              '- As declarações Rust são ligadas pelo símbolo mangled exato; a alternativa C++ exige escopo, tipos de argumentos '
              'normalizados e distinção de membro/estático. Declarações discordantes não são resolvidas por preferência silenciosa. '
              'Hashes dos arquivos públicos estão em r5_abi_sources.csv; nenhuma fonte é buscada pela branch móvel no script.',
              '- A assinatura demangled e o mangled de funções ordinárias não fornecem o retorno. '
              'Sem declaração compatível, `nao_declarado` é marcador de metadados, não um tipo C++ inventado. '
              'Conversões codificam seu tipo alvo; construtores/destrutores seguem a regra C++ de retorno void.',
              '- Referências, inclusive &&, são ponteiros. Callback é ponteiro. this ocupa x0 em membros não estáticos; '
              'um escopo de classe sozinho não prova que o método seja de instância. Quando faltam fontes, `this?` e os argumentos '
              'ficam indeterminados em vez de deslocar os registradores por suposição.',
              '- Inteiros/enums usam os bits baixos do próximo x; a classe e o tipo indicam a largura. Retornos de até 32 bits '
              'usam w0. Estruturas pequenas usam um/dois x; objetos não triviais para chamadas usam cópia indireta. '
              'Alinhamento 16 ajusta o próximo x para índice par. Excedentes ficam na pilha, relativa ao SP de entrada.',
              '- HFA usa até quatro v, inclusive no retorno: uma HFA de quatro doubles continua em v0–v3 mesmo com 32 bytes. '
              'Outros agregados grandes/não triviais retornam por x8, sem deslocar os argumentos de x0. '
              'Tipos desconhecidos bloqueiam a posição dos argumentos seguintes porque podem consumir x, v ou uma cópia indireta.',
              '- Sem candidato/declaração de x8 não significa que todas as APIs desconhecidas estejam provadas como retorno direto. '
              'A observação binária de x8 só percorre o bloco inicial do SDK, até a primeira transferência de controle.', '',
              '## Uma linha por função', '',
              'Índices de argumentos explícitos começam em 0; this é separado. `?` indica posição indeterminada. '
              'A tabela CSV preserva os tipos e classes completos.', '']
    for area in dict.fromkeys(row['area'] for row in selected.values()):
        report += [f'### {area}', '', '| Função | Argumentos → locais | Retorno |', '|---|---|---|']
        for name, meta in selected.items():
            if meta['area'] != area: continue
            group = by_function[name]
            args_text = '; '.join(f'{r[3]}:{r[6]}' for r in group if r[3] != 'retorno') or 'sem argumentos'
            ret = next(r for r in group if r[3] == 'retorno')
            report.append(f'| {meta["demangled"]} | {args_text} | {ret[4]} → {ret[6]} ({ret[5]}) |')
        report.append('')
    report += ['## Tipos e status', '',
               'Tamanhos/alinhamentos são do modelo público AArch64 LP64. Tipos de buffers e referências encontrados nos argumentos '
               'também aparecem como apoio, mas seu tamanho não é necessário para colocar o ponteiro em registrador. '
               'Nomes apenas opacos não recebem tamanho presumido. Campos são descrições de armazenamento, não cópia de fonte SDK.', '',
               '`transporte_corrobora_sem_prova_sizeof` significa que o binário copia/transporta a largura pública, '
               'sem provar sozinho a extensão completa ou o alinhamento do objeto. Não há promoção automática para sizeof confirmado.', '',
               '| Tipo | Tamanho | Alinhamento | Campos / status | Fonte pública | Evidência main RVA |', '|---|---:|---:|---|---|---|']
    for typ, size, align, fields, source, confirmed, evidence_rva in type_rows:
        link = f'[fonte]({source})' if source else 'nao_confirmado'
        report.append(f'| {typ} | {size if size != "" else "?"} | {align if align != "" else "?"} | {fields} | {link} | {evidence_rva or "—"} |')
    report += ['', '## Declarações conflitantes e escopos pendentes', '']
    conflicts = [r for r in function_meta if r[5] == 'conflito_fontes']
    report += [f'- {r[1]}: ' + '; '.join(f'{ret} ([fonte]({url}))' for ret, this, url, basis in facts[r[0]]) for r in conflicts]
    if not conflicts: report.append('Nenhuma divergência de retorno entre as declarações compatíveis encontradas.')
    report += ['', 'this/estático sem confirmação:', '']
    report += [f'- {r[1]}' for r in unresolved_this] or ['Nenhum.']
    report += ['', '## Evidências binárias de transporte', '', '| Chamada main RVA | Tipo/evento | Bytes | Mnemônico | Limite |', '|---|---|---:|---|---|']
    report += [f'| {pc} | {typ} | {size} | {mnemonic} | {note} |' for name, pc, module, typ, size, mnemonic, note in evidence]
    report += ['', 'Candidatos que leem x8 de entrada antes de redefinir (não promovidos a retorno indireto sem declaração):', '']
    report += [f'- {selected[name]["demangled"]}: SDK {pc}, observação {site}, {mnemonic}.' for name, pc, site, mnemonic, status in x8_candidates] or ['Nenhum no bloco inicial inspecionado.']
    report += ['', '## Saídas e autoverificações', '',
               '- r5_abi.csv e r5_types.csv: colunas e classificação solicitadas.',
               '- r5_public_abi.csv e r5_abi_function_status.csv: declarações, fontes e status por símbolo.',
               '- r5_abi_binary_evidence.csv e r5_abi_x8_candidates.csv: observações estáticas sem listar instruções.',
               '- r5_abi_sources.csv: URLs fixadas e SHA-256 das fontes públicas.', '',
               '1. Cobertura exata das importações selecionadas, uma linha de retorno por símbolo; CSV relido e comparado integralmente.',
               '2. Nenhum argumento nomeia x8+ ou v8+ como registrador ordinário; testes sintéticos cobrem nove/dez argumentos, '
               'bancos independentes, alinhamento 16, HFA de 32 bytes, cópia não trivial e resultado x8.',
               '3. Todo precisa_tamanho está na tabela de tipos, incluindo o marcador nao_declarado para protótipo de retorno ausente.', '',
               '## Comandos de aceite', '', '```text', r'build\venv\Scripts\python.exe -I analysis\recomp\r5_abi.py',
               *output, 'Saidas: analysis/recomp/generated/r5_abi*.csv, r5_types.csv, r5_public_abi.csv e r5_abi_report.md',
               f'Exit code: {0 if all(checks) and safety.returncode == 0 else 1}', '```', '',
               '```text', r'python tools\check_repo_safety.py', safety.stdout.strip(), safety.stderr.strip(),
               f'Exit code: {safety.returncode}', '```', '',
               'A segurança acima é executada com o mesmo interpretador em modo isolado; o comando independente também deve '
               'ser executado no aceite. Sem commit ou push.', '']
    REPORT.write_text('\n'.join(report), encoding='utf-8', newline='\n')
    print('Saidas: analysis/recomp/generated/r5_abi*.csv, r5_types.csv, r5_public_abi.csv e r5_abi_report.md', flush=True)
    return 0 if all(checks) and safety.returncode == 0 else 1


if __name__ == '__main__':
    raise SystemExit(run())
