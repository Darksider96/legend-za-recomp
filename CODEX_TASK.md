# TAREFA PARA O CODEX — Descoberta de funções fora do `.eh_frame` (apoio à R4)

## Contexto
O censo anterior (`analysis/recomp/control_flow_report.md`, aceito) mostrou 17 alvos de `BL`/`B` que caem fora de
qualquer função do `.eh_frame`, todos na faixa **0x1d92734–0x1dcacd0** (~229 KB sem FDE; provável assembly escrito à
mão, que não gera unwind). O `.eh_frame` cobre 98,86% do `.text`: ~572 KB ficam em lacunas. O recompilador só traduz
funções com início e tamanho conhecidos; um alvo de chamada sem função vira símbolo indefinido no link da R4.

Objetivo: propor inícios e tamanhos de funções para as lacunas, num CSV que o recompilador vai consumir.

Entradas (as mesmas da tarefa anterior, não versionadas): `analysis/boot/text.bin`,
`analysis/recomp/generated/functions.csv`, `imports.csv`, `exports.csv`, e as saídas do seu censo
(`call_targets.csv`, `plt_map.csv`, `indirect_branches.csv`). Capstone no `build\venv`.

## Arquivos que você pode criar (e SOMENTE estes)
- `analysis/recomp/gap_functions.py`
- `analysis/recomp/gap_functions_report.md`
- Saídas CSV **somente** em `analysis/recomp/generated/`

Não edite nenhum outro arquivo (inclusive `control_flow_census.py`; pode importar funções dele se quiser).
Mesma regra de conteúdo da tarefa anterior: o `.md` só tem contagens, RVAs, mnemônicos e nomes de importações/
exportações — nada de listagens de instruções ou pseudocódigo.

## Parte 1 — Inventário das lacunas
- Listar todas as lacunas entre funções do `.eh_frame` dentro do `.text` (excluindo a PLT, já mapeada).
- Classificar cada lacuna: `zeros/padding` (só zeros, `udf` ou `nop`), `dados` (maioria não decodificável ou
  sem nenhum terminador de fluxo), `codigo` (decodifica e tem terminadores `ret`/`b`/`br`).
- Saída: `generated/gaps.csv` com `inicio,tamanho,classe,alvos_de_chamada,exports_dentro`.
- No relatório: contagem e bytes por classe; as 10 maiores lacunas de código (RVA e tamanho).

## Parte 2 — Inícios e tamanhos propostos
Para cada lacuna de `codigo`, propor funções:
- **Inícios**, em ordem de confiança: alvos de `BL`/tail call (`call_targets.csv`, classe `lacuna`), exportações
  dentro da lacuna, primeiro endereço após padding/fim de função anterior, endereços carregados por `ADR`/`ADRP+ADD`
  em código conhecido que caem na lacuna (prováveis ponteiros de função).
- **Fim**: o próximo início proposto ou o início da próxima função do `.eh_frame`, cortando padding no final.
- Também propagar: alvos de `BL`/`B` feitos **de dentro** das funções propostas que caem em outras lacunas viram novos
  inícios (repetir até estabilizar).
- Saída: `generated/gap_functions.csv` com `rva,tamanho,origem` (origem = `alvo_bl`, `export`, `sequencia`, `adr`,
  `propagado`; se houver mais de uma, a de maior confiança).

## Autoverificações (o script deve conferir e imprimir)
1. Os 17 alvos `lacuna` do censo são todos inícios em `gap_functions.csv`.
2. Nenhuma função proposta sobrepõe função do `.eh_frame`, a PLT ou outra proposta; todas alinhadas a 4 bytes.
3. Toda função proposta termina num terminador de fluxo (`ret`, `b`, `br`, `udf`/`brk`) ou exatamente no início
   seguinte (fall-through declarado — conte quantas são assim).
4. Depois de somar as propostas às funções do `.eh_frame`, refazer a classificação da Parte 2 do censo: o número de
   alvos `lacuna` deve cair para **0** (ou cada restante explicado no relatório).

## Critério de aceite (cole a saída no relatório final)
```
build\venv\Scripts\python.exe -I analysis\recomp\gap_functions.py
python tools\check_repo_safety.py
```
O primeiro mostra as quatro autoverificações; o segundo continua `OK` com o relatório rastreado (`git add -N`).

## Regras
- Se o mesmo erro acontecer duas vezes seguidas, **pare e relate**. Não tente de novo em loop.
- Não faça commit nem push. Ao terminar, relatório curto: arquivos criados, números principais e saída dos comandos.
