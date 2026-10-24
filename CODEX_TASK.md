# TAREFA PARA O CODEX — Superfície de API da R5 (fs, hid, applet, conta, horário, configurações)

> **Esta é uma tarefa NOVA.** O censo de kernel (`kernel_census*`) já foi aceito e commitado: **não edite**
> `analysis/recomp/kernel_census.py` nem `kernel_census_report.md` (as edições não pedidas que estão na sua cópia de
> trabalho não serão incorporadas). Os únicos arquivos desta tarefa são `r5_api_surface.py` e
> `r5_api_surface_report.md`, descritos abaixo.

## Contexto
O censo de kernel foi aceito (58 SVCs conferidas por varredura independente). Na R5 o runtime vai emular os
serviços do sistema. Para cada área há duas opções: emular o **serviço IPC** (o sdk recompilado fala com ele) ou
**sobrescrever a API `nn::`** do sdk por nome com uma implementação nativa. A escolha depende de quantas funções o
jogo usa, com que argumentos e quais serviços elas alcançam. Esta tarefa levanta esses dados.

Entradas (todas em `analysis/recomp/generated/`): `imports.csv`, `plt_map.csv`, `call_targets.csv`, `image.bin`,
`functions.csv`, `gap_functions.csv` (main); `sdk/*` (sdk); e os CSVs do seu censo de kernel
(`import_kernel_reach.csv`, `service_names.csv`, `svc_sites.csv`). Capstone no `build\venv`.

## Arquivos que você pode criar (e SOMENTE estes)
- `analysis/recomp/r5_api_surface.py`
- `analysis/recomp/r5_api_surface_report.md`
- Saídas CSV **somente** em `analysis/recomp/generated/`
Mesma regra de conteúdo do `.md`: contagens, RVAs, nomes de importações/exportações (demangled), nomes de serviços,
mnemônicos e **strings curtas de caminho/montagem** (ex.: `rom:`, `save:`, `rom:/data/...`). Nada de listagens de
instruções ou pseudocódigo.

## Parte 1 — Importações por área
Áreas: `nn::fs`, `nn::hid`, `nn::oe` + `nn::applet`, `nn::account`, `nn::time`, `nn::settings`, `nn::err`,
`nn::nifm`, `nn::aoc`, `nn::pl`, `nn::vi`, `nn::audio`, `nn::os` (só para referência de volume).
Para cada importação de função do main nessas áreas: nome demangled, chamadas diretas (stub da PLT em
`call_targets.csv`), funções distintas do main que chamam, serviços e SVCs alcançáveis (`import_kernel_reach.csv`).
Saída `generated/r5_imports.csv`; no relatório, uma tabela por área ordenada por chamadas.

## Parte 2 — Montagens e caminhos do sistema de arquivos
- Para cada chamada do main a funções `nn::fs::Mount*`, `OpenFile`, `OpenDirectory`, `GetEntryType`, `CreateFile`,
  `DeleteFile`, `Commit*`: tentar recuperar o argumento de string (nome de montagem ou caminho) montado com
  `ADRP+ADD`/`ADR` no registrador de argumento (`x0`/`x1`) nas até 16 instruções anteriores à chamada, sem
  sobrescrita no meio.
- Saída `generated/r5_fs_strings.csv` (`chamada_rva,importacao,registrador,string`); no relatório: os nomes de
  montagem distintos e os 30 prefixos de caminho mais frequentes (até o segundo `/`), com contagens; e quantas
  chamadas ficaram sem string recuperada.

## Parte 3 — Recomendação por área
Para cada área, uma linha: total de chamadas, nº de funções distintas usadas, serviços IPC alcançados, e uma
**recomendação** `api` (sobrescrever as funções `nn::` por nome) ou `ipc` (emular o serviço), com o critério que
você usou explicado em uma frase (por exemplo: poucas funções de alto nível e serviço com protocolo complexo → `api`).
Não é decisão final — o Claude Code decide —, é um insumo justificado.

## Autoverificações (o script deve conferir e imprimir)
1. Toda importação listada existe em `imports.csv` e tem stub em `plt_map.csv` (ou é marcada `sem_stub`).
2. A soma das chamadas por área bate com `call_targets.csv`.
3. Toda string recuperada na Parte 2 é ASCII terminada em zero e está no `.rodata` do main.

## Critério de aceite
```
build\venv\Scripts\python.exe -I analysis\recomp\r5_api_surface.py
python tools\check_repo_safety.py
```

## Regras
- Se o mesmo erro acontecer duas vezes seguidas, **pare e relate**. Não tente de novo em loop.
- Não faça commit nem push.
