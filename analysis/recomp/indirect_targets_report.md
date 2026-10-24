# Alvos indiretos para a tabela de despacho R4

Inventário estático, sem executar o jogo. Todos os endereços são RVAs. A união é uma estimativa estrutural; um endereço em `.text` não prova por si só um ponteiro de função.

## Entradas e classificação

- `.eh_frame`: **222376** funções; propostas das lacunas: **1034**; stubs PLT: **917**.
- `.text`: **0x0** a **0x2fc7250**, limite final exclusivo.
- Imagem: **0x0** a **0x5f7a000**, limite final exclusivo.
- Classes: início de função conhecida, stub PLT, interior de função e fora desses intervalos. As propostas das lacunas continuam tendo limites heurísticos.

## Parte 1 — ponteiros relocados para .text

- Relocações relativas totais: **346629**.
- Referências para `.text`: **197588**; destinos distintos: **88972**.

| Classe | Referências | Alvos distintos |
|---|---:|---:|
| inicio_funcao | 197440 | 88841 |
| stub_plt | 0 | 0 |
| meio_funcao | 140 | 128 |
| fora | 8 | 3 |

| Segmento do slot, conforme CSV | Referências para .text |
|---|---:|
| data | 197588 |

### Vinte destinos interiores mais referenciados

| Alvo RVA | Função contendo, RVA | Deslocamento | Referências |
|---|---|---:|---:|
| 0x1da6a30 | 0x1da6a20 | 0x10 | 4 |
| 0x1da2dec | 0x1da2c88 | 0x164 | 2 |
| 0x1da6a78 | 0x1da6a20 | 0x58 | 2 |
| 0x1dbbaf4 | 0x1dbbad8 | 0x1c | 2 |
| 0x1dbbb04 | 0x1dbbad8 | 0x2c | 2 |
| 0x1dbbb0c | 0x1dbbad8 | 0x34 | 2 |
| 0x1dbbb10 | 0x1dbbad8 | 0x38 | 2 |
| 0x1dbbb24 | 0x1dbbad8 | 0x4c | 2 |
| 0x1dbbb2c | 0x1dbbad8 | 0x54 | 2 |
| 0x1dbbb34 | 0x1dbbad8 | 0x5c | 2 |
| 0x1681a00 | 0x16818e0 | 0x120 | 1 |
| 0x1681a64 | 0x16818e0 | 0x184 | 1 |
| 0x1681ad0 | 0x16818e0 | 0x1f0 | 1 |
| 0x1681b40 | 0x16818e0 | 0x260 | 1 |
| 0x1681bb0 | 0x16818e0 | 0x2d0 | 1 |
| 0x1681c20 | 0x16818e0 | 0x340 | 1 |
| 0x1681c80 | 0x16818e0 | 0x3a0 | 1 |
| 0x1681cd0 | 0x16818e0 | 0x3f0 | 1 |
| 0x1681d30 | 0x16818e0 | 0x450 | 1 |
| 0x1681df0 | 0x16818e0 | 0x510 | 1 |

Dos destinos interiores distintos, **45** estão em intervalos propostos das lacunas. Essa classificação pode sinalizar entradas adicionais/aliases dentro de um intervalo inferido; não autoriza promover todo endereço automaticamente.

### Referências fora de funções e PLT

| Alvo RVA | Referências |
|---|---:|
| 0x0 | 5 |
| 0x8 | 2 |
| 0x2fc38dc | 1 |

Esses endereços são mantidos na união, com classe `fora`; limites de segmento e metadados também podem produzir ponteiros para `.text` sem representar código chamável.

## Parte 2 — INIT_ARRAY

- Início **0x39e1000**, tamanho **15296 bytes**, **1912 slots** de oito bytes.
- Slots com exatamente uma relocação relativa e destino `inicio_funcao`: **1912**.
- Invariante de 1912 entradas: **OK**.

## Parte 3 — candidatas a vtable

- Sequências máximas: **32012**, cobrindo **197588 slots** em `.rodata`/`.data`.

| Entradas por sequência | Sequências |
|---|---:|
| 1 | 12130 |
| 2-3 | 4985 |
| 4-7 | 4720 |
| 8-15 | 7824 |
| 16-63 | 2292 |
| 64+ | 61 |

Maior sequência: **0x39e1000**, **1912 entradas**, segmento **.data**.

As sequências incluem singletons e não excluem o INIT_ARRAY: o critério pedido é apenas contiguidade de slots relativos para `.text`. Assim, nem toda sequência é uma vtable semântica.

## Parte 4 — endereços montados no código

- Materializações aceitas: **33192**; destinos distintos: **29655**.
- Métodos: **22202 ADR**, **10990 ADRP/ADD**.
- Cadeias ADRP candidatas verificadas em detalhe: **11029**.
- Excluídas por uso exclusivo como base de load até sobrescrita: **0**.

| Classe | Materializações | Alvos distintos |
|---|---:|---:|
| inicio_funcao | 28718 | 25220 |
| stub_plt | 0 | 0 |
| meio_funcao | 4474 | 4435 |
| fora | 0 | 0 |

Todas as funções conhecidas e propostas são percorridas. ADRP/ADD exige fonte e destino ADD iguais ao registrador ADRP, no máximo doze instruções adiante, sem sobrescrita ou transferência intermediária. A origem registrada é o ADR ou o ADD que completa o endereço.

O filtro de loads exige primeiro uso nas quatro instruções seguintes, apenas usos como base de load e sobrescrita que encerre o valor em até doze instruções. Se houver armazenamento do ponteiro, outro uso, transferência de controle ou valor ainda vivo ao fim da janela, a candidata é conservada. Isso evita descartar callbacks só porque também são usados numa leitura.

Endereços de labels e bases de destinos relativos podem permanecer como candidatos. A análise local não é prova de que cada constante será chamada por BR/BLR, nem cobre ponteiros construídos por outros padrões ou gravados dinamicamente.

## Parte 5 — união para despacho

- Exportações de função: **33** nomes, **33** destinos distintos.
- União de relocados, código e exports: **118196** destinos.
- Inícios conhecidos: **113630**; demais: **4566**.

| Classe | Destinos na união |
|---|---:|
| inicio_funcao | 113630 |
| stub_plt | 0 |
| meio_funcao | 4563 |
| fora | 3 |

O CSV de união preserva origens e contagens de evidência. Destinos interiores e `fora` não são silenciosamente promovidos a funções traduzidas. Relocações por símbolo para o SDK e endereços resolvidos em runtime não entram como relativos estáticos para o código do main.

## Autoverificações

| Tipo de relocação | Linhas |
|---|---:|
| relr | 346629 |
| relative | 0 |
| abs64 | 3303 |
| jump_slot | 917 |
| glob_dat | 55 |

| Segmento de destino dos relativos | Referências |
|---|---:|
| .bss | 8497 |
| .data | 40807 |
| .rodata | 99737 |
| .text | 197588 |

- Fora da imagem: **0**; alvos em padding entre segmentos são contados separadamente.
- As somas conferem referências, destinos distintos, INIT_ARRAY, segmentos e histograma de sequências.

## Arquivos

- `generated/text_pointers.csv`: ponteiros relativos para `.text`.
- `generated/init_array_audit.csv`: auditoria de cada slot inicializador.
- `generated/vtable_candidates.csv`: sequências máximas de slots.
- `generated/code_text_addresses.csv`: endereços materializados aceitos.
- `generated/code_text_address_details.csv`: método, registrador e origem ADRP.
- `generated/excluded_text_addresses.csv`: exclusões justificadas pelo filtro de loads.
- `generated/dispatch_targets.csv`: união estimada para despacho.
- `generated/relative_target_segments.csv`: contagem dos destinos relativos por segmento.
- `generated/relative_targets_outside_image.csv`: exceções de limites de imagem.

## Aceite

```text
build\venv\Scripts\python.exe -I analysis\recomp\indirect_targets.py
Capstone 5.0.7; relocs=350904; ponteiros .text=197588; varrendo codigo
Relocacoes: 350904; relativas: 346629
Ponteiros .text: 197588; alvos distintos: 88972
Ponteiros por classe: inicio_funcao=197440; stub_plt=0; meio_funcao=140; fora=8
INIT_ARRAY: 1912 slots; relativos e inicio_funcao: 1912
Candidatas vtable: 32012; maior: 1912 entradas
Enderecos no codigo: 33192; distintos: 29655; excluidos base-load: 0
Codigo por classe: inicio_funcao=28718; stub_plt=0; meio_funcao=4474; fora=0
Despacho estimado: 118196; inicio_funcao: 113630; outros: 4566
Despacho por classe: inicio_funcao=113630; stub_plt=0; meio_funcao=4563; fora=3
AUTOVERIFICACAO 1: OK; total e tipos de relocacao
AUTOVERIFICACAO 2: OK; INIT_ARRAY 1912/1912
AUTOVERIFICACAO 3: OK; relativos fora da imagem: 0
AUTOVERIFICACAO 4: OK; somas de classes, segmentos e sequencias
Saidas: analysis/recomp/generated/*.csv e analysis/recomp/indirect_targets_report.md
```

```text
python tools\check_repo_safety.py
OK: 243 arquivos verificados
```

Código de saída do verificador: **0**. Nenhum commit ou push foi realizado.
