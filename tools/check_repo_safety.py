#!/usr/bin/env python3
"""
tools/check_repo_safety.py - Verificação de segurança pré-commit do repositório legend-za-recomp.
Garante que nenhum asset do jogo, binário proibido ou código descompilado do Ghidra seja versionado.
"""

import os
import sys
import subprocess

# 1. Extensões proibidas
FORBIDDEN_EXTENSIONS = {
    ".nsp", ".xci", ".keys", ".bin", ".bfbs", ".bnk", ".pck", ".wem",
    ".bntx", ".trmdl", ".trmsh", ".trmbf", ".trcol", ".trscn", ".trpak",
    ".trpfs", ".trpfd", ".dll", ".exe"
}

# 2. Prefixos e fragmentos de caminhos proibidos
FORBIDDEN_PATH_PREFIXES = ("romfs/", "exefs/", "recomp/out/")
FORBIDDEN_PATH_FRAGMENTS = ("/generated/",)

# 3. Limite máximo de tamanho por arquivo (5 MB)
MAX_FILE_SIZE_BYTES = 5 * 1024 * 1024  # 5,242,880 bytes

# 4. Extensões de texto sujeitas à verificação de padrões do Ghidra
TEXT_EXTENSIONS = {".txt", ".c", ".cpp", ".h", ".hpp", ".md", ".py", ".json"}

# Padrões característicos gerados pelo descompilador do Ghidra
GHIDRA_PATTERNS = ["param_1", "uVar1", "undefined8", "FUN_71", "local_10"]

# Arquivos que citam os padroes acima por definicao (este script e a especificacao dele): sem isso, versionar o
# proprio verificador bloquearia todo commit pelo hook de pre-commit
CONTENT_SCAN_EXEMPT = {"tools/check_repo_safety.py", "ANTIGRAVITY_TASK.md"}


def normalize_path(path: str) -> str:
    """Normaliza o caminho com barras comuns e remove prefixos relativos."""
    p = path.replace("\\", "/").strip()
    if p.startswith("./"):
        p = p[2:]
    return p


def check_file_rules(rel_path: str, abs_path: str = None,
                     content_override: str = None,
                     size_override: int = None) -> list:
    """
    Verifica um arquivo individual contra todas as regras de segurança.
    Retorna uma lista de strings descrevendo as violações encontradas (vazia se válido).
    """
    violations = []
    norm = normalize_path(rel_path)
    _, ext = os.path.splitext(norm)
    ext_lower = ext.lower()

    # Regra 1: Extensões proibidas
    if ext_lower in FORBIDDEN_EXTENSIONS:
        violations.append(f"Extensao proibida: {ext_lower}")

    # Regra 2: Caminhos proibidos
    if norm.startswith(FORBIDDEN_PATH_PREFIXES):
        matched_prefix = next(p for p in FORBIDDEN_PATH_PREFIXES if norm.startswith(p))
        violations.append(f"Caminho proibido (comeca com '{matched_prefix}')")

    for frag in FORBIDDEN_PATH_FRAGMENTS:
        if frag in norm or norm.startswith(frag.lstrip("/")) or norm.endswith(frag.rstrip("/")):
            violations.append(f"Caminho proibido (contem '{frag}')")

    # Regra 3: Limite de tamanho (5 MB)
    file_size = None
    if size_override is not None:
        file_size = size_override
    elif abs_path and os.path.isfile(abs_path):
        try:
            file_size = os.path.getsize(abs_path)
        except OSError:
            pass

    if file_size is not None and file_size > MAX_FILE_SIZE_BYTES:
        mb = file_size / (1024 * 1024)
        violations.append(f"Arquivo maior que 5 MB ({mb:.2f} MB)")

    # Regra 4: Padrões típicos do decompilador do Ghidra em arquivos de texto
    if ext_lower in TEXT_EXTENSIONS and norm not in CONTENT_SCAN_EXEMPT:
        text = None
        if content_override is not None:
            text = content_override
        elif abs_path and os.path.isfile(abs_path):
            try:
                with open(abs_path, "r", encoding="utf-8", errors="ignore") as f:
                    text = f.read()
            except OSError:
                pass

        if text:
            for pattern in GHIDRA_PATTERNS:
                if pattern in text:
                    violations.append(f"Contem padrao do decompilador Ghidra: '{pattern}'")

    return violations


def run_self_test() -> int:
    """
    Executa o auto-teste das regras de segurança com casos de teste embutidos.
    Retorna 0 se todos os casos passarem, ou 1 se houver divergência.
    """
    test_cases = [
        # (descricao, rel_path, size_bytes, content, expected_fail, pattern_expected)
        ("Arquivo .nsp proibido", "Pokemon_Legends_ZA.nsp", 100, None, True, "Extensao proibida"),
        ("Arquivo .xci proibido", "game.xci", 100, None, True, "Extensao proibida"),
        ("Arquivo .keys proibido", "prod.keys", 100, None, True, "Extensao proibida"),
        ("Arquivo .bin proibido", "analysis/boot/rodata.bin", 100, None, True, "Extensao proibida"),
        ("Arquivo .dll proibido", "oo2core_9_win64.dll", 100, None, True, "Extensao proibida"),
        ("Arquivo .exe proibido", "za_port.exe", 100, None, True, "Extensao proibida"),
        ("Asset .trmdl proibido", "pm0150.trmdl", 100, None, True, "Extensao proibida"),
        ("Asset .trscn proibido", "field.trscn", 100, None, True, "Extensao proibida"),
        ("Asset .trcol proibido", "t1_g01.trcol", 100, None, True, "Extensao proibida"),
        ("Audio .bnk proibido", "BGM.bnk", 100, None, True, "Extensao proibida"),
        ("Audio .pck proibido", "Stream.pck", 100, None, True, "Extensao proibida"),
        ("Audio .wem proibido", "audio.wem", 100, None, True, "Extensao proibida"),
        ("Caminho romfs/ proibido", "romfs/arc/data.dat", 100, None, True, "comeca com 'romfs/'"),
        ("Caminho exefs/ proibido", "exefs/main", 100, None, True, "comeca com 'exefs/'"),
        ("Caminho recomp/out/ proibido", "recomp/out/build.log", 100, None, True, "comeca com 'recomp/out/'"),
        ("Caminho contendo /generated/ proibido", "analysis/recomp/generated/exports.csv", 100, None, True, "contem '/generated/'"),
        ("Arquivo maior que 5 MB", "large_archive.dat", 6 * 1024 * 1024, None, True, "maior que 5 MB"),
        ("Texto com padrao Ghidra 'param_1'", "src/test.cpp", 100, "void foo(int param_1) {}", True, "param_1"),
        ("Texto com padrao Ghidra 'uVar1'", "src/test.c", 100, "uVar1 = 5;", True, "uVar1"),
        ("Texto com padrao Ghidra 'undefined8'", "include/types.h", 100, "typedef undefined8 u64;", True, "undefined8"),
        ("Texto com padrao Ghidra 'FUN_71'", "decomp.txt", 100, "call FUN_7100116410;", True, "FUN_71"),
        ("Texto com padrao Ghidra 'local_10'", "stack.json", 100, '{"var": "local_10"}', True, "local_10"),
        ("Arquivo valido C++", "recomp/src/nso.cpp", 5000, "#include <iostream>\nint main() { return 0; }", False, None),
        ("Arquivo valido Markdown", "README.md", 2000, "# Projeto legend-za-recomp", False, None),
        ("Arquivo valido CMake", "CMakeLists.txt", 1000, "cmake_minimum_required(VERSION 3.20)", False, None),
        ("Script Python valido", "tools/check_repo_safety.py", 3000, "#!/usr/bin/env python3", False, None),
        ("Verificador isento da busca (cita os padroes)", "tools/check_repo_safety.py", 3000,
         'GHIDRA_PATTERNS = ["param_1", "uVar1"]', False, None),
        ("Mesmo conteudo fora da isencao e barrado", "tools/outro.py", 3000, 'x = ["param_1"]', True, "param_1"),
    ]

    all_passed = True
    print("=== EXECUTANDO AUTO-TESTE (SELF-TEST) ===")
    for desc, path, size, content, should_fail, expected_tag in test_cases:
        violations = check_file_rules(path, size_override=size, content_override=content)
        failed = len(violations) > 0

        ok = (failed == should_fail)
        if ok and should_fail and expected_tag:
            # Garante que o motivo esperado foi mencionado
            ok = any(expected_tag.lower() in v.lower() for v in violations)

        if ok:
            status = "[PASS]"
        else:
            status = "[FAIL]"
            all_passed = False

        detail = f" -> {violations}" if violations else " -> [Valido]"
        print(f"  {status} {desc}: '{path}'{detail}")

    print("=========================================")
    if all_passed:
        print(f"OK: Todos os {len(test_cases)} casos de auto-teste passaram com sucesso.")
        return 0
    else:
        print("FALHA: Um ou mais casos de auto-teste falharam.")
        return 1


def main() -> int:
    if "--self-test" in sys.argv:
        return run_self_test()

    # Executa verificação real via git ls-files --cached
    try:
        result = subprocess.run(
            ["git", "ls-files", "--cached"],
            capture_output=True,
            text=True,
            check=True
        )
    except subprocess.CalledProcessError as e:
        print(f"[ERRO] Falha ao executar 'git ls-files --cached': {e}", file=sys.stderr)
        return 1
    except FileNotFoundError:
        print("[ERRO] Executavel 'git' nao encontrado no PATH.", file=sys.stderr)
        return 1

    tracked_files = [line.strip() for line in result.stdout.splitlines() if line.strip()]

    repo_root = os.getcwd()
    total_checked = 0
    all_violations = []

    for rel_path in tracked_files:
        abs_path = os.path.join(repo_root, rel_path)
        violations = check_file_rules(rel_path, abs_path=abs_path)
        total_checked += 1

        if violations:
            for v in violations:
                all_violations.append((rel_path, v))

    if all_violations:
        print(f"[FALHA DE SEGURANCA] Foram encontradas {len(all_violations)} violacao(oes) no repositorio:")
        for path, reason in all_violations:
            print(f"  - {path}: {reason}")
        print("\nCommit bloqueado: remova os arquivos ou padroes proibidos antes de commitar.")
        return 1

    print(f"OK: {total_checked} arquivos verificados")
    return 0


if __name__ == "__main__":
    sys.exit(main())
