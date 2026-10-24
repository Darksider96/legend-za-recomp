# Censo de fluxo de controle AArch64

Análise estática para R3/R4. Endereços são RVAs; nenhuma execução do jogo.

## Entradas e cobertura

- Funções do `.eh_frame`: **222,376**; intervalos alinhados, positivos, sem sobreposição.
- `.text`: **50,098,768 bytes**, **12,524,692** palavras decodificadas em passos de quatro bytes.
- Cobertura dos intervalos: **49,525,908 bytes (98.8565%)**.
- Importações: **1,001** linhas; todos os slots separados por ponto e vírgula são considerados.
- Disassembler: **Capstone 5.0.7**, venv isolado; **58** palavras tratadas como dados não decodificados.

## Parte 1 — PLT

- Padrões completos encontrados: **918**; stubs comuns mapeados: **917**.
- Faixa dos inícios de stubs mapeados: **0x2fc3900 a 0x2fc7240**; tamanho por stub: **16 bytes**.
- Stubs comuns sem importação: **0**; ver `generated/plt_unmapped.csv`.
- Resolver PLT0 separado: **1** (corpo 0x2fc38e4, GOT 0x3ce8ff8).
- O corpo de PLT0 também contém o padrão de quatro mnemônicos, mas é precedido pelo salvamento de pilha do resolver.
- A exclusão de PLT0 exige esse prefixo específico e ausência de importação para seu GOT; não ajusta a contagem esperada.
- Invariante de 917 JUMP_SLOT: **OK**. A contagem obtida é 917, diferença +0.
- `generated/plt_map.csv` contém somente stubs de importação; o `BR` do resolver também entra na classe `x17_plt`.

## Parte 2 — chamadas e tail calls

- `BL` dentro dos intervalos conhecidos: **682,939**.
- `B` que sai do próprio intervalo: **63,232**.
- Alvos distintos combinados: **115,143**.

| Classe do alvo | Alvos distintos |
|---|---:|
| inicio_funcao | 114,272 |
| stub_plt | 854 |
| meio_funcao | 0 |
| lacuna | 17 |

A classificação prioriza início de função, depois stub PLT, interior de intervalo e lacuna. Um salto entre dois intervalos do `.eh_frame` é contado como tail call operacionalmente; pode ser uma divisão de função, não necessariamente uma chamada de função em nível de linguagem.

Os 854 alvos PLT desta parte são somente os referenciados por chamadas diretas com origem nos intervalos conhecidos; isso não altera os 917 stubs mapeados na Parte 1.

### Até vinte lacunas com mais referências

Existem 17 alvos distintos nesta classe; a tabela lista 17.

| Alvo RVA | BL + tail calls |
|---|---:|
| 0x1dab738 | 32 |
| 0x1d92a84 | 6 |
| 0x1d92a90 | 4 |
| 0x1d92864 | 2 |
| 0x1daf8d4 | 2 |
| 0x1d927b4 | 1 |
| 0x1d9282c | 1 |
| 0x1d928b8 | 1 |
| 0x1d92d64 | 1 |
| 0x1d9da28 | 1 |
| 0x1d9da34 | 1 |
| 0x1d9debc | 1 |
| 0x1d9ec44 | 1 |
| 0x1d9ecb8 | 1 |
| 0x1d9fea8 | 1 |
| 0x1d9ffc0 | 1 |
| 0x1daf994 | 1 |

## Parte 3 — indiretos

- `.text` inteiro: **19,944 BR** e **178,853 BLR**; `RET` excluído.
- Fora dos intervalos conhecidos: **1,021 BR**, **159 BLR**.

| Classe de BR | Sites |
|---|---:|
| tabela_de_salto | 4,454 |
| x17_plt | 918 |
| outro | 14,572 |

- Tabelas candidatas: **4,454**; base resolvida **4,236**; número de entradas resolvido **4,190**; ambos **3,990**.
- A base em `jump_tables.csv` é a base dos dados lidos pela carga indexada, não a base de destinos relativos.
- Célula vazia significa não resolvido; não é substituída por zero.
- `indirect_by_function.csv` inclui todas as funções, mesmo com contagem zero. `indirect_branches.csv` permite conferir os sites e os casos fora de funções.

### Método e limites das tabelas

Reconhecimento local conservador em até 64 instruções anteriores, limitado ao início da função. Exige fluxo da carga indexada para o registrador de `BR`; offsets relativos de 1, 2 ou 4 bytes alimentam `ADD`. Tabelas de ponteiros de 8 bytes exigem base constante e guarda de índice. Uma carga de método virtual isolada não é classificada como switch.

Bases constantes são propagadas por `ADR`, `ADRP`, `MOV`, `ADD` e `SUB` suportados. O número de entradas vem de `CMP` seguido de saída unsigned por `B.HI` ou `B.HS`, com conferência do registrador de índice e cópias por `MOV`. Normalizações diferentes, valores carregados de memória, definições distantes e guardas de outro formato podem ficar não resolvidos. A quantidade recuperada é o limite de índices permitido pela guarda, incluindo possíveis casos default.

O censo linear inclui PLT e lacunas. Palavras de dados alinhadas dentro de `.text` podem ter decodificação válida como instruções; as contagens não demonstram execução nem recuperam todo o CFG. A classe de switch é estrutural e não deve ser tratada como uma lista exaustiva ou como prova de destinos.

## Aceite

Comando do censo e saída:

```text
build\venv\Scripts\python.exe -I analysis\recomp\control_flow_census.py
Capstone 5.0.7; funcoes=222376; iniciando censo
Censo direto concluido; BR=19944; BLR=178853; analisando tabelas
Funcoes: 222376; bytes cobertos: 49525908/50098768
PLT: 917 mapeados; 0 stubs sem import; 1 corpo PLT0 separado
BL: 682939; tail calls B: 63232; alvos distintos: 115143
Alvos: inicio_funcao=114272; stub_plt=854; meio_funcao=0; lacuna=17
BR: 19944; BLR: 178853
BR classes: tabela_de_salto=4454; x17_plt=918; outro=14572
Tabelas: 4454; base=4236; entradas=4190; ambas=3990
Autoverificacoes: OK
Saidas: analysis/recomp/generated/*.csv e analysis/recomp/control_flow_report.md
Invariante PLT 917: OK
```

Comando de segurança e saída observada:

```text
python tools\check_repo_safety.py
OK: 228 arquivos verificados
```

Código de saída do verificador: **0**.

Nenhum commit ou push foi realizado. Os CSVs ficam somente em `generated/`.
