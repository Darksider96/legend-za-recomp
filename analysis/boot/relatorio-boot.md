# Boot de exefs/main — nnMain e chamadas Nintendo

Análise estática em Ghidra 12.2 com SwitchLoader, sobre uma cópia do projeto existente `PokemonZA`. O executável do jogo não foi executado. Os endereços abaixo usam a base de análise `0x7100000000`; para outra base, use `base_do_main + RVA`. Esses RVAs não são offsets do arquivo NSO comprimido.

## Identidade e entradas

- Arquivo: `C:\Users\johna\OneDrive\Documentos\legend za\exefs\main`.
- Formato: NSO0, AArch64 little endian.
- SHA-256 do arquivo: `056eef7956b09414bfa0489b8de3ed0674393513f09a78d5312354cbd9557720`.
- SHA-256 de `.text` exportada do Ghidra: `996ff2d7ffa3954837b62475cdcb6605306beff39f3b2eed2bd3624ee2ab649b`. Coincide com o hash da seção registrado no cabeçalho NSO: os bytes de código analisados correspondem ao arquivo fornecido.
- `nnMain`: **`0x71019E8950`**, RVA **`0x019E8950`**, tamanho `0x310` pelo registro de unwind.
- `nninitStartup`: **`0x71019E8720`**, RVA **`0x019E8720`**, tamanho `0x22C`.

Ambos os nomes são símbolos presentes no programa importado, não nomes atribuídos por heurística. `nnMain` é a entrada da aplicação; não representa a primeira instrução de todo o processo, que também envolve `rtld`, `sdk` e inicializadores. A precedência convencional de `nninitStartup` sobre `nnMain` não foi medida por execução nesta análise.

## Preparação em nninitStartup

Esta tabela segue os locais de chamada na preparação de memória e alocadores, incluindo helpers alcançados a partir de `nninitStartup`. Há inicialização local protegida por guards e caminhos condicionais; a tabela não é um trace de execução.

| API | Instrução de chamada | Stub PLT | Evidência/contexto |
|---|---|---|---|
| `nn::os::QueryMemoryInfo(nn::os::MemoryInfo*)` | `0x71019E873C` | `0x7102FC5760` | Primeira chamada direta de `nninitStartup`; o resultado participa da seleção de configuração de memória. |
| `nn::os::InitializeReaderWriterLock(...)` | `0x7101650B4C` | `0x7102FC39C0` | Helper de memória: `nninitStartup → 0x7101650464 → 0x7101650B08`. |
| `nn::os::GetVirtualAddressMemoryResourceUsage()` | `0x71019F837C` | `0x7102FC5810` | Helper `0x71019F8310`, chamado em `0x71019E87A0`. |
| `nn::os::InitializeLightEvent(event, false, 1)` | `0x71005564B4` | `0x7102FC3A30` | Helper `0x7100556450`, chamado em `0x71019E8828` e `0x71019E88AC`. |
| `nn::os::SetMemoryAllocatorForThreadLocal(alloc, free)` | `0x71019E8910` | `0x7102FC58F0` | Callbacks `0x71019E8C60` e `0x71019E8C64`. |
| `nn::fs::SetAllocator(alloc, free)` | `0x71019E8924` | `0x7102FC5900` | Callbacks `0x71019E8C68` e `0x71019E8C84`; primeira chamada direta a `nn::fs` neste hook. |
| `nn::os::AllocateTlsSlot(slot, destructor)` | `0x7101A0A7AC` | `0x7102FC4770` | Inicialização sob guard: `0x71019E892C → 0x7101A0A864 → 0x7101A0A6B0 → 0x7101A0A7A0`. |
| `nn::os::InitializeMutex(mutex, false, 0)` | `0x7101A0A928` | `0x7102FC3B70` | Inicialização sob guard por `0x7101A0A6B0 → 0x7101A0A628 → 0x7101A0A910`. |

Os helpers de sincronização também contêm `WaitLightEvent`/`SignalLightEvent` condicionais. A seleção exata desses ramos depende do estado de execução.

## Começo de nnMain

As duas primeiras chamadas diretas são, nessa ordem:

```text
0x71019E896C  BL 0x7102FC5910  nn::os::GetHostArgc()
0x71019E8974  BL 0x7102FC5920  nn::os::GetHostArgv()
```

Depois, `0x71019E8980` chama novamente o helper de TLS `0x7101A0A864`; seus guards podem evitar repetir a inicialização feita antes. Em `0x71019E89A0`, o helper `0x710055C1B4` pode chamar `nn::os::InitializeLightEvent(..., false, 1)` em `0x710055C208`, PLT `0x7102FC3A30`.

No corpo de `nnMain`, `nn::os::WaitLightEvent` aparece em **`0x71019E8A18`**, PLT `0x7102FC3970`, e `nn::os::SignalLightEvent` em **`0x71019E8A8C`**, PLT `0x7102FC3980`. São ramos de contenção/sinalização, não chamadas incondicionais em todo boot.

## Primeira configuração de controles

Cadeia direta recuperada na construção do objeto da aplicação:

```text
nnMain
  [0x71019E8BBC] → 0x7100B10BD0
  [0x7100B10C00] → 0x7100B10C50
  [0x7100B10CB8] → 0x7100B10CFC
  [0x7100B10D0C] → 0x7100B10DA0
  [0x7100B10DD8] → 0x71019DC220
```

Nesse último construtor, o caminho de sucesso termina com:

| Ordem local | API/argumentos recuperados | Instrução de chamada | Stub PLT |
|---|---|---|---|
| 1 | `nn::hid::InitializeNpad()` | `0x71019DCA1C` | `0x7102FC4910` |
| 2 | `nn::hid::SetSupportedNpadIdType(ids, 2)`, `ids = {0, 0x20}` | `0x71019DCA30` | `0x7102FC4950` |
| 3 | `nn::hid::SetSupportedNpadStyleSet(0x7)` | `0x71019DCA38` | `0x7102FC4940` |

Os IDs vêm de uma gravação little endian de `0x0000002000000000` em dois elementos de 32 bits na pilha. A máscara `0x7` é passada em `w0`.

## Inicialização virtual e montagem de arquivos

Após a construção, `nnMain` chama `0x71019E31B0` em `0x71019E8BC4`. Essa função executa `BLR` em `0x71019E31D0`, usando o slot `+0x20` da vtable do objeto.

A atribuição do construtor `0x7100B10DA0` fornece a resolução estática:

```text
GOT [0x7103CC99E0] = 0x7103A04398
vptr do objeto     = 0x7103A043A8
[vptr + 0x20]      = 0x71005AEB60
```

Em `0x71005AECA0`, esse método chama `0x71019DD110`. O dispatcher percorre registros de 16 bytes entre `0x7103A38E18` e `0x7103A39038`, chamando os ponteiros de método em `0x71019DD140`. O registro **`0x7103A38E78`** contém `{0x71019DD280, 0}`; esse callback chama **`0x710193A498`** em **`0x71019DD2E8`**.

O caminho de montagem nesse helper contém, nessa ordem:

| API | Instrução de chamada | Stub PLT | Argumentos |
|---|---|---|---|
| `nn::fs::QueryMountRomCacheSize(size_t*)` | `0x710193A670` | `0x7102FC5600` | Ponteiro para variável na pilha. |
| `nn::fs::MountRom(const char*, void*, size_t)` | `0x710193A6A4` | `0x7102FC5610` | Nome **`"data"`**, buffer alocado e tamanho consultado. |

O nome está em `0x7103166A0E`. Entre as chamadas, o código aloca o buffer com o tamanho retornado, solicitando alinhamento `8`. O volume `data:` é, portanto, relevante para uma camada de compatibilidade com esse caminho de boot.

Mais adiante no mesmo método virtual, `0x71005AEE38 → 0x7101183038` contém **outra** chamada a `nn::hid::InitializeNpad()` em **`0x710118310C`**, para a mesma PLT `0x7102FC4910`. O código contém múltiplos locais de inicialização de Npad; não assuma uma chamada única.

## Evidências, ferramentas e limites

- `inventory.txt`: identidade do programa, mapeamento de seções e símbolos importados/exportados.
- `boot-verified.txt`: assembly e pseudocódigo de `nnMain`, construtores, callback de inicialização, montagem e helpers de memória.
- `boot-functions.txt`: `nninitStartup`, helpers de sincronização e entrada do loop da aplicação. Para os limites de `nnMain`, prefira `boot-verified.txt`.
- `subsystems.txt` e `application-init.txt`: helpers adicionais, incluindo TLS.
- `virtual-init.txt` e `virtual-tables.txt`: despacho virtual e dispatcher da tabela de callbacks.
- `init-callback-table.txt`: valores dos registros da tabela de callbacks.
- `boot-paths.json`: caminhos estáticos recuperados; a busca usa menor número de arestas, **não** ordem cronológica. O caminho mais curto para Npad nesse arquivo corresponde à segunda inicialização; a primeira está explicitada acima.
- `unwind-call-edges.txt`: locais BL/B decodificados dos bytes exportados, com limites de função verificados pelos registros de unwind.
- Scripts Java e Python usados estão nesta pasta.

O diagnóstico do REA 6.7.0 confirmou registro do cliente e Ghidra instalado, mas rejeitou Ghidra 12.2: essa versão do REA aceita Ghidra 12.1.x. Os MCPs REA não estavam expostos na sessão. Por isso, os resultados de desassemblagem/decompilação foram obtidos pelo Ghidra headless diretamente; não há Evidence IDs emitidos pelo REA para esses resultados. Nenhuma configuração do REA foi alterada.

As conclusões se apoiam nos símbolos, nas instruções e nos valores das tabelas. Assinaturas e estruturas inferidas pelo decompilador não devem ser tratadas como código-fonte original. A análise não cobre a sequência interna de `sdk`/`rtld`, todos os construtores globais ou uma ordem temporal entre threads. As chamadas listadas são APIs C++ do SDK; esta análise não recuperou IDs de IPC ou números de SVC correspondentes.
