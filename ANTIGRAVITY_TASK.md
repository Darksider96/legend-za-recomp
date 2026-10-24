# TAREFA PARA O ANTIGRAVITY — Lote 6: rodar o jogo recompilado e resumir o log de boot

O lote 5 foi aceito (`hle_priority`, 130.557 chamadas conferidas). O runtime do jogo recompilado já existe
(`recomp/runtime/`, executável `za_recomp`, opção CMake `-DRECOMP_GAME=ON`). O build é longo (~20–30 min: traduz
main e sdk inteiros e compila ~150 arquivos grandes). Esta tarefa cria duas ferramentas para rodar e ler o boot.

## Regras
- Crie **somente** os arquivos listados. Não edite nada em `recomp/` além do arquivo `.py` desta tarefa, nem
  `CMakeLists.txt`, `.gitignore`, manifesto, `za_port/`, `analysis/`.
- **Não** use `build/recomp` nem `build/claude-recomp` (são de outros processos); use `build/game`.
- Se o mesmo erro acontecer duas vezes seguidas, **pare e relate**. Não faça commit nem push.

## Tarefa A — `tools/run_game.bat`
1. Mesmo `PATH` do WinLibs do `run_all_tests.bat`.
2. Configura `build/game` a partir de `recomp/` com `-G Ninja -DCMAKE_BUILD_TYPE=Release -DRECOMP_GAME=ON`
   (aceita um argumento opcional `--jobs N` que vira `-DRECOMP_GUEST_JOBS=N`; padrão 6).
3. Compila só o alvo `za_recomp` (`cmake --build build/game --target za_recomp`). Se falhar, imprime
   `run_game: BUILD FALHOU` e sai com 1.
4. Roda `build\game\za_recomp.exe exefs` **a partir da raiz do repositório** (o log `za_recomp.log` é criado na pasta
   atual), guarda o código de saída e imprime as últimas 30 linhas de `za_recomp.log`.
5. Imprime `run_game: saida N` e sai com o mesmo código do jogo. (Na fase atual o jogo para de propósito na primeira
   SVC não implementada, com código 3 — isso é o esperado, não é erro do script.)

## Tarefa B — `recomp/tools/boot_log_summary.py`
`python recomp/tools/boot_log_summary.py [za_recomp.log]` (padrão `za_recomp.log`), só biblioteca padrão.
- Conta as linhas por etiqueta inicial entre colchetes (`[loader]`, `[rtld]`, `[boot]`, `[svc]`, `[host]`, ...) e
  imprime a contagem.
- Para `[svc]`: conta por nome de SVC (o texto depois do número hexadecimal, ex. `0x29 GetInfo`).
- Encontra o **motivo da parada**: a primeira linha que contém `nao implementada`, `sem definicao`,
  `nao traduzida`, `excecao`, `BRK/UDF` ou `nao e inicio de funcao`, e imprime essa linha e as linhas seguintes que
  começam com dois espaços (`  LR = ...`, `  x0..x7 = ...`, `  funcao nativa: ...`).
- Se não houver motivo de parada, imprime `boot: sem parada registrada`.
- `--self-test`: monta um log de exemplo em memória (com loader, rtld, duas SVCs, uma parada com LR e x0..x7) e confere
  contagens e o bloco de parada. Sai 0 só se tudo bater.

## Aceite (cole a saída)
```
python recomp/tools/boot_log_summary.py --self-test
tools\run_game.bat
python recomp/tools/boot_log_summary.py za_recomp.log
```
O segundo pode terminar com `run_game: saida 3` (parada esperada). Cole o resumo do terceiro — ele é a primeira
leitura do boot do jogo e vai orientar a R5.
