#!/usr/bin/env python3
"""
Gera relatorio de prioridade de funcoes do SDK para a camada HLE.
Junta plt_map.csv, call_targets.csv e imports.csv para mapear a frequencia
com que cada importacao e chamada no executavel principal.

Uso:
  python recomp/tools/hle_priority.py <pasta_generated> --out <arquivo.md>
  python recomp/tools/hle_priority.py --self-test
"""

import argparse
import csv
import io
import os
import sys
from collections import defaultdict
from typing import Dict, List, Any, Tuple


def escape_markdown_cell(text: str) -> str:
    """Escapa a barra vertical '|' para nao quebrar celulas de tabela Markdown."""
    return text.replace("|", r"\|")


def process_hle_priority(
    plt_reader: csv.DictReader,
    call_targets_reader: csv.DictReader,
    imports_reader: csv.DictReader,
) -> Tuple[Dict[str, Any], str]:
    """
    Processa os três CSVs e retorna estatísticas e o relatório em Markdown.
    """
    # 1. Carrega imports.csv mapeado por nome (mangled)
    imports_by_name: Dict[str, Dict[str, str]] = {}
    for row in imports_reader:
        mangled = row.get("nome", "").strip()
        imports_by_name[mangled] = {
            "mangled": mangled,
            "namespace": row.get("namespace", "").strip() or "(global)",
            "demangled": row.get("demangled", "").strip(),
            "tipo": row.get("tipo", "").strip(),
            "resolvido": row.get("resolvido_no_sdk", "").strip(),
        }

    # 2. Carrega call_targets.csv (linhas com classe == 'stub_plt')
    calls_by_stub: Dict[int, int] = defaultdict(int)
    total_call_targets_calls = 0
    for row in call_targets_reader:
        if row.get("classe", "").strip() == "stub_plt":
            rva_str = row.get("alvo_rva", "").strip()
            rva = int(rva_str, 16) if rva_str.startswith("0x") or rva_str.startswith("0X") else int(rva_str)
            calls = int(row.get("quantidade_de_chamadas", 0))
            calls_by_stub[rva] += calls
            total_call_targets_calls += calls

    # 3. Carrega plt_map.csv e faz o join
    items: List[Dict[str, Any]] = []
    total_joined_calls = 0
    zero_calls_count = 0

    for row in plt_reader:
        stub_str = row.get("stub_rva", "").strip()
        stub_rva = int(stub_str, 16) if stub_str.startswith("0x") or stub_str.startswith("0X") else int(stub_str)
        import_name = row.get("import", "").strip()

        calls = calls_by_stub.get(stub_rva, 0)
        total_joined_calls += calls
        if calls == 0:
            zero_calls_count += 1

        info = imports_by_name.get(import_name, {
            "mangled": import_name,
            "namespace": "(desconhecido)",
            "demangled": import_name,
        })

        items.append({
            "stub_rva": stub_rva,
            "mangled": import_name,
            "demangled": info["demangled"] or import_name,
            "namespace": info["namespace"],
            "calls": calls,
        })

    # Estatísticas por namespace
    by_namespace: Dict[str, List[Dict[str, Any]]] = defaultdict(list)
    namespace_totals: Dict[str, int] = defaultdict(int)

    for item in items:
        ns = item["namespace"]
        by_namespace[ns].append(item)
        namespace_totals[ns] += item["calls"]

    # Ordena itens dentro de cada namespace por chamadas decrescentes, depois demangled
    for ns in by_namespace:
        by_namespace[ns].sort(key=lambda x: (-x["calls"], x["demangled"]))

    # Ordena namespaces por total de chamadas decrescente, depois nome
    sorted_namespaces = sorted(namespace_totals.keys(), key=lambda ns: (-namespace_totals[ns], ns))

    # Top 50 geral
    top_50 = sorted(items, key=lambda x: (-x["calls"], x["demangled"]))[:50]

    stats = {
        "stubs_count": len(items),
        "total_joined_calls": total_joined_calls,
        "total_call_targets_calls": total_call_targets_calls,
        "zero_calls_count": zero_calls_count,
        "active_calls_count": len(items) - zero_calls_count,
        "namespaces_count": len(sorted_namespaces),
    }

    # Gera Markdown
    md_lines = [
        "# Prioridade das Funções do SDK para a Camada HLE",
        "",
        "## Resumo Geral",
        f"- **Importações com stub (PLT):** {len(items)}",
        f"- **Total de chamadas diretas a importações:** {total_joined_calls}",
        f"- **Importações chamadas diretamente:** {len(items) - zero_calls_count}",
        f"- **Importações com 0 chamadas diretas:** {zero_calls_count} (usadas via ponteiro ou inicializadores)",
        f"- **Namespaces identificados:** {len(sorted_namespaces)}",
        "",
        "## Top 50 Importações Mais Chamadas",
        "",
        "| # | Importação (demangled) | Namespace | Chamadas |",
        "| :--- | :--- | :--- | :--- |",
    ]

    for idx, item in enumerate(top_50, 1):
        demangled_esc = escape_markdown_cell(item["demangled"])
        ns_esc = escape_markdown_cell(item["namespace"])
        md_lines.append(f"| {idx} | {demangled_esc} | {ns_esc} | {item['calls']} |")

    md_lines.extend([
        "",
        "## Importações por Namespace",
        "",
    ])

    for ns in sorted_namespaces:
        ns_items = by_namespace[ns]
        total_ns_calls = namespace_totals[ns]
        ns_title_esc = escape_markdown_cell(ns)
        md_lines.extend([
            f"### Namespace `{ns_title_esc}` ({total_ns_calls} chamadas, {len(ns_items)} importações)",
            "",
            "| Importação (demangled) | Chamadas |",
            "| :--- | :--- |",
        ])
        for item in ns_items:
            demangled_esc = escape_markdown_cell(item["demangled"])
            md_lines.append(f"| {demangled_esc} | {item['calls']} |")
        md_lines.append("")

    return stats, "\n".join(md_lines) + "\n"


def run_self_test() -> int:
    """Executa o auto-teste completo em memória sem depender de arquivos externos."""
    mock_plt = io.StringIO(
        "stub_rva,got_rva,import\n"
        "0x1000,0x5000,_Z4funcv\n"
        "0x2000,0x5008,_Z4pipev\n"
        "0x3000,0x5010,_Z7uncalledv\n"
    )

    mock_call_targets = io.StringIO(
        "alvo_rva,classe,quantidade_de_chamadas,exemplo_de_origem_rva\n"
        "0x1000,stub_plt,100,0x40\n"
        "0x2000,stub_plt,250,0x80\n"
        "0x4000,inicio_funcao,999,0x10\n"
    )

    # Note: _Z4pipev has '|' and ',' in demangled name
    mock_imports = io.StringIO(
        "nome,namespace,demangled,tipo,got,resolvido_no_sdk\n"
        "_Z4funcv,nn::os,nn::os::func(),func,0x5000,sim\n"
        '_Z4pipev,c,"c_func(int, float | char)",func,0x5008,nao\n'
        "_Z7uncalledv,nn::fs,nn::fs::uncalled(),func,0x5010,?\n"
    )

    stats, md = process_hle_priority(
        csv.DictReader(mock_plt),
        csv.DictReader(mock_call_targets),
        csv.DictReader(mock_imports),
    )

    print("[SELF-TEST] Stubs processados:", stats["stubs_count"])
    assert stats["stubs_count"] == 3, f"Esperado 3 stubs, obtido {stats['stubs_count']}"

    print("[SELF-TEST] Total chamadas diretas (juncao):", stats["total_joined_calls"])
    assert stats["total_joined_calls"] == 350, f"Esperado 350 chamadas, obtido {stats['total_joined_calls']}"
    assert stats["total_call_targets_calls"] == 350

    print("[SELF-TEST] Stubs com 0 chamadas:", stats["zero_calls_count"])
    assert stats["zero_calls_count"] == 1, f"Esperado 1 stub com 0 chamadas, obtido {stats['zero_calls_count']}"

    # Checa escape do pipe no Markdown
    assert r"c_func(int, float \| char)" in md, "Pipe '|' nao foi escapado corretamente como '\\|'"
    print("[SELF-TEST] Escape de barra vertical '|' em demangled: OK")

    # Checa ordenacao dos namespaces: 'c' (250 chamadas) antes de 'nn::os' (100 chamadas)
    pos_c = md.find("### Namespace `c` (250 chamadas")
    pos_os = md.find("### Namespace `nn::os` (100 chamadas")
    pos_fs = md.find("### Namespace `nn::fs` (0 chamadas")
    assert pos_c != -1 and pos_os != -1 and pos_fs != -1, "Secoes de namespace nao encontradas"
    assert pos_c < pos_os < pos_fs, "Ordem dos namespaces decrescente por chamadas incorreta"
    print("[SELF-TEST] Ordenacao de namespaces por total de chamadas: OK")

    # Checa presenca do Top 50
    assert "## Top 50 Importações Mais Chamadas" in md
    print("[SELF-TEST] Secao Top 50 presente: OK")

    print("[SELF-TEST] Todos os testes passaram com sucesso.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Gera lista de prioridade HLE a partir dos inventarios da R4")
    parser.add_argument("generated_dir", nargs="?", help="Diretorio contendo plt_map.csv, call_targets.csv e imports.csv")
    parser.add_argument("--out", help="Arquivo Markdown de saida (ex: analysis/recomp/hle_priority.md)")
    parser.add_argument("--self-test", action="store_true", help="Executa auto-teste embutido")

    args = parser.parse_args()

    if args.self_test:
        return run_self_test()

    if not args.generated_dir:
        parser.error("Diretorio com os CSVs gerados obrigatorio (ou use --self-test)")

    if not args.out:
        parser.error("Parametro --out <arquivo.md> e obrigatorio")

    plt_path = os.path.join(args.generated_dir, "plt_map.csv")
    call_targets_path = os.path.join(args.generated_dir, "call_targets.csv")
    imports_path = os.path.join(args.generated_dir, "imports.csv")

    for path in [plt_path, call_targets_path, imports_path]:
        if not os.path.isfile(path):
            print(f"Erro: arquivo '{path}' nao encontrado.", file=sys.stderr)
            return 1

    with open(plt_path, "r", encoding="utf-8", newline="") as f_plt, \
         open(call_targets_path, "r", encoding="utf-8", newline="") as f_ct, \
         open(imports_path, "r", encoding="utf-8", newline="") as f_imp:
        stats, md = process_hle_priority(
            csv.DictReader(f_plt),
            csv.DictReader(f_ct),
            csv.DictReader(f_imp),
        )

    out_dir = os.path.dirname(args.out)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    with open(args.out, "w", encoding="utf-8") as f_out:
        f_out.write(md)

    print(f"Importacoes com stub: {stats['stubs_count']}")
    print(f"Total chamadas diretas: {stats['total_joined_calls']} (juncao) | {stats['total_call_targets_calls']} (call_targets stub_plt)")
    print(f"Importacoes com 0 chamadas diretas: {stats['zero_calls_count']}")
    print(f"Relatorio gravado em: {args.out}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
