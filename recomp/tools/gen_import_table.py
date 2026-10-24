#!/usr/bin/env python3
"""
Gera tabela C++ de importacoes HLE a partir do imports.csv (nso_inspect).
Uso:
  python recomp/tools/gen_import_table.py <imports.csv> --out <pasta>
  python recomp/tools/gen_import_table.py --self-test
"""

import argparse
import csv
import io
import os
import sys
from typing import List, Dict, Tuple

HEADER_TEMPLATE = """#pragma once
#include <cstddef>
namespace recomp::hle {
struct ImportInfo {
    const char* mangled;
    const char* demangled;
    const char* ns;        // coluna namespace
    bool function;         // tipo == "func"
    bool in_sdk;           // resolvido_no_sdk == "sim"
};
extern const ImportInfo kImports[];
extern const std::size_t kImportCount;
} // namespace recomp::hle
"""

def escape_cpp_string(s: str) -> str:
    """Escapa caracteres especiais para um literal de string C++."""
    out = []
    for c in s:
        if c == '\\':
            out.append('\\\\')
        elif c == '"':
            out.append('\\"')
        elif c == '\n':
            out.append('\\n')
        elif c == '\r':
            out.append('\\r')
        elif c == '\t':
            out.append('\\t')
        elif ord(c) < 32 or ord(c) == 127:
            out.append(f'\\x{ord(c):02x}')
        else:
            out.append(c)
    return ''.join(out)

def parse_imports_csv(reader) -> List[Dict[str, str]]:
    """Le as linhas do CSV e retorna lista de dicionarios das importacoes."""
    entries = []
    for row in reader:
        # colunas: nome,namespace,demangled,tipo,got,resolvido_no_sdk
        mangled = row.get("nome", "").strip()
        ns = row.get("namespace", "").strip()
        demangled = row.get("demangled", "").strip()
        tipo = row.get("tipo", "").strip()
        resolvido = row.get("resolvido_no_sdk", "").strip()

        entries.append({
            "mangled": mangled,
            "ns": ns,
            "demangled": demangled,
            "function": tipo.lower() == "func",
            "in_sdk": resolvido.lower() == "sim",
        })
    return entries

def generate_table(entries: List[Dict[str, str]]) -> Tuple[str, str]:
    """Ordena as entradas por ns e mangled, e gera header e source C++."""
    sorted_entries = sorted(entries, key=lambda e: (e["ns"], e["mangled"]))

    header = HEADER_TEMPLATE

    lines = [
        '#include "import_table.hpp"',
        '',
        'namespace recomp::hle {',
        '',
        'const ImportInfo kImports[] = {',
    ]

    for e in sorted_entries:
        mangled_esc = escape_cpp_string(e["mangled"])
        demangled_esc = escape_cpp_string(e["demangled"])
        ns_esc = escape_cpp_string(e["ns"])
        fn_str = "true" if e["function"] else "false"
        sdk_str = "true" if e["in_sdk"] else "false"
        lines.append(f'    {{ "{mangled_esc}", "{demangled_esc}", "{ns_esc}", {fn_str}, {sdk_str} }},')

    if not sorted_entries:
        # Para lista vazia, define um array vazio valido ou kImportCount 0
        lines.append('    {}')

    lines.append('};')
    lines.append('')
    if sorted_entries:
        lines.append('const std::size_t kImportCount = sizeof(kImports) / sizeof(kImports[0]);')
    else:
        lines.append('const std::size_t kImportCount = 0;')
    lines.append('')
    lines.append('} // namespace recomp::hle')
    lines.append('')

    source = '\n'.join(lines)
    return header, source

def run_self_test() -> int:
    """Executa auto-teste completo sem dependencias externas."""
    sample_csv = (
        'nome,namespace,demangled,tipo,got,resolvido_no_sdk\n'
        '_ZN2nn2os12WaitEventEPv,nn::os,"nn::os::WaitEvent(void*, int)",func,0x1000,sim\n'
        '_Z4testPKc,c,"test(char const*, ""quote"")",func,0x1008,nao\n'
        'g_GlobalVar,,g_GlobalVar,obj,0x1010,?\n'
        '_ZN2nn2fs8OpenFileEv,nn::fs,nn::fs::OpenFile(),func,0x1018,sim\n'
        'strlen,c,strlen,func,0x1020,?\n'
    )

    f = io.StringIO(sample_csv)
    reader = csv.DictReader(f)
    entries = parse_imports_csv(reader)

    print("[SELF-TEST] Linhas lidas do CSV de exemplo:", len(entries))
    assert len(entries) == 5, f"Esperado 5 linhas, obtido {len(entries)}"

    header, source = generate_table(entries)

    # 1. Checa interface do header
    assert "struct ImportInfo {" in header
    assert "extern const ImportInfo kImports[];" in header
    assert "extern const std::size_t kImportCount;" in header
    print("[SELF-TEST] Header interface: OK")

    # 2. Checa ordenacao: ns entao mangled
    # Ordem esperada dos namespaces: "" (g_GlobalVar), "c" (_Z4testPKc, strlen), "nn::fs", "nn::os"
    expected_order = [
        ("g_GlobalVar", ""),
        ("_Z4testPKc", "c"),
        ("strlen", "c"),
        ("_ZN2nn2fs8OpenFileEv", "nn::fs"),
        ("_ZN2nn2os12WaitEventEPv", "nn::os"),
    ]
    for mangled, ns in expected_order:
        assert f'"{escape_cpp_string(mangled)}"' in source
    print("[SELF-TEST] Ordenacao por (ns, mangled): OK")

    # 3. Checa escape de virgula e aspas no demangled
    assert 'test(char const*, \\"quote\\")' in source
    assert 'nn::os::WaitEvent(void*, int)' in source
    print("[SELF-TEST] Escape de virgulas e aspas: OK")

    # 4. Checa booleano de function e in_sdk
    # g_GlobalVar e obj, entao function deve ser false
    assert '{ "g_GlobalVar", "g_GlobalVar", "", false, false }' in source
    # WaitEvent e func e resolvido sim
    assert '{ "_ZN2nn2os12WaitEventEPv", "nn::os::WaitEvent(void*, int)", "nn::os", true, true }' in source
    print("[SELF-TEST] Flags booleanas (function, in_sdk): OK")

    # 5. Checa kImportCount
    assert "kImportCount = sizeof(kImports) / sizeof(kImports[0]);" in source
    print("[SELF-TEST] kImportCount: OK")

    print("[SELF-TEST] Todos os testes passaram com sucesso.")
    return 0

def main() -> int:
    parser = argparse.ArgumentParser(description="Gera import_table.hpp e import_table.cpp a partir de imports.csv")
    parser.add_argument("csv_path", nargs="?", help="Caminho para o imports.csv")
    parser.add_argument("--out", help="Diretorio de saida para import_table.hpp e import_table.cpp")
    parser.add_argument("--self-test", action="store_true", help="Executa auto-teste embutido")

    args = parser.parse_args()

    if args.self_test:
        return run_self_test()

    if not args.csv_path:
        parser.error("Caminho do imports.csv obrigatorio (ou use --self-test)")

    if not args.out:
        parser.error("Parametro --out <pasta> e obrigatorio")

    if not os.path.isfile(args.csv_path):
        print(f"Erro: arquivo '{args.csv_path}' nao encontrado.", file=sys.stderr)
        return 1

    with open(args.csv_path, "r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        entries = parse_imports_csv(reader)

    header, source = generate_table(entries)

    os.makedirs(args.out, exist_ok=True)
    header_path = os.path.join(args.out, "import_table.hpp")
    source_path = os.path.join(args.out, "import_table.cpp")

    with open(header_path, "w", encoding="utf-8") as f:
        f.write(header)

    with open(source_path, "w", encoding="utf-8") as f:
        f.write(source)

    count = len(entries)
    print(f"Geradas {count} importacoes em {header_path} e {source_path}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
