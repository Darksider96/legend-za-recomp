# Fase 5 — cries e dispatch Wwise

Análise estática de `exefs/main`, SHA-256 `056eef7956b09414bfa0489b8de3ed0674393513f09a78d5312354cbd9557720`. VAs usam a base `0x7100000000`; os offsets nos bancos são offsets de arquivo, não VAs. Nenhum jogo ou áudio foi executado. Os nomes dos wrappers abaixo são atribuições semânticas da análise, não símbolos originais recuperados.

## IDs dos quatro cries

O binário contém o formato literal **`PLAY_PV_%04hu` em `0x710306CF3E`**. Os nomes construídos e seus hashes correspondem exatamente a objetos HIRC **tipo 4 (Event)** no banco **`PM_VOICE.bnk`**, versão BKHD **145 / 0x91**.

| Pokémon | Espécie | Nome do evento | Event ID decimal | Event ID hex | Offset do objeto Event no banco |
|---|---:|---|---:|---|---|
| Mewtwo | 150 | `PLAY_PV_0150` | 2296219905 | `0x88DD8901` | `0xC6A73` |
| Pikachu | 25 | `PLAY_PV_0025` | 1944037004 | `0x73DFA68C` | `0xC633E` |
| Bulbasaur | 1 | `PLAY_PV_0001` | 1910481826 | `0x71DFA3A2` | `0xC61A4` |
| Wartortle | 8 | `PLAY_PV_0008` | 1910481835 | `0x71DFA3AB` | `0xC629A` |

O ID do objeto HIRC é armazenado cinco bytes depois do offset indicado (tipo `u8`, tamanho `u32`, ID `u32`). Os quatro eventos têm uma ação, e cada ação aponta para um **SwitchContainer, tipo 6**. Portanto, o Event ID não equivale a um Media ID único e incondicional.

| Evento | Action ID | SwitchContainer ID |
|---|---:|---:|
| Mewtwo | 211802658 | 262373802 |
| Pikachu | 530197658 | 42095902 |
| Bulbasaur | 418804259 | 702153992 |
| Wartortle | 17934557 | 271553487 |

`PM.bnk` realmente contém **1.749 registros DIDX**, mas esses quatro eventos estão em **`PM_VOICE.bnk`**. O segundo contém 258 objetos Event e 5.056 objetos HIRC no total (o script verifica a contagem real). Presença de mídia em um banco não identifica o nome do Event que a dispara.

## Conversão species → nome → ID

Três consumidores principais usam o mesmo literal:

* Construtor/componente `0x71003CAD18`, `snprintf` em **`0x71003CADF8`**: guarda a string de cry no objeto, em `this+0x180`.
* Cry espacial `0x710084F6F4`, `snprintf` em **`0x710084F75C`**, recebe o número em `w1`, transferido ao argumento de formato `w3`.
* Cry global `0x71009DF02C`, `snprintf` em **`0x71009DF094`**, recebe o número em `w0`.
* Helper de formatação `0x7102717428`, tail call `snprintf` em `0x7102717440`.

Em `0x71009DEC24`, o número enviado ao cry global vem do método virtual **`+0x1A8`** do objeto de dados Pokémon: carga do slot em `0x71009DEC90`, chamada em `0x71009DEC94`, depois `BL 0x71009DF02C` em `0x71009DECA4`. O form vem do slot **`+0x1B0`** (chamada `0x71009DEC50`), podendo ser substituído por um campo `u16` de configuração. O caminho espacial `0x710084F4D4` usa os mesmos slots, com chamada do getter species em `0x710084F66C` e dispatch do cry em `0x710084F684`. Isso não prova que species esteja em um offset direto e fixo de `FieldEntity`.

Antes do PostEvent, o jogo configura switches:

* `0x710084FB8C` constrói **`FORM%02hu`**, literal em `0x710311DFC2`; grupo literal **`FORM`** em `0x7103053722`.
* `0x710084FB00` configura o grupo **`PV`**, literal em `0x710311DFBF`; o valor vem de `0x710084FE70`.
* Valores PV 0–12 em tabela de registros de 24 bytes `0x7103D262B8`: `INDEX`, `SAD`, `ANGER`, `ATK`, `SPATK`, `GLAD1`, `GLAD2`, `GLAD3`, `EX1`, `EX2`, `EX3`, `EX4`, `ENV`. A tabela foi lida em `phase5-audio-pv-switch-table.json`.
* Valores 13–17 seguem um caminho de sorteio em `0x710084FE70`, não são índices diretos além da tabela.

## PostEvent / SetSwitch / SetRTPCValue

O jogo chega primeiro ao overload por string, que converte o nome em ID Wwise. O FNV de 64 bits da string da Trinity é um mecanismo separado e não deve ser truncado para construir um ID Wwise.

Grafo do cry espacial:

```text
0x710084F4D4
  -> espécie por vslot +0x1A8; form por vslot +0x1B0
  -> 0x710084F6F4
     -> snprintf("PLAY_PV_%04hu", species) @0x710084F75C
     -> 0x710084FB8C -> FORM%02hu -> SetSwitch
     -> 0x710084FB00 -> PV -> SetSwitch
     -> 0x710084F90C @0x710084F8AC
        -> 0x71003AD92C @0x710084F938
           -> 0x7101C3FA54
              -> virtual +0xD8 @0x7101C3FA7C
                 -> 0x7101C41610
                    -> PostEvent(name, ...) 0x710045F4A0
                       -> hash Wwise 32-bit
                       -> PostEvent(event_id, ...) 0x710045F590
```

O cry global `0x71009DF02C` chama o mesmo `0x710084F90C` em **`0x71009DF248`**. O retorno do núcleo é um playing ID; o wrapper espacial registra callback/dados da reprodução em `0x710084FC3C`, chamado em `0x710084F960` quando o playing ID não é zero.

A resolução virtual foi conferida no construtor **`0x7101C3E53C`**: objeto de backend de oito bytes, vptr **`0x7103A42788`** gravado em `0x7101C3E7FC`; o ponteiro é guardado no agregador de áudio em `+0x60`.

| Operação | Dispatcher da engine | Callsite indireto | Slot | Thunk do backend | Implementação identificada |
|---|---|---|---|---|---|
| PostEvent por nome | `0x7101C3FA54` | `0x7101C3FA7C` | `+0xD8` | `0x7101C41610` | `0x710045F4A0` |
| SetSwitch por nomes | `0x7101C3FC80` | `0x7101C3FC9C` | `+0xB0` | `0x7101C415D0` | `0x7100CD30D0` |
| SetRTPC por nome | `0x7101C3FC4C` | `0x7101C3FC6C` | `+0xA0` | `0x7101C415A8` | `0x7102F0DE80` |

Os thunks retiram `this` dos argumentos e fazem tail calls. Por exemplo, `0x7101C41634 -> 0x710045F4A0`.

## Hash Wwise comprovado no assembly

Em `0x710045F4A0`:

```text
0x710045F4B4/4B8: seed w0 = 0x811C9DC5
0x710045F4C8/4CC: prime w10 = 0x01000193
0x710045F4D4: MUL w11,w0,w10
0x710045F4D0..4E8: ASCII A..Z -> a..z
0x710045F4EC: EOR w0,w11,w9
0x710045F4F4: repete até NUL
0x710045F50C: tail B 0x710045F590 (sem external sources)
0x710045F568: BL 0x710045F590 (ramo com external sources)
```

Recorrência exata: `h = (h * 16777619) XOR ascii_lower(c)`, aritmética `u32`. É **FNV-1**, não FNV-1a. `0x7100CD30D0` aplica o mesmo algoritmo a grupo e valor do switch. O núcleo numérico **`0x710045F590`** procura `event_id` em uma tabela de eventos e enfileira um comando de reprodução; o campo `event_id` é escrito no comando em `+0x30`.

## Reprodução e cobertura

`python analysis/boot/phase5_audio_events.py`:

* Confere o literal no snapshot de `.rodata`.
* Lê chunks dos bancos sem decodificar ou tocar áudio.
* Calcula os quatro IDs pelo algoritmo extraído.
* Exige HIRC tipo 4 → Action tipo 3 → SwitchContainer tipo 6 para cada um.
* Grava `phase5_wwise_events.json`, incluindo SHA-256 do banco e offsets físicos.

Evidências headless em `phase5-audio-callers/`, `phase5-audio-dispatch/`, `phase5-audio-backend/`, `phase5-audio-init/`, `phase5-audio-interface/`, `phase5-audio-ak-api/` e `phase5-audio-species/`; assembly dos thunks em `phase5-audio-ak-thunks.txt`. Todas as sessões usaram a cópia `phase2-fs-project/PokemonZA` com `-readOnly -noanalysis`; alterações de análise foram descartadas.

O coletor decompilado às vezes incorpora funções vizinhas; os limites autoritativos são os intervalos de assembly explícitos, conferidos na `.eh_frame_hdr`. Não foi reconstruída uma única mídia default por espécie: o estado dos switches escolhe os descendentes. `unique_media_id` permanece `null` no JSON, sem confundir essa lacuna com os Event IDs já comprovados.
