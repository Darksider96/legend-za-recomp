# TAREFA PARA O CODEX — Censo do kernel e dos serviços usados pelo sdk (planejamento da R5)

## Contexto
As tabelas de salto foram aceitas (27.860 entradas, todas dentro da função do `BR`, conferido). Mudança de
arquitetura (ver seção 6 do `ORCHESTRATION_MANIFEST.md`): o **sdk também é recompilado** e a fronteira nativa fica
nas **SVCs** (chamadas ao kernel do Horizon) e nos **serviços** acessados por IPC (`sm:`, `fsp-srv`, `hid`,
`appletOE`, `vi:m`, `nvdrv`, `audren:u`...). Para planejar a R5 precisamos saber quais SVCs e serviços o jogo usa e
quais APIs `nn::` levam até eles.

Entradas novas em `analysis/recomp/generated/sdk/` (geradas por `nso_inspect exefs/sdk`): `exports.csv` (23.000
exportações), `functions.csv` (32.097 funções), `imports.csv`, `relocations.csv`, `layout.csv`, `image.bin`.
Para o main: os CSVs de sempre em `analysis/recomp/generated/` (inclusive `image.bin`, `plt_map.csv`, `imports.csv`).
Capstone no `build\venv`.

## Arquivos que você pode criar (e SOMENTE estes)
- `analysis/recomp/kernel_census.py`
- `analysis/recomp/kernel_census_report.md`
- Saídas CSV **somente** em `analysis/recomp/generated/`
Mesma regra de conteúdo: o `.md` só tem contagens, RVAs, mnemônicos, nomes de SVCs/serviços e nomes de
importações/exportações (demangled pode).

## Parte 1 — SVCs
- Todas as instruções `svc #imm` no sdk e no main: módulo, RVA, número da SVC, função que contém (RVA) e o nome da
  exportação dessa função quando existir (no sdk, quase toda função `svc` é um wrapper exportado como
  `nn::svc::...`).
- Saída `generated/svc_sites.csv`; no relatório: tabela por número de SVC (nome do Horizon, ex. 0x29 = GetInfo) com
  quantidade de sites e nomes dos wrappers.

## Parte 2 — Nomes de serviço
- Procurar no `.rodata` do sdk (e do main) os nomes de serviço do `sm` (strings ASCII curtas terminadas em zero,
  como `fsp-srv`, `hid`, `appletOE`, `vi:m`, `nvdrv`, `audren:u`, `set:sys`, `time:u`, `acc:u0`, `nifm:u`,
  `bsd:u`, `ssl`, `pctl:a`, `aoc:u`, `friend:u`, `prepo:u`, `bcat:u`, `nim`, ...). Use uma lista conhecida de serviços
  do Horizon + strings no formato `[a-z0-9:-]{2,8}` referenciadas por `ADRP+ADD` no código.
- Para cada nome: as funções que o referenciam (RVA e exportação mais próxima/contida).
- Saída `generated/service_names.csv` (`modulo,string_rva,nome,funcao_rva,exportacao`).

## Parte 3 — Do main até o kernel
- Para cada importação do main que é **função** e resolve para uma exportação do sdk (pelo nome): percorrer o grafo
  de chamadas diretas (`BL`/`B` para fora) dentro do sdk a partir dela, até profundidade 12, e registrar quais SVCs
  (Parte 1) e quais nomes de serviço (Parte 2) são alcançáveis.
- Saída `generated/import_kernel_reach.csv` (`importacao,svcs,servicos,funcoes_visitadas`).
- No relatório: para os 30 namespaces `nn::` com mais chamadas diretas (use `analysis/recomp/hle_priority.md` ou
  `call_targets.csv` + `plt_map.csv`), os serviços e SVCs alcançáveis; e a lista de importações que **não** alcançam
  nenhuma SVC (são puras — o código recompilado do sdk já basta).

## Autoverificações (o script deve conferir e imprimir)
1. Toda instrução `svc` encontrada fica dentro de uma função conhecida do seu módulo (ou é listada como exceção).
2. Cada SVC da Parte 1 tem nome do Horizon atribuído (ou é marcada `desconhecida`).
3. As 917 importações com stub do main: quantas resolvem para exportação do sdk pelo nome, quantas não (listar as que não).
4. Contagens das partes batem com os CSVs gravados.

## Critério de aceite
```
build\venv\Scripts\python.exe -I analysis\recomp\kernel_census.py
python tools\check_repo_safety.py
```

## Regras
- Se o mesmo erro acontecer duas vezes seguidas, **pare e relate**. Não tente de novo em loop.
- Não faça commit nem push.
