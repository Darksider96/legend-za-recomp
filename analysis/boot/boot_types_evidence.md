# Evidências de boot_types.h

O header reconstrói a ABI observada no `exefs/sdk` local (SHA-256 `0449487b457490afae879c08d6a90dd28295eaaa5c55677089a46b13991afbab`) e nos callers de `exefs/main`. Não foram recuperadas declarações originais de tipos/debug nem nomes originais dos membros. Campos desconhecidos ficam reservados; tamanhos totais e alinhamentos seguem o layout natural AArch64 dos acessos observados. Isso é uma reconstrução de ABI, não a extração de um header original do SDK.

Os endereços do SDK e do main usam bases de análise separadas, ambas `0x7100000000`. Não confundir seus espaços de endereçamento. O Ghidra headless usou o SwitchLoader já instalado; o REA continua indisponível nesta sessão pelas limitações registradas na investigação anterior.

| Tipo | Layout reconstruído | Evidência |
|---|---|---|
| `EventClearMode` | enum de 32 bits; `AutoClear=1`, contraparte manual `0` | SDK `0x7100138C44` compara `w2` com `1`; WaitLightEvent distingue consumir o sinal de preservá-lo. Os nomes dos enumeradores são descritivos. |
| `LightEventType` | tamanho `0x8`, alinhamento `4`; auto-clear em `0`, estado em `1`, palavra do sinal em `4` | SDK `0x7100138C40..0x7100138C64`; Finalize em `0x7100138C70`. O caller main escreve em `+2/+3` para seu próprio lock, por exemplo `0x710055C214/218`; esses bytes não comportam ponteiros nativos. |
| `MutexType` | tamanho `0x20`, alinhamento `8`; state `0`, recursive `1`, lock-level `4`, lock-count `8`, owner `0x10`, seção crítica `0x18` | SDK `0x710013C0A0..0x710013C0E4`, `LockMutex` em `0x710013C100`; o construtor interno `0x7100145EEC` zera uma palavra de 32 bits. Owner é carregado de `tpidrro_el0+0x1F8`, não um ID de thread de 32 bits. |
| `ReaderWriterLockType` | tamanho `0x30`, alinhamento `8`; lock agregado `0`, contador de leitura do writer `0xC`, state `0x10`, owner `0x18`, write-count `0x20`, condition variables `0x24/0x28` | Inicialização SDK `0x710013C280`; implementação de aquisição em `0x7100146210` e `0x7100146930`. Usa operações atômicas de 64 bits em `+0`; a inicialização preserva seu bit `63`. O significado de `+8` não foi estabelecido. |

O estado do LightEvent usa `0` para não sinalizado, `1` para espera pendente e `2` para sinalizado. O booleano auto-clear ocupa um byte; não inserir um enum de quatro bytes nesse campo. `InitializeLightEvent` não escreve nos bytes `+2/+3`. `InitializeMutex` e `InitializeReaderWriterLock` também não inicializam todo o padding.

## MountRom

Os argumentos do símbolo `_ZN2nn2fs8MountRomEPKcPvm` são `const char*`, `void*`, `size_t` de 64 bits. O status retornado usa 32 bits em `w0`; o header oferece um carrier trivial mínimo `nn::Result`, substituível pela definição já existente via macro `LEGEND_ZA_BOOT_TYPES_EXTERNAL_NN_RESULT`.

O caller em `main:0x710193A670` consulta o tamanho; a alocação em `main:0x710193A68C` recebe esse tamanho e alinhamento `8`; `main:0x710193A6A4` passa `"data"`, o buffer e o tamanho consultado. O overload com buffer rejeita ponteiro nulo no SDK, por exemplo `sdk:0x71000E2450`. O RomFS monta storages que referenciam o buffer; preserve seu tempo de vida enquanto o volume estiver montado.

O SDK fornece uma fórmula, não um tamanho fixo para todos os títulos:

```text
sdk:0x71000F5E50 lê 0x50 bytes do cabeçalho do IStorage.
raw = u64(header+0x10) + u64(header+0x20)
    + u64(header+0x30) + u64(header+0x40)
required = (raw + 7) & ~uint64_t(7)
sdk:0x71000E2FA0 retorna max(required, 0x20) no caminho RomFS.
```

O caminho alternativo de filesystem também aplica o mínimo `0x20`. A consulta exige um ponteiro de saída não nulo e só deve ser usada quando retornar sucesso. O tamanho específico desse conteúdo requer os metadados do RomFS; não foi determinado por execução nesta análise. O valor `64*1024` presente nos stubs atuais de `za_port` não vem dessa evidência. O mínimo `0x20` também não substitui a consulta. O alinhamento `8` registrado no header é o solicitado pelo caller observado; não é uma alegação de validação universal do alinhamento por todos os overloads.

## Inclusão e validação

Use `boot_types.h` como definição autoritativa desses três tipos no código que precisar reproduzir a ABI do jogo. Os structs de `za_port/include/nn/os.h` e `os.hpp` contêm ponteiros/contadores em posições diferentes e não podem ser incluídos juntamente com essas definições. Nenhum arquivo existente de `za_port` foi modificado.

Para reutilizar o `nn::Result` existente:

```cpp
#include "nn/types.h"
#define LEGEND_ZA_BOOT_TYPES_EXTERNAL_NN_RESULT
#include "boot_types.h"
```

Validado com devkitA64 GCC 15.2.0, C++17, `-Wall -Wextra -Werror -pedantic`. Os `static_assert` de tamanho/alinhamento/offset e propriedades triviais passaram. Nove referências de funções foram compiladas para AArch64; seus nomes Itanium coincidem com os imports de `main`. O modo com `nn::Result` de `za_port/include/nn/types.h` também passou em compilação. Isso valida a representação C++ escolhida, não substitui testes de comportamento ou prova de uma declaração original ausente.

Arquivos de evidência: `sdk-types-evidence.txt`, `sdk-lock-details.txt`, `sdk-rom-details.txt`, `sdk-rom-cache.txt`, `boot_types_symbols.txt`, `boot_types_validation.txt` e o probe `boot_types_compile_check.cpp`.
