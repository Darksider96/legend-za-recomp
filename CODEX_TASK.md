# TAREFA PARA O CODEX — Alvos indiretos: ponteiros para o `.text` (apoio à tabela de despacho da R4)

## Contexto
As duas tarefas anteriores foram aceitas (censo de fluxo de controle e funções das lacunas). Na R4, todo `BLR`/`BR`
passa por `recomp::Dispatch(endereço)`, que precisa achar a função traduzida daquele endereço. São 178.853 `BLR`:
os destinos vêm de vtables, callbacks e ponteiros de função gravados nos dados (relocados no carregamento) ou montados
no código com `ADR`/`ADRP+ADD`. Precisamos saber quais endereços do `.text` podem ser destino indireto e se todos são
inícios de função conhecidos.

Entradas novas (geradas por `build\claude-recomp\nso_inspect.exe exefs\main --sdk exefs\sdk`, já em
`analysis/recomp/generated/`):
- `relocations.csv` — `rva,tipo,segmento,alvo_rva,simbolo`; `tipo` em `relr`, `relative`, `abs64`, `glob_dat`,
  `jump_slot`. Nas relativas, `alvo_rva` é o RVA para onde o ponteiro aponta; nas demais, `simbolo` é a importação.
- `layout.csv` — `nome,rva,tamanho` de `.text`, `.rodata`, `.data`, `.bss` e `init_array`.
Mais as de antes: `functions.csv`, `gap_functions.csv`, `plt_map.csv`, `analysis/boot/text.bin`. Capstone no `build\venv`.

## Arquivos que você pode criar (e SOMENTE estes)
- `analysis/recomp/indirect_targets.py`
- `analysis/recomp/indirect_targets_report.md`
- Saídas CSV **somente** em `analysis/recomp/generated/`

Mesma regra de conteúdo: o `.md` só tem contagens, RVAs, mnemônicos e nomes de importações/exportações.

## Parte 1 — Ponteiros relocados para o `.text`
- Para cada relocação relativa (`relr`/`relative`) com `alvo_rva` dentro do `.text`, classificar o alvo:
  `inicio_funcao` (`.eh_frame` ou `gap_functions.csv`), `stub_plt`, `meio_funcao`, `fora` (lacuna/padding).
- Saída: `generated/text_pointers.csv` com `slot_rva,segmento,alvo_rva,classe`.
- No relatório: total por classe e por segmento do slot; alvos distintos; os 20 alvos `meio_funcao` mais referenciados
  (RVA, função que os contém e deslocamento dentro dela).

## Parte 2 — INIT_ARRAY
- O `init_array` (ver `layout.csv`) tem **1.912** entradas de 8 bytes. Conferir que cada slot tem uma relocação relativa
  e que todos os alvos são `inicio_funcao`. É invariante de aceite: se algum não for, explique no relatório.

## Parte 3 — Candidatas a vtable
- Sequências máximas de slots consecutivos (passo de 8 bytes) em `.rodata`/`.data` em que todos são relocações
  relativas para o `.text`. Saída `generated/vtable_candidates.csv` com `inicio_rva,entradas,segmento`.
- No relatório: quantidade de sequências, histograma de tamanhos (1, 2–3, 4–7, 8–15, 16–63, 64+) e a maior.

## Parte 4 — Endereços do `.text` montados no código
- Em todas as funções (`.eh_frame` + lacunas), encontrar `ADR` e `ADRP`+`ADD` (mesmo registrador, até 12 instruções
  adiante, sem sobrescrita nem transferência de controle no meio) cujo resultado cai no `.text`, excluindo os casos em
  que o registrador é usado logo em seguida só como base de load (tabelas de salto/literais).
- Classificar como na Parte 1. Saída `generated/code_text_addresses.csv` com `origem_rva,alvo_rva,classe`.

## Parte 5 — Conjunto de despacho
- União dos alvos das Partes 1 e 4 com as exportações de função: é a estimativa do conjunto mínimo da tabela de
  despacho. No relatório: tamanho do conjunto, quantos são `inicio_funcao` e quantos não (por classe).

## Autoverificações (o script deve conferir e imprimir)
1. O total de linhas de `relocations.csv` é **350.904** (346.629 `relr` + 3.303 `abs64` + 917 `jump_slot` + 55 `glob_dat`).
2. INIT_ARRAY: 1.912 slots relocados, todos `inicio_funcao` (ou a diferença explicada).
3. Todo `alvo_rva` relativo cai dentro da imagem (`.text` a `.bss`); contar quantos apontam para cada segmento.
4. As somas por classe de cada parte batem com os totais.

## Critério de aceite (cole a saída no relatório final)
```
build\venv\Scripts\python.exe -I analysis\recomp\indirect_targets.py
python tools\check_repo_safety.py
```

## Regras
- Se o mesmo erro acontecer duas vezes seguidas, **pare e relate**. Não tente de novo em loop.
- Não faça commit nem push. Ao terminar, relatório curto: arquivos criados, números principais e saída dos comandos.
