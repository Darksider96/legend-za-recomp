# Descoberta de funções nas lacunas do unwind

Levantamento estático para R4, sem executar o jogo. Todos os endereços são RVAs. Os limites propostos são unidades de tradução inferidas, não limites originais comprovados.

## Inventário

- Funções conhecidas: **222376**; `.text`: **50098768 bytes**.
- Lacunas excluindo a PLT: **40464**, **558156 bytes**.
- Stubs PLT excluídos: **917**; corpos PLT0 excluídos: **1**.

| Classe | Lacunas | Bytes |
|---|---:|---:|
| zeros/padding | 40462 | 327312 |
| dados | 1 | 32 |
| codigo | 1 | 230812 |

Zeros/padding exige somente palavras zero, `UDF` ou `NOP`. Dados significa maioria de palavras não decodificáveis, ou ausência de `RET`, `B` e `BR`. As demais regiões decodificáveis com esses terminadores são classificadas como código. O prefixo e o sufixo de `.text` também são inventariados.

### Até dez maiores lacunas de código

Foram encontradas 1 lacunas dessa classe; são exibidas 1.

| Início RVA | Tamanho em bytes |
|---|---:|
| 0x1d92734 | 230812 |

## Inícios e limites propostos

- Propostas: **1034**, cobrindo **230808 bytes**.
- Referências ADR ou ADRP/ADD em código conhecido: **0**.
- Propagação até ponto fixo: **7 rodadas**; novas referências contadas: **4118 BL**, **538 B externos**.

| Origem prioritária | Propostas |
|---|---:|
| alvo_bl | 17 |
| export | 0 |
| sequencia | 569 |
| adr | 0 |
| propagado | 448 |

Prioridade preservada: alvo do censo, exportação de função, sequência, referência de endereço e propagação. Exportações de objetos contam no inventário, mas não geram inícios de função. Referências de endereço precisam estar alinhadas e cair numa lacuna classificada como código.

Sequência considera o primeiro código da lacuna, o primeiro endereço após padding e um prólogo de pilha típico após `RET`, `BR` ou `B`. Não se presume que toda instrução após um retorno seja uma função nova.

ADR é resolvido diretamente; ADRP/ADD usa até doze instruções seguintes, interrompendo a busca quando o registrador é sobrescrito ou há transferência de controle. Esses endereços podem ser labels internos e sua origem continua identificada no CSV.

Cada fim usa o início seguinte ou a borda da lacuna. Padding posterior a terminador é removido. Quando existe fall-through real até o próximo início, seu padding é conservado para não criar uma borda intermediária sem terminador. Os bytes removidos/conservados constam no CSV detalhado.

Todo BL em propostas gera início se seu alvo estiver numa lacuna de código. B só gera início quando sai da proposta corrente; branches internos não são todos promovidos a funções. Isso também permite descobrir chamadas a outras funções dentro da mesma lacuna física. O processo é repetido até estabilizar.

## Quatro autoverificações

| Verificação | Resultado |
|---|---|
| 1. Todos os 17 alvos do censo são inícios | OK; ausentes=0 |
| 2. Alinhamento e ausência de sobreposição | OK; problemas=0 |
| 3. Terminador ou fall-through declarado | OK; fall-through=157; outros=0 |
| 4. Reclassificação incluindo as propostas | OK; lacunas restantes=0 |

| Classe final dos alvos | Quantidade distinta |
|---|---:|
| inicio_funcao | 115168 |
| stub_plt | 917 |
| meio_funcao | 0 |
| lacuna | 0 |

A reclassificação inclui as referências originais do censo e os BL/B externos das propostas. O censo original e seus CSVs não são alterados.

## Limites da inferência

Um intervalo pode conter mais de uma função original ou representar um fragmento. Padding, prólogos e endereços materializados são evidência heurística; indirect calls, vtables em segmentos de dados e branches condicionais não promovem automaticamente todos os seus destinos. As verificações demonstram cobertura dos alvos diretos e coerência dos intervalos propostos, não equivalência de execução nem recuperação completa do CFG.

## Arquivos

- `generated/gaps.csv`: todas as lacunas e classificação.
- `generated/gap_functions.csv`: rva, tamanho e origem para o recompilador.
- `generated/gap_function_details.csv`: fins, terminadores e fall-through.
- `generated/gap_seed_evidence.csv`: origem e endereço de evidência de cada início.
- `generated/call_targets_after_gaps.csv`: reclassificação após a união.
- `generated/gap_unresolved.csv`: alvos ainda em lacunas e motivo.

## Aceite

```text
build\venv\Scripts\python.exe -I analysis\recomp\gap_functions.py
Capstone 5.0.7; lacunas=40464; descobrindo referencias ADR
Propagacao 1: 401 novos inicios; total=987
Propagacao 2: 32 novos inicios; total=1019
Propagacao 3: 3 novos inicios; total=1022
Propagacao 4: 4 novos inicios; total=1026
Propagacao 5: 4 novos inicios; total=1030
Propagacao 6: 4 novos inicios; total=1034
Propagacao 7: 0 novos inicios; total=1034
Lacunas: 40464; bytes=558156; PLT excluida
Classes: zeros/padding=40462/327312 bytes; dados=1/32 bytes; codigo=1/230812 bytes
Propostas: 1034; bytes=230808; rodadas=7
AUTOVERIFICACAO 1: OK; alvos originais=17; ausentes=0
AUTOVERIFICACAO 2: OK; desalinhamentos/sobreposicoes=0
AUTOVERIFICACAO 3: OK; finais nao resolvidos=0; fall-through=157
AUTOVERIFICACAO 4: OK; alvos lacuna apos uniao=0
Alvos apos uniao: inicio_funcao=115168; stub_plt=917; meio_funcao=0; lacuna=0
Saidas: analysis/recomp/generated/*.csv e analysis/recomp/gap_functions_report.md
```

```text
python tools\check_repo_safety.py
OK: 236 arquivos verificados
```

Código de saída do verificador: **0**. Nenhum commit ou push foi realizado.
