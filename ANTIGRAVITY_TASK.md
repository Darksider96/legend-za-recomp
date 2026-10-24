# TAREFAS PARA O ANTIGRAVITY — Lote 2

A tarefa anterior (verificação de segurança) foi aceita. Faça a **Tarefa A** e depois a **Tarefa B**, nessa ordem.
Entregue um relatório por tarefa com a saída dos comandos de aceite.

## Regras (valem para as duas)
- Crie **somente** os arquivos listados em cada tarefa. Não edite nenhum outro arquivo, inclusive `CMakeLists.txt`,
  `.gitignore`, o manifesto e tudo em `recomp/src`, `recomp/include`, `za_port/` e `analysis/`.
  Se achar que outro arquivo precisa mudar, escreva isso no relatório em vez de mudar.
- Se o mesmo erro acontecer duas vezes seguidas, **pare e relate**. Não tente de novo em loop.
- Não faça commit nem push.
- Toda saída de build/geração vai para `build/` (já ignorado pelo git), nunca para dentro de `recomp/` ou `za_port/`.

---

## Tarefa A — `tools/run_all_tests.bat`

Script que compila e testa os dois projetos CMake do repositório e diz no fim se está tudo verde.

1. Colocar no início do `PATH` o toolchain WinLibs:
   `%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin`
   (é ele que tem `cmake.exe`, `ninja.exe` e `g++`; o cmake do devkitPro não serve).
2. Para cada projeto, configurar com `-G Ninja -DCMAKE_BUILD_TYPE=Release`, compilar e rodar `ctest --output-on-failure`:
   - `recomp/` em `build/recomp`
   - `za_port/` em `build/za_port`
3. Imprimir um resumo final com uma linha por projeto (`recomp: OK` ou `recomp: FALHOU`, idem `za_port`) e sair com
   código 0 só se os dois passarem. Uma falha num projeto não deve impedir o outro de rodar.
4. Rodar a partir da raiz do repositório (`tools\run_all_tests.bat`).

**Aceite:** `tools\run_all_tests.bat` termina com os dois `OK` e código de saída 0. Cole o resumo final.

---

## Tarefa B — `recomp/tools/gen_import_table.py`

Gera, a partir do inventário de importações, uma tabela C++ que a camada HLE vai usar.

- Entrada: o `imports.csv` produzido por `nso_inspect` (colunas `nome,namespace,demangled,tipo,got,resolvido_no_sdk`;
  alguns campos vêm entre aspas porque contêm vírgulas — use o módulo `csv`).
- Uso: `python recomp/tools/gen_import_table.py <imports.csv> --out <pasta>`; gera `<pasta>/import_table.hpp` e
  `<pasta>/import_table.cpp`. Python 3, só biblioteca padrão.
- `import_table.hpp` deve conter exatamente esta interface:
  ```cpp
  #pragma once
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
  ```
- `import_table.cpp`: inclui o header, define `kImports` com uma entrada por linha do CSV, ordenada por `ns` e depois
  por `mangled`, e `kImportCount`. Escapar corretamente `\` e `"` nas strings C++.
- Opção `--self-test`: gera a tabela a partir de um CSV de exemplo embutido no script (incluindo um nome com vírgula e
  um com aspas), confere o conteúdo gerado e sai com 0 só se tudo bater. Não depende de nenhum arquivo externo.

**Aceite** (cole a saída dos três):
```
python recomp/tools/gen_import_table.py --self-test
python recomp/tools/gen_import_table.py analysis/recomp/generated/imports.csv --out build/hle_table
g++ -std=c++20 -Wall -Wextra -fsyntax-only -Ibuild/hle_table build/hle_table/import_table.cpp
```
O segundo deve informar **1001** importações; o terceiro não pode ter erros nem warnings.
Se `analysis/recomp/generated/imports.csv` não existir, gere com:
`build\recomp\nso_inspect.exe exefs\main --sdk exefs\sdk` (depois da Tarefa A).
