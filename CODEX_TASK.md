# TAREFA PARA O CODEX — Destinos das tabelas de salto (`switch`) para o tradutor (R4)

## Contexto
A tarefa de alvos indiretos foi aceita (INIT_ARRAY 1912/1912 conferido de forma independente). Ela mostrou 4.474
endereços **no meio de funções** montados com `ADR`/`ADRP+ADD`: em geral são bases de tabelas de `switch`. Hoje o
tradutor transforma todo `BR Xn` em `Dispatch(Xn)`, que só sabe achar inícios de função. Para um `BR` de tabela de
salto, o tradutor vai gerar um `switch` local com `goto` para cada destino — para isso precisa da lista exata de
destinos de cada `BR`.

Entradas (em `analysis/recomp/generated/`, não versionadas):
- `jump_tables.csv` (do seu censo): `br_rva,base_rva,entradas,tamanho_entrada` — 4.454 candidatas.
- **`image.bin`** (novo, do `nso_inspect`): imagem plana `.text`+`.rodata`+`.data`+`.bss`, offset no arquivo = RVA,
  **sem** relocações aplicadas. As tabelas de offsets ficam no `.rodata`.
- `relocations.csv`, `layout.csv`, `functions.csv`, `gap_functions.csv`, `indirect_branches.csv`, `code_text_addresses.csv`.
Capstone no `build\venv`.

## Arquivos que você pode criar (e SOMENTE estes)
- `analysis/recomp/jump_table_targets.py`
- `analysis/recomp/jump_table_targets_report.md`
- Saídas CSV **somente** em `analysis/recomp/generated/`
Não edite nenhum outro arquivo — inclusive os seus anteriores (`gap_functions*`, `indirect_targets*`); se achar que
precisam de correção, escreva no relatório.

## O que fazer
Para cada `BR` candidato, reconstruir a fórmula do destino a partir das instruções anteriores (dentro da função) e
calcular os destinos reais lendo as entradas da tabela no `image.bin`. Formas esperadas (confirme no binário):
1. **Offsets relativos de 1/2/4 bytes**: `ldrb/ldrh/ldrsw wE, [xT, xI, ...]` lendo `T[i]`; destino =
   `B + (extensão(T[i]) << s)`, onde `B` vem de um `ADR`/`ADRP+ADD` (pode ser diferente de `T`) e `s` do `add ..., lsl #s`.
   Respeite zero-extensão (`ldrb/ldrh`) e sinal (`ldrsb/ldrsh/ldrsw`).
2. **Tabela de ponteiros de 8 bytes** (`ldr xE, [xT, xI, lsl #3]`): os slots devem ter relocação relativa em
   `relocations.csv`; destino = `alvo_rva` da relocação (não o valor cru do `image.bin`).
3. Outras formas: classificar como `nao_resolvido` com o motivo.

Número de entradas: o `entradas` do censo (guarda `cmp`/`b.hi`); quando faltar, não chute — `nao_resolvido`.

## Saídas
- `generated/jump_table_targets.csv`: `br_rva,funcao_rva,indice,alvo_rva` (uma linha por entrada).
- `generated/jump_table_summary.csv`: `br_rva,funcao_rva,forma,entradas,destinos_distintos,status,motivo`.

## Autoverificações (o script deve conferir e imprimir)
1. Todo destino resolvido está **dentro da mesma função** do `BR` (`.eh_frame` ou `gap_functions.csv`) e alinhado a 4.
   Contar e listar no relatório (só RVAs) os que caírem fora.
2. Todo destino resolvido decodifica como instrução válida no Capstone (não é dado).
3. Para as tabelas de 8 bytes: todo slot lido tem relocação relativa.
4. Cobertura: quantos `BR` candidatos ficaram `resolvido`, `parcial` e `nao_resolvido`; e quantos dos 4.474 endereços
   `meio_funcao` de `code_text_addresses.csv` são base `B` de alguma tabela resolvida.

## Relatório
Contagens por forma e status, histograma de número de entradas, os 20 `BR` não resolvidos com mais instruções
`BR` na função (só RVAs e motivo), e a saída dos comandos de aceite.

## Critério de aceite
```
build\venv\Scripts\python.exe -I analysis\recomp\jump_table_targets.py
python tools\check_repo_safety.py
```

## Regras
- Se o mesmo erro acontecer duas vezes seguidas, **pare e relate**. Não tente de novo em loop.
- Não faça commit nem push.
