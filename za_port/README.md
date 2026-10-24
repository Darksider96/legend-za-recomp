# Pokémon Legends: Z-A — Native Re-implementation Port (`za_port`)

Projeto em C++20 que recria a infraestrutura de execução, o Game Loop e a tabela de subsistemas do executável Nintendo Switch (`exefs/main`, AArch64) de **Pokémon Legends: Z-A**, mapeados via engenharia reversa estática no Ghidra.

---

## 🏛️ Arquitetura e Mapeamento de RVAs

Com base na análise estática de `exefs/main` (Base de análise `0x7100000000`):

| Símbolo / Componente | Endereço Ghidra | RVA | Descrição |
|---|---|---|---|
| `nninitStartup` | `0x71019E8720` | `0x019E8720` | Hook inicial do Switch: alocadores de memória, TLS e permissões. |
| `nnMain` | `0x71019E8950` | `0x019E8950` | Ponto de entrada da aplicação (`argc`, `argv`, criação do AppFramework). |
| `AppFramework::Construct` | `0x7100B10BD0` | `0x00B10BD0` | Instanciação e amarração da VTable da aplicação. |
| `AppFramework::RunLoop` | `0x71019E31B0` | `0x019E31B0` | Dispatcher do Game Loop principal: chama Init, PreLoop, Tick em loop e Finalize. |
| `AppFramework::Initialize` | `0x71005AEB60` | `0x005AEB60` | Slot `+0x20` da vtable: dispara o dispatching dos 34 subsistemas da engine. |
| `AppFramework::Tick` | `0x71019E32EC` | `0x019E32EC` | Slot `+0x18` da vtable: frame loop, dreno de eventos OE, medição de delta time e atualização de subsistemas. |
| `AppFramework::Finalize` | `0x710104624C` | `0x0104624C` | Slot `+0x28` da vtable: encerramento ordenado dos subsistemas. |
| `SubsystemDispatcher` | `0x71019DD110` | `0x019DD110` | Itera pela tabela de 34 registros de callbacks (`0x7103A38E18` a `0x7103A39038`). |
| `RomFs Mount Callback` | `0x71019DD280` | `0x019DD280` | Subsistema #06: chama helper `0x710193A498`, executa `QueryMountRomCacheSize` e `MountRom("data")`. |

---

## 🧩 Stubs e Módulos Implementados

### 1. `nn::os` (Sincronização, Memória e Temporização)
- **`MemoryInfo` / `QueryMemoryInfo`**: Simulação da alocação de memória Switch (4 GB RAM).
- **`SetMemoryAllocatorForThreadLocal`**: Registro de alocadores da thread principal.
- **`AllocateTlsSlot` / `FreeTlsSlot` / `GetTlsValue` / `SetTlsValue`**: Thread-Local Storage completo.
- **`MutexType`**: Mutex recursivo e simples com rastreamento de threads.
- **`ReaderWriterLockType`**: Leitura compartilhada e escrita exclusiva (`std::shared_mutex`).
- **`LightEventType`**: Eventos leves com modos `Manual` e `Auto` (usados no boot de `nnMain`).
- **`ConditionVariableType`**: Variáveis de condição integradas aos mutexes.
- **`GetSystemTick` / `ConvertToTimeSpan`**: Emulação do clock de hardware do Switch a 19.2 MHz.

### 2. `nn::fs` (Sistema de Arquivos e RomFS)
- **`MountRom("data", ...)`**: Montagem do volume de RomFS do jogo. Suporta mapeamento para pastas locais (`SetMountDirectory("data", "caminho")`).
- **`QueryMountRomCacheSize`**: Consulta de buffer de cache de montagem.
- **`MountSaveDataReadOnly` / `IsSaveDataExisting`**: Gerenciamento de save data.
- **`OpenFile` / `ReadFile` / `GetFileSize` / `CloseFile`**: Operações de I/O em arquivos virtuais.

### 3. `nn::hid` (Controles e Entrada)
- **`InitializeNpad()`**: Inicialização de Npad.
- **`SetSupportedNpadIdType`**: Configurado com os IDs mapeados `{No1, Handheld}` (`{0x0, 0x20}`).
- **`SetSupportedNpadStyleSet`**: Configurado com a máscara exata `0x7` (`FullKey | Handheld | JoyDual`).

### 4. `nn::oe` (Operating Environment)
- **`TryPopNotificationMessage`**: Dreno assíncrono de notificações do sistema operacional (mudança de foco, transição dock/portátil, modo de performance, suspensão).

### 5. `za::SubsystemRegistry` (Tabela de 34 Subsistemas da Engine)
Mapeia de forma 1:1 todos os 34 slots de inicialização encontrados na tabela `0x7103A38E18` .. `0x7103A39038`:
- `#00` Memory Heap
- `#01` Thread Pool
- `#03` System Timer
- `#04` OE Manager
- `#06` RomFS Mount Manager (`"data:"`)
- `#08` HID Controller Manager
- `#10` Graphics / Display
- `#11` NVN Device Adapter
- `#16` Physics & Collision
- `#24` Field / Terrain Streaming
- `#25` Battle Engine
- `#33` Root Scene Coordinator

---

## 🛠️ Como Compilar e Executar

### Pré-requisitos
- Compilador C++20 (MSVC 2019+, GCC 11+, Clang 13+, ou devkitA64 15.2.0)
- CMake 3.20 ou superior

### Compilação com CMake (Nativo Windows / Linux):
```bash
cd za_port
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Execução:
```bash
./za_port
```

O binário executará a sequência real de boot:
1. `nninitStartup()`
2. `nnMain()`
3. Construção de `GameApplication` (`AppFramework`)
4. Inicialização de todos os 34 subsistemas na ordem exata de `exefs/main`
5. Montagem do RomFS `"data:"`
6. Execução do Game Loop (`Tick`) com medição de ticks e dreno de mensagens OE
7. Encerramento gracioso em ordem reversa
