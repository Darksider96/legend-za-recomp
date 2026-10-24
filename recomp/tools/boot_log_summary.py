#!/usr/bin/env python3
"""
Resume o log de execucao do jogo recompilado (za_recomp.log).
Conta linhas por etiqueta ([loader], [rtld], [svc], etc.),
lista chamadas de SVC e extrai o motivo da parada e registradores.

Uso:
  python recomp/tools/boot_log_summary.py [za_recomp.log]
  python recomp/tools/boot_log_summary.py --self-test
"""

import argparse
import io
import re
import sys
from collections import defaultdict
from typing import Dict, List, Optional, Tuple

STOP_PATTERNS = [
    "nao implementada",
    "sem definicao",
    "nao traduzida",
    "excecao",
    "BRK/UDF",
    "nao e inicio de funcao",
]


def parse_boot_log(lines: List[str]) -> Tuple[Dict[str, int], Dict[str, int], Optional[List[str]]]:
    """
    Analisa as linhas do log do za_recomp.
    Retorna:
      - tag_counts: contagem de linhas por etiqueta [tag]
      - svc_counts: contagem de chamadas por nome de SVC
      - stop_block: lista de linhas do motivo da parada e contexto (ou None)
    """
    tag_counts: Dict[str, int] = defaultdict(int)
    svc_counts: Dict[str, int] = defaultdict(int)
    stop_block: Optional[List[str]] = None

    tag_regex = re.compile(r"^\[([a-zA-Z0-9_-]+)\]")
    svc_id_name_regex = re.compile(r"^\[svc\]\s+(?:0x[0-9a-fA-F]+\s+)?([a-zA-Z0-9_]+)")

    i = 0
    n = len(lines)
    while i < n:
        raw_line = lines[i]
        line = raw_line.rstrip("\r\n")

        # Verifica etiqueta [tag]
        m_tag = tag_regex.match(line)
        if m_tag:
            tag = f"[{m_tag.group(1)}]"
            tag_counts[tag] += 1

            # Se for [svc], extrai o nome da SVC
            if tag == "[svc]":
                m_svc = svc_id_name_regex.match(line)
                if m_svc:
                    svc_name = m_svc.group(1)
                    svc_counts[svc_name] += 1

        # Verifica motivo da parada (primeira ocorrencia)
        if stop_block is None:
            for pat in STOP_PATTERNS:
                if pat in line:
                    block = [line]
                    j = i + 1
                    while j < n and lines[j].startswith("  "):
                        block.append(lines[j].rstrip("\r\n"))
                        j += 1
                    stop_block = block
                    break

        i += 1

    return dict(tag_counts), dict(svc_counts), stop_block


def format_summary(
    tag_counts: Dict[str, int],
    svc_counts: Dict[str, int],
    stop_block: Optional[List[str]],
) -> str:
    """Formata o resumo em texto legivel."""
    out = []
    out.append("=== Resumo do Log de Boot (za_recomp) ===")
    out.append("")
    out.append("Linhas por etiqueta:")
    if tag_counts:
        # Ordena por contagem decrescente, depois nome
        for tag, count in sorted(tag_counts.items(), key=lambda x: (-x[1], x[0])):
            out.append(f"  {tag}: {count}")
    else:
        out.append("  (nenhuma etiqueta encontrada)")

    if svc_counts:
        out.append("")
        out.append("Chamadas de SVC:")
        for svc, count in sorted(svc_counts.items(), key=lambda x: (-x[1], x[0])):
            out.append(f"  {svc}: {count}")

    out.append("")
    out.append("Motivo da parada:")
    if stop_block:
        for blk_line in stop_block:
            out.append(f"  {blk_line}")
    else:
        out.append("  boot: sem parada registrada")

    return "\n".join(out)


def run_self_test() -> int:
    """Auto-teste em memoria."""
    sample_log = [
        "[loader] main em 0x7100000000, sdk em 0x7108000000",
        "[rtld] pronto",
        "[boot] nn::init::Start em 0x7108001000",
        "[svc] OutputDebugString: inicializando subsistemas",
        "[svc] 0x29 GetInfo nao implementada (x0=0 x1=1 x2=2 x3=3)",
        "  funcao nativa: rtld:init",
        "  LR = sdk+0x1234",
        "  x0..x7 = 0 1 2 3 4 5 6 7",
    ]

    tag_counts, svc_counts, stop_block = parse_boot_log(sample_log)

    print("[SELF-TEST] Etiquetas encontradas:", tag_counts)
    assert tag_counts.get("[loader]") == 1
    assert tag_counts.get("[rtld]") == 1
    assert tag_counts.get("[boot]") == 1
    assert tag_counts.get("[svc]") == 2
    print("[SELF-TEST] Contagem de etiquetas: OK")

    print("[SELF-TEST] SVCs encontradas:", svc_counts)
    assert svc_counts.get("OutputDebugString") == 1
    assert svc_counts.get("GetInfo") == 1
    print("[SELF-TEST] Contagem de SVCs: OK")

    print("[SELF-TEST] Bloco de parada:", stop_block)
    assert stop_block is not None
    assert len(stop_block) == 4
    assert "[svc] 0x29 GetInfo nao implementada" in stop_block[0]
    assert "  funcao nativa: rtld:init" in stop_block[1]
    assert "  LR = sdk+0x1234" in stop_block[2]
    assert "  x0..x7 = 0 1 2 3 4 5 6 7" in stop_block[3]
    print("[SELF-TEST] Bloco de parada e registradores: OK")

    # Caso sem parada registrada
    clean_sample = [
        "[loader] pronto",
        "[boot] sucesso",
    ]
    _, _, clean_stop = parse_boot_log(clean_sample)
    formatted = format_summary({}, {}, clean_stop)
    assert "boot: sem parada registrada" in formatted
    print("[SELF-TEST] Caso sem parada: OK")

    print("[SELF-TEST] Todos os testes passaram com sucesso.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Resume o log de execucao do za_recomp")
    parser.add_argument("log_file", nargs="?", default="za_recomp.log", help="Caminho do za_recomp.log (padrao: za_recomp.log)")
    parser.add_argument("--self-test", action="store_true", help="Executa auto-teste embutido")

    args = parser.parse_args()

    if args.self_test:
        return run_self_test()

    try:
        with open(args.log_file, "r", encoding="utf-8", errors="replace") as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"Erro: arquivo de log '{args.log_file}' nao encontrado.", file=sys.stderr)
        return 1

    tag_counts, svc_counts, stop_block = parse_boot_log(lines)
    summary = format_summary(tag_counts, svc_counts, stop_block)
    print(summary)
    return 0


if __name__ == "__main__":
    sys.exit(main())
