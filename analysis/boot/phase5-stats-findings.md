# Fase 5 — tabelas Avalon

Análise estática de `exefs/main` e dos assets fornecidos, sem executar o jogo.
Endereços são VAs com base `0x7100000000`; offsets de asset são relativos ao arquivo
extraído. O código decompilado foi conferido pelo assembly dos intervalos explícitos.

## Localização real

No dicionário recuperado há **3.476 caminhos `.tbl`**, todos sob `ik_message/`.
Isso classifica os caminhos conhecidos, não os membros ainda sem nome do archive.
As tabelas numéricas identificadas nesta investigação estão em:

| Asset | Tamanho | Estrutura observada |
|---|---:|---|
| `avalon/data/personal_array.bin` | 377.176 | FlatBuffers, vetor de 1.422 tabelas |
| `avalon/data/waza_array.bin` | 90.640 | FlatBuffers, vetor de 902 tabelas |
| `avalon/data/tokusei_array.bin` | 4.024 | FlatBuffers, vetor de 299 tabelas |
| `avalon/data/growTable.bin` | 3.248 | FlatBuffers, campo inline com 808 `u32` |

`growTable.bin` **não** possui um vetor com contador no campo 0. Seu root tem
object-size `0xCA4`; o campo inline começa em arquivo `+0x10` e ocupa 3.232 bytes.
Os 808 valores podem ser particionados em oito grupos de 101; os nomes/classes
dessas curvas não foram recuperados.

## Carregadores

| Construção do caminho | String referenciada | Chamada para loader comum |
|---|---|---|
| `0x7100CF5880` | `personal_array.bin` em `0x710309A87E` | `0x7100CF58F8` → `0x7100CF5B90` |
| `0x7100CF5910` | `waza_array.bin` em `0x710303CB6A` | `0x7100CF5988` → `0x7100CF5B90` |
| `0x7100CF59A0` | `tokusei_array.bin` em `0x7103007DE0` | `0x7100CF5A18` → `0x7100CF5B90` |
| `0x7100CF5A30` | `growTable.bin` em `0x71030CEFD4` | `0x7100CF5AA8` → `0x7100CF5B90` |

Os quatro builders consultam o contexto de paths por `0x7100365AD8` e formatam
diretório, separador opcional e basename. A configuração dos prefixos usa a string
`avalon/data` em `0x71031536A3`, referenciada por `0x710056C230`.

Para personal, `0x7100CF3560` chama o builder em `0x7100CF358C`, solicita o asset
pelo loader já mapeado (`0x710003E0E0` no ramo observado) e chama o construtor do
índice `0x7100CF3D30`. Algumas decompilações absorvem essa função seguinte;
`phase5-stats-index/7100cf3d30.txt` contém seu próprio intervalo e assembly.

## Identificador espécie/forma e índice em memória

`personal_array.bin`: root no offset `0xC`, campo 0 → vetor de uoffset32 para as
1.422 tabelas. Cada record tem 26 slots lógicos na vtable. O campo 0 do record é
um valor inline de quatro bytes consumido como **dois `uint16_t` adjacentes**:

```cpp
struct SpeciesFormWire {
    uint16_t species_id; // +0
    uint16_t form_id;    // +2
};
```

`0x7100CF3D30` resolve esse campo, carrega os dois halfwords e insere no hash map
em contexto `+0x48` usando `species * 10000 + form`. O payload do node é o ponteiro
para a tabela FlatBuffers original; o record sentinela de chave 0 fica em
contexto `+0x80`. Não se transforma o record inteiro em um POD de stride fixo.

`0x7100CF4270(context, species, form)` repete a fórmula, procura o map e retorna a
tabela original; se não encontra a chave, devolve o record sentinela.

Os accessors em `0x7100CF93BC` e seguintes recebem um objeto que contém espécie
`u16` em `+8` e forma `u16` em `+0xA`, passados ao lookup. Esses offsets pertencem
ao objeto consumidor desses accessors; não são atribuídos automaticamente ao
`FieldPokemonComponent` nem a um `PokeActor` hipotético.

## Seis bytes de stats

O campo 20 (entrada da vtable `+0x2C`) é um bloco inline de seis bytes unsigned.
Os accessors abaixo comprovam a largura e o índice dentro desse bloco:

| Função | Byte |
|---|---:|
| `0x7100CF9428` | 0 |
| `0x7100CF9474` | 1 |
| `0x7100CF94C0` | 2 |
| `0x7100CF950C` | 3 |
| `0x7100CF9558` | 4 |
| `0x7100CF95A4` | 5 |

`0x7100CF971C` também lê e soma os seis bytes. Os valores abaixo são os bytes
extraídos, na ordem armazenada. A interpretação descritiva é
HP/Attack/Defense/SpAttack/SpDefense/Speed; os nomes originais dos membros não
foram recuperados. A largura e as posições são comprovadas pelo código.

| Espécie / forma 0 | Índice do record | Seis bytes |
|---|---:|---|
| Mewtwo, 150 | 213 | 106, 110, 90, 154, 90, 130 |
| Pikachu, 25 | 33 | 35, 55, 40, 50, 50, 90 |
| Bulbasaur, 1 | 1 | 45, 49, 49, 65, 65, 45 |
| Wartortle, 8 | 11 | 59, 63, 80, 65, 80, 58 |

O índice do record não é o National Dex: as formas introduzem records adicionais.
As 1.422 chaves espécie/forma são únicas no asset observado.

## Reprodução e evidências

Extração de subset pelo parser local já fornecido:

```powershell
& .\za_port\trpfs_tool.exe --romfs romfs -o analysis/boot/phase5-stats-assets extract `
  avalon/data/personal_array.bin avalon/data/waza_array.bin `
  avalon/data/tokusei_array.bin avalon/data/growTable.bin
python analysis/boot/phase5_stats_assets.py
```

- `phase5-stats-assets.json`: identidade SHA-256 de cada asset, shapes, records de
  teste e limite da classificação dos `.tbl`.
- `phase5-stats-code-refs.json`: xrefs de strings pelos bytes de ADRP/ADD.
- `phase5-stats-ghidra/`: builders e configuração dos prefixos.
- `phase5-stats-readers/`: carregamento, loader comum e inserção no map.
- `phase5-stats-index/`: construção do índice e lookup.
- `phase5-stats-accessors/` e `phase5-stats-statbytes.txt`: leituras de stats.
- `phase5_stats_types_fragment.h`: storage dos dois blocos inline comprovados;
  o header final integra essas definições com os componentes.

Não se reconstituiu aqui o schema completo dos 26 campos pessoais, todos os
campos de golpes/habilidades nem os enums originais dos tipos. Tamanho de arquivo,
contagem de vetor e semântica de campo são observações distintas.
