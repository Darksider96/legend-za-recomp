# TAREFA PARA O CODEX — Levantamento do fluxo de controle para o recompilador (apoio à R3/R4)

## Contexto
O projeto agora é um **recomp** (ver `ORCHESTRATION_MANIFEST.md`). O tradutor já gera C++ para funções do `main` e
precisa de três informações que ainda não temos: quais alvos de `BL` são stubs da PLT (chamadas ao SDK), quais
alvos de chamada não começam uma função do `.eh_frame`, e como o jogo usa saltos indiretos (`BR`).

Entradas (já existem na sua máquina, não versionadas):
- `analysis/boot/text.bin` — `.text` do `main` (RVA 0 = início do arquivo; idêntico ao extraído do NSO)
- `analysis/recomp/generated/functions.csv` — `rva,tamanho` das 222.376 funções do `.eh_frame`
- `analysis/recomp/generated/imports.csv` — importações; a coluna `got` traz o(s) RVA(s) do slot GOT de cada uma
Se os CSVs não existirem: `build\recomp\nso_inspect.exe exefs\main --sdk exefs\sdk` (ou compile `recomp/` antes).

Disassembler: Capstone em um venv isolado (`python -m venv build\venv` e `build\venv\Scripts\pip install capstone`).

## Arquivos que você pode criar (e SOMENTE estes)
- `analysis/recomp/control_flow_census.py` — script único que faz as três partes abaixo
- `analysis/recomp/control_flow_report.md` — relatório com os números (gerado pelo script ou escrito a partir dele)
- Saídas CSV detalhadas **somente** em `analysis/recomp/generated/` (pasta não versionada)

Não edite nenhum outro arquivo (nada em `recomp/`, `za_port/`, `tools/`, manifesto, `.gitignore`).

## Regra de conteúdo (obrigatória)
O relatório `.md` vai para o GitHub: pode conter **contagens, endereços (RVA), mnemônicos e nomes de importações**.
**Não pode** conter listagens de instruções, trechos desmontados ou pseudocódigo descompilado do jogo — esses ficam
só nos CSVs de `generated/`. O hook `tools/check_repo_safety.py` barra os padrões do Ghidra; não tente contorná-lo.

## Parte 1 — Mapa PLT → importação
- Os stubs da PLT do AArch64 ficam no fim do `.text` e seguem o padrão de 16 bytes
  `adrp x16, página` / `ldr x17, [x16, #off]` / `add x16, x16, #off` / `br x17`.
- Para cada stub: calcular o RVA do slot GOT que ele lê e cruzar com a coluna `got` do `imports.csv`.
- Saída: `generated/plt_map.csv` com `stub_rva,got_rva,import`.
- No relatório: quantos stubs, faixa de RVAs, quantos mapeados.
- **Invariante de aceite:** o número de stubs mapeados deve ser **917**, igual ao número de relocações JUMP_SLOT do
  `nso_inspect`. Se não bater, investigue e explique a diferença no relatório (não ajuste o número à força).

## Parte 2 — Alvos de chamada e de salto fora das funções conhecidas
- Percorrer todas as instruções de todas as funções do `functions.csv` e coletar alvos diretos de `BL` e de `B`
  que saem da própria função (tail calls).
- Classificar cada alvo distinto: (a) início de função do `.eh_frame`; (b) stub da PLT (Parte 1); (c) meio de uma
  função conhecida; (d) fora de qualquer função (lacuna do `.eh_frame`).
- Saída: `generated/call_targets.csv` com `alvo_rva,classe,quantidade_de_chamadas,exemplo_de_origem_rva`.
- No relatório: total de `BL`, total de tail calls, contagem de alvos por classe e as 20 lacunas mais chamadas (só RVA
  e quantidade).

## Parte 3 — Saltos indiretos
- Contar `BR` (exceto `ret`) e `BLR` no `.text` inteiro e por função.
- Para cada `BR`, verificar se ele termina uma sequência típica de tabela de `switch` (carga de entrada de tabela
  indexada por registrador, por exemplo `ldrb/ldrh/ldrsw ... [xN, xM, ...]` seguida de `add` e `br` nas instruções
  anteriores); classificar como `tabela_de_salto`, `x17_plt`, `outro`.
- Para as tabelas detectadas, tentar descobrir o endereço base e o número de entradas (pelo `cmp`/`b.hi` que limita
  o índice) e gravar em `generated/jump_tables.csv` (`br_rva,base_rva,entradas,tamanho_entrada`).
- No relatório: totais por classe e quantas tabelas tiveram base e tamanho resolvidos.

## Critério de aceite (cole a saída no relatório final)
```
build\venv\Scripts\python.exe -I analysis\recomp\control_flow_census.py
python tools\check_repo_safety.py
```
O primeiro gera os três CSVs e o relatório; o segundo continua `OK` com o relatório rastreado (`git add -N` nele
antes de rodar). O mapa PLT tem 917 stubs mapeados (ou a diferença explicada).

## Regras
- Se o mesmo erro acontecer duas vezes seguidas, **pare e relate**. Não tente de novo em loop.
- Não faça commit nem push. Ao terminar, escreva um relatório curto: arquivos criados, os números principais das três
  partes e a saída dos comandos de aceite.
