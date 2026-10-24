# TAREFA PARA O ANTIGRAVITY — Lote 5: prioridade das funções do SDK para a camada HLE

Os lotes 3A e 4 foram aceitos (`--difftest` e `simd_test`). Na R4 o jogo vai começar a chamar as funções `nn::`, libc
e NVN, que precisam de implementação nativa (HLE). Esta tarefa gera a lista de prioridade: quantas chamadas o jogo faz
a cada importação.

## Arquivos
- **Criar** `recomp/tools/hle_priority.py` (Python 3, só biblioteca padrão, módulo `csv`).
- **Criar** `analysis/recomp/hle_priority.md` (gerado pelo script; vai para o GitHub: só nomes de importações e contagens).
- Não edite nenhum outro arquivo.

## Entradas (em `analysis/recomp/generated/`, não versionadas)
- `plt_map.csv` — `stub_rva,got_rva,import` (917 linhas): o stub da PLT de cada importação.
- `call_targets.csv` — `alvo_rva,classe,quantidade_de_chamadas,exemplo_de_origem_rva`: as linhas com classe `stub_plt`
  dizem quantos `BL`/tail calls vão para cada stub.
- `imports.csv` — `nome,namespace,demangled,tipo,got,resolvido_no_sdk`.

## O que o script faz
`python recomp/tools/hle_priority.py analysis/recomp/generated --out analysis/recomp/hle_priority.md`
1. Junta `call_targets.csv` (classe `stub_plt`) com `plt_map.csv` por RVA do stub e com `imports.csv` pelo nome.
2. Para cada importação com stub: chamadas diretas (soma de `quantidade_de_chamadas`), namespace e nome demangled.
3. Escreve o `.md` com:
   - um resumo: total de chamadas diretas a importações, quantas importações têm stub, quantas têm 0 chamadas diretas
     (só chamadas por ponteiro, provavelmente);
   - uma tabela por namespace (ordenada pelo total de chamadas do namespace, decrescente) com as colunas
     `importação (demangled) | chamadas`, ordenada por chamadas decrescentes;
   - uma seção "Top 50" com as 50 importações mais chamadas de qualquer namespace.
   Use o nome demangled; se tiver `|`, escape como `\|` para não quebrar a tabela Markdown.
4. Opção `--self-test`: monta CSVs de exemplo em memória (3 stubs, um com 0 chamadas, um nome com `|` e vírgula), roda a
   junção e confere os totais e a ordenação. Sai com 0 só se tudo bater.

## Aceite (cole a saída dos três)
```
python recomp/tools/hle_priority.py --self-test
python recomp/tools/hle_priority.py analysis/recomp/generated --out analysis/recomp/hle_priority.md
python tools/check_repo_safety.py
```
O segundo deve imprimir: importações com stub = **917**, e o total de chamadas diretas deve ser igual à soma de
`quantidade_de_chamadas` das linhas `stub_plt` do `call_targets.csv` (imprima as duas somas lado a lado). O terceiro
continua `OK` com o `.md` rastreado (`git add -N analysis/recomp/hle_priority.md` antes).

## Regras
- Se o mesmo erro acontecer duas vezes seguidas, **pare e relate**.
- Não faça commit nem push.
