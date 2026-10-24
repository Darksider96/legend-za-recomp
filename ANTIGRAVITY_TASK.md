# TAREFA PARA O ANTIGRAVITY — Verificação de segurança do repositório

## Contexto
O repositório `legend-za-recomp` vai para o GitHub. A regra 5 do `ORCHESTRATION_MANIFEST.md` proíbe versionar
material do jogo. O `.gitignore` já cobre isso, mas precisamos de uma checagem automática antes de cada commit.

## Arquivos que você pode criar (e SOMENTE estes)
1. `tools/check_repo_safety.py`
2. `tools/install_hooks.bat`

**Não edite nenhum outro arquivo**: nem `.gitignore`, nem o manifesto, nem nada em `recomp/`, `za_port/` ou `analysis/`.
Se achar que outro arquivo precisa mudar, escreva isso no relatório final em vez de mudar.

## O que `tools/check_repo_safety.py` deve fazer
- Python 3, só biblioteca padrão. Rodar a partir da raiz do repositório.
- Obter a lista de arquivos com `git ls-files --cached` (arquivos rastreados e já no stage).
- Falhar (código de saída 1) e listar o motivo de cada arquivo que bater em qualquer regra:
  1. Extensões proibidas: `.nsp .xci .keys .bin .bfbs .bnk .pck .wem .bntx .trmdl .trmsh .trmbf .trcol .trscn .trpak .trpfs .trpfd .dll .exe`
  2. Caminho começando com `romfs/`, `exefs/` ou `recomp/out/`, ou contendo `/generated/`
  3. Arquivo maior que 5 MB
  4. Arquivo de texto (`.txt .c .cpp .h .hpp .md .py .json`) contendo padrões típicos do decompilador do Ghidra:
     `param_1`, `uVar1`, `undefined8`, `FUN_71`, `local_10` (ler como texto UTF-8 ignorando erros)
- Sem nenhum problema: imprimir `OK: N arquivos verificados` e sair com 0.
- Opção `--self-test`: testar as funções de regra com uma lista de exemplos embutida no próprio script (sem tocar
  no git), por exemplo `romfs/a.bin` deve falhar, `recomp/src/nso.cpp` deve passar, um texto com `param_1` deve
  falhar. Imprimir cada caso e sair com 0 só se todos baterem com o esperado.

## O que `tools/install_hooks.bat` deve fazer
- Criar `.git/hooks/pre-commit` (sem extensão) que execute `python tools/check_repo_safety.py` e bloqueie o commit
  se o código de saída for diferente de 0.
- Se o hook já existir, não sobrescrever: avisar e sair.

## Critério de aceite (rode e cole a saída no relatório)
```
python tools/check_repo_safety.py --self-test
python tools/check_repo_safety.py
```
O primeiro deve sair com 0 com todos os casos corretos. O segundo deve sair com 0 no repositório atual.

## Regras
- Se o mesmo erro acontecer duas vezes seguidas, **pare e relate**. Não tente de novo em loop.
- Não faça commit nem push. Ao terminar, escreva um relatório curto: arquivos criados e saída dos dois comandos.
