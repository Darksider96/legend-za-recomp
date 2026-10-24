# Destinos das tabelas de salto para R4

Análise estática da imagem sem relocação; nenhum executável do jogo foi iniciado. RVAs são offsets na imagem. O relatório contém metadados; os CSVs detalhados permanecem em `generated/` ignorado pelo Git.

## Resultado

- Candidatas: 4454; resolvido=3927; parcial=9; nao_resolvido=518
- Entradas exportadas: 27860; destinos distintos: 17374
- Bases B resolvidas distintas: 3896; enderecos meio_funcao cobertos: 3929/4474
- Alvos calculados rejeitados: fora/desalinhado=43; instrucao invalida=0
- Slots de ponteiros: 0; sem relativa: 0

Somente tabelas `resolvido` têm uma lista completa validada estruturalmente para gerar o `switch` local. Linhas `parcial` exportam apenas entradas validadas e exigem tratamento dos índices restantes; `nao_resolvido` exige fallback ou análise adicional.

## Método e limites

- Entradas: `image.bin`, `layout.csv`, `functions.csv`, `gap_functions.csv`, `jump_tables.csv`, `relocations.csv`, `indirect_branches.csv` e `code_text_addresses.csv`. Os inventários anteriores não foram alterados.
- A fatia reversa segue definições de registradores, aliases W/X, MOV, ADR/ADRP, ADD/SUB constantes, extensões e LSL; tem limite de 256 instruções por busca, dentro da mesma função. Não atravessa chamadas, transferências incondicionais, instruções inválidas ou joins conhecidos.
- Saídas condicionais podem ser atravessadas pelo caminho de fallthrough. Alvos diretos internos e destinos indiretos recuperados viram cercas de join; a recuperação repete até essas cercas estabilizarem. Não há reconstrução completa do CFG nem prova global de todos os predecessores indiretos desconhecidos. Constantes não locais ficam explicitamente sem resolução.
- Para offsets, a fórmula é `B + offset(T[i])` módulo 64 bits, preservando largura do registrador, sinal/zero-extensão e deslocamentos. B e T são recuperados separadamente. A escala de indexação da carga deve corresponder ao tamanho da entrada. Bases conhecidas e tamanho são confrontados com o censo.
- O número de entradas vem exclusivamente de `entradas` no censo. Nenhum tamanho foi inferido por conteúdo ou por plausibilidade dos destinos.
- Para ponteiros de 8 bytes, o valor vem exclusivamente de `alvo_rva` da relocação RELR/RELATIVE do slot. Ausência de relocação rejeita a entrada. A imagem nunca é relocada nem escrita.
- Cada entrada exportada pertence à mesma função do BR, alinha a 4 e decodifica em uma instrução Capstone válida. Isso é uma condição estrutural, não prova isolada de alcançabilidade ou de que bytes jamais sejam dados. UDF é rejeitado adicionalmente.
- Versão observada do Capstone: `5.0.7`. Testes sintéticos cobrem extensão com sinal, offset negativo e uso obrigatório de relocação para ponteiros.
- Iterações de cercas por função: `{1: 406, 2: 3613}`.

## Contagens por forma e status

| Forma | Resolvido | Parcial | Não resolvido |
|---|---:|---:|---:|
| offset_s32 | 157 | 0 | 10 |
| offset_u16 | 209 | 1 | 89 |
| offset_u8 | 3561 | 8 | 419 |

## Histograma de entradas

| Entradas | Resolvido | Parcial | Não resolvido |
|---|---:|---:|---:|
| 1 | 0 | 0 | 0 |
| 2-3 | 0 | 0 | 0 |
| 4-7 | 3294 | 0 | 127 |
| 8-15 | 431 | 8 | 75 |
| 16-63 | 156 | 0 | 46 |
| 64+ | 46 | 1 | 6 |
| sem_numero | 0 | 0 | 264 |

## Cobertura das bases B

Das 4474 materializações classificadas como `meio_funcao`, 3929 apontam para B de alguma tabela resolvida. São 3896 RVAs de base distintos cobertos. A contagem principal é por linha/origem, para preservar o denominador 4.474; não por tabela nem por RVA distinto. B é a base de destinos em `.text`, não a base T dos dados da tabela.

## 20 BR não resolvidos com mais BR na função

Ordenação: quantidade total de BR na função, depois RVA crescente. Contagem de BR usa `indirect_branches.csv`.

| BR RVA | Função RVA | BR na função | Motivo |
|---|---|---:|---|
| 0x4154 | 0x3ef0 | 36 | base da tabela nao provada localmente |
| 0x4404 | 0x3ef0 | 36 | base da tabela nao provada localmente |
| 0xb0f32c | 0xb0f08c | 9 | base da tabela nao provada localmente |
| 0xb1002c | 0xb0fd8c | 9 | base da tabela nao provada localmente |
| 0x2ea0ec4 | 0x2ea0c28 | 9 | base da tabela nao provada localmente |
| 0xf47b8 | 0xf42e0 | 7 | base da tabela nao provada localmente |
| 0xf4fa8 | 0xf42e0 | 7 | base da tabela nao provada localmente |
| 0xf5208 | 0xf42e0 | 7 | base da tabela nao provada localmente |
| 0xf6014 | 0xf42e0 | 7 | numero de entradas ausente no censo; nao inferido |
| 0xf62d8 | 0xf42e0 | 7 | numero de entradas ausente no censo; nao inferido |
| 0xf6d14 | 0xf42e0 | 7 | numero de entradas ausente no censo; nao inferido |
| 0xf6fd4 | 0xf42e0 | 7 | numero de entradas ausente no censo; nao inferido |
| 0xe2ea88 | 0xe2e9f0 | 7 | base da tabela nao provada localmente |
| 0x3c04c | 0x3a9e0 | 6 | numero de entradas ausente no censo; nao inferido |
| 0xc13b40 | 0xc13a30 | 6 | base da tabela nao provada localmente |
| 0xc13bb4 | 0xc13a30 | 6 | base da tabela nao provada localmente |
| 0xc13da0 | 0xc13a30 | 6 | base da tabela nao provada localmente |
| 0xc13e14 | 0xc13a30 | 6 | base da tabela nao provada localmente |
| 0xc13f2c | 0xc13a30 | 6 | base da tabela nao provada localmente |
| 0xc13fa4 | 0xc13a30 | 6 | base da tabela nao provada localmente |

Motivos agregados para as candidatas não resolvidas:

- base da tabela nao provada localmente: 254.
- numero de entradas ausente no censo; nao inferido: 264.

Dos 43 cálculos rejeitados fora/desalinhados, 43 têm o BR em função proposta por `gap_functions.csv`. Esses destinos cruzam os limites fornecidos; isso exige revisar os limites ou confirmar uma transferência não local antes de gerar gotos. Não demonstra, por si só, que a fórmula esteja errada. Os arquivos anteriores permanecem intactos.

## Rejeições e autoverificações

1. Exportados dentro da mesma função e alinhados: OK. Cálculos rejeitados fora/desalinhados: 43.
2. Exportados decodificam: OK. Cálculos rejeitados por instrução inválida: 0.
3. Slots de ponteiro com relocação relativa: OK; 0 slots. Se zero, a verificação no binário é vacuamente verdadeira; o caminho é exercitado sinteticamente.
4. Cobertura e somas: OK; 4454/4454 candidatas, 3929/4474 materializações cobertas.

RVAs dos cálculos fora/desalinhados (BR, função, destino; sem bytes ou listagem):

- 0x1dab848, 0x1dab7ac, 0x1daf414
- 0x1db4ee4, 0x1db4e48, 0x1db4e14
- 0x1db4ee4, 0x1db4e48, 0x1db4e14
- 0x1db4ee4, 0x1db4e48, 0x1db4e14
- 0x1db4ee4, 0x1db4e48, 0x1db4e14
- 0x1db4ee4, 0x1db4e48, 0x1db4e14
- 0x1db4ee4, 0x1db4e48, 0x1db4e10
- 0x1db4f7c, 0x1db4e48, 0x1db4e14
- 0x1db4f7c, 0x1db4e48, 0x1db4e14
- 0x1db4f7c, 0x1db4e48, 0x1db4e14
- 0x1db4f7c, 0x1db4e48, 0x1db4e14
- 0x1db4f7c, 0x1db4e48, 0x1db4e14
- 0x1db4f7c, 0x1db4e48, 0x1db4e10
- 0x1db4ff4, 0x1db4e48, 0x1db4e14
- 0x1db4ff4, 0x1db4e48, 0x1db4e14
- 0x1db4ff4, 0x1db4e48, 0x1db4e14
- 0x1db4ff4, 0x1db4e48, 0x1db4e14
- 0x1db4ff4, 0x1db4e48, 0x1db4e14
- 0x1db4ff4, 0x1db4e48, 0x1db4e10
- 0x1db4ff4, 0x1db4e48, 0x1db4e14
- 0x1db4ff4, 0x1db4e48, 0x1db4e14
- 0x1db5050, 0x1db4e48, 0x1db4e14
- 0x1db5050, 0x1db4e48, 0x1db4e10
- 0x1db5050, 0x1db4e48, 0x1db4e14
- 0x1db5050, 0x1db4e48, 0x1db4e14
- 0x1db5050, 0x1db4e48, 0x1db4e14
- 0x1db50cc, 0x1db4e48, 0x1db4e14
- 0x1db50cc, 0x1db4e48, 0x1db4e10
- 0x1db50cc, 0x1db4e48, 0x1db4e14
- 0x1db50cc, 0x1db4e48, 0x1db4e14
- 0x1db50cc, 0x1db4e48, 0x1db4e14
- 0x1db50cc, 0x1db4e48, 0x1db4e14
- 0x1db514c, 0x1db4e48, 0x1db4e14
- 0x1db514c, 0x1db4e48, 0x1db4e14
- 0x1db514c, 0x1db4e48, 0x1db4e14
- 0x1db514c, 0x1db4e48, 0x1db4e14
- 0x1db514c, 0x1db4e48, 0x1db4e14
- 0x1db514c, 0x1db4e48, 0x1db4e10
- 0x1dc075c, 0x1dc06d8, 0x1dc0800
- 0x1dc075c, 0x1dc06d8, 0x1dc0820
- 0x1dc075c, 0x1dc06d8, 0x1dc0858
- 0x1dc075c, 0x1dc06d8, 0x1dc0830
- 0x1dc3ad8, 0x1dc3ab8, 0x1dc3b18

## Saídas

- `generated/jump_table_targets.csv`: quatro colunas pedidas; uma linha por entrada validada.
- `generated/jump_table_summary.csv`: sete colunas pedidas; uma linha por candidata.
- `generated/jump_table_formulas.csv`: T, B, carga e expressão aritmética completa, sem listagem de instruções.
- `generated/jump_table_rejected_targets.csv`: índice e motivo de cada rejeição calculada.
- `generated/jump_table_pointer_slots.csv`: auditoria de relocação para os slots de 8 bytes.
- `generated/jump_table_base_coverage.csv`: cobertura por materialização original.

## Comandos de aceite

```text
build\venv\Scripts\python.exe -I analysis\recomp\jump_table_targets.py
Capstone 5.0.7; candidatas=4454; funcoes=4019
Funcoes analisadas: 1000/4019
Funcoes analisadas: 2000/4019
Funcoes analisadas: 3000/4019
Funcoes analisadas: 4000/4019
Candidatas: 4454; resolvido=3927; parcial=9; nao_resolvido=518
Entradas exportadas: 27860; destinos distintos: 17374
Bases B resolvidas distintas: 3896; enderecos meio_funcao cobertos: 3929/4474
Alvos calculados rejeitados: fora/desalinhado=43; instrucao invalida=0
Slots de ponteiros: 0; sem relativa: 0
AUTOVERIFICACAO 1: OK
AUTOVERIFICACAO 2: OK
AUTOVERIFICACAO 3: OK
AUTOVERIFICACAO 4: OK
Saidas: analysis/recomp/generated/*.csv e analysis/recomp/jump_table_targets_report.md
Exit code: 0
```

```text
python tools\check_repo_safety.py
OK: 246 arquivos verificados

Exit code: 0
```

A verificação de segurança acima é executada pelo script com o mesmo interpretador, em modo isolado. O comando independente também deve ser executado no aceite. Sem commit ou push.
