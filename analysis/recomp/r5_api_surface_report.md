# Superfície de API para planejamento da R5

Análise estática de importações e callsites; recomendações são insumos para decisão do Claude Code. Nenhum runtime ou inventário anterior foi alterado. O SDK continua sendo recompilado conforme o manifesto.

## Resultado e critérios de contagem

- Importacoes: 310; sem stub: 0; sem alcance: 0
- Chamadas do censo: 31220; sites na imagem: 31230; adicionais fora dos FDEs: 10
- FS selecionadas: 38 chamadas; strings recuperadas: 3; sem string: 35
- Montagens literais distintas: 2; prefixos de caminho distintos: 1

- `Chamadas` nas tabelas é exatamente a soma em `call_targets.csv` para os stubs da importação. O censo original só contava funções FDE. `Imagem` inclui também as funções propostas em `gap_functions.csv`; esses totais permanecem separados. Funções chamadoras distintas usam ambos os inventários.
- API observada significa uma importação com pelo menos um callsite na imagem; API importada pode ter zero chamadas diretas e ainda ser usada por ponteiro. Chamadores de uma área são uma união, não a soma dos chamadores de cada API.
- Alcance de serviços/SVCs é herdado de `import_kernel_reach.csv`, limitado ao grafo direto do SDK até profundidade 12. Não prova execução nem pureza; chamadas indiretas, callbacks e caminhos dependentes de argumentos não estão completos.
- As tabelas mostram nomes do catálogo público de serviços; os campos `servicos` e `candidatos_formato` do CSV preservam também as strings heurísticas do censo. Uma string de formato compatível não é automaticamente serviço IPC. Todos os nomes e números de alcance foram confrontados com `service_names.csv` e `svc_sites.csv`.
- Referências públicas usadas como apoio ao critério: [cliente FS libnx](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/services/fs.c) usa sessões/domínios; [cliente HID libnx](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/services/hid.c) combina serviço e memória compartilhada. Esses clientes não demonstram que os layouts desta versão do SDK sejam idênticos.

## Recomendação por área

Regra de planejamento: overrides de APIs quando a superfície observada é delimitada e mapeia diretamente ao backend do host; manter o SDK quando há muitas APIs relacionadas a estado/layout compartilhado. Para `nn::os`, a opção `ipc` é somente a categoria manter SDK/fronteira SVC; não há afirmação de um serviço IPC nn::os. A recomendação cobre as importações observadas; chamadas internas do SDK podem exigir cobertura adicional.

| Área | Chamadas | Imagem | APIs observadas/importadas | Chamadores | Serviços do catálogo | Recomendação | Critério |
|---|---:|---:|---:|---:|---|---|---|
| nn::fs | 124 | 133 | 32/32 | 62 | fsp-srv, sm: | api | 32 APIs observadas: backend de arquivos nativo e montagem por nome oferecem uma fronteira menor que reproduzir sessoes, dominios e objetos do protocolo FS |
| nn::hid | 110 | 110 | 53/53 | 77 | hid, sm: | ipc | 53 APIs observadas: preservar polling e estado do SDK usando servico HID e memoria compartilhada reduz a superficie de overrides |
| nn::oe + nn::applet | 32 | 32 | 19/19 | 25 | appletOE, sm: | api | 19 APIs observadas: superficie de ciclo de vida delimitada permite mapear o estado de aplicacao e janela do host sem reproduzir o protocolo completo de applet |
| nn::account | 64 | 64 | 25/25 | 23 | acc:u0, sm: | api | 25 APIs observadas: centralizar usuarios e handles em uma implementacao nativa das APIs observadas limita o protocolo de conta necessario para R5 |
| nn::time | 50 | 50 | 18/18 | 25 | sm:, time:a, time:r, time:s, time:su, time:u | api | 18 APIs observadas: mapear relogios e conversoes observados para o host evita construir a infraestrutura de servico de horario nesta etapa |
| nn::settings | 16 | 16 | 2/2 | 1 | — | api | 2 APIs observadas: poucas APIs observadas de configuracao favorecem overrides de valores e formatos documentados pelos argumentos |
| nn::err | 3056 | 3056 | 18/18 | 1082 | — | api | 18 APIs observadas: mapear construcao e apresentacao dos erros observados para o host evita depender de UI de applet para diagnosticos |
| nn::nifm | 15 | 16 | 7/7 | 10 | nifm:a, nifm:s, sm: | api | 7 APIs observadas: superficie pequena permite modelar o estado de rede do host por API antes de implementar sessoes IPC de rede |
| nn::aoc | 4 | 4 | 3/3 | 3 | — | api | 3 APIs observadas: superficie pequena permite consultar o catalogo de conteudo instalado por API; ausencia de servico no grafo nao e prova de ausencia de IPC |
| nn::pl | 5 | 5 | 4/4 | 5 | pl:u, sm: | api | 4 APIs observadas: poucas APIs de fontes permitem fornecer buffers nativos com ABI e vida util verificadas sem reproduzir o servico completo |
| nn::vi | 14 | 14 | 7/7 | 4 | sm:, vi:m, vi:s, vi:u | api | 7 APIs observadas: poucas APIs de display podem compartilhar janela e handles com o backend grafico nativo |
| nn::audio | 30 | 30 | 20/20 | 14 | audout:u, audren:u, sm: | api | 20 APIs observadas: mapear as APIs observadas para o backend de audio nativo evita reproduzir o protocolo do renderer; requer cobrir callbacks e estado alem das importacoes diretas |
| nn::os | 27700 | 27700 | 102/102 | 11973 | — | ipc | referencia de volume: manter o SDK e implementar a fronteira SVC para preservar objetos e sincronizacao; aqui ipc designa manter o SDK, nao um protocolo de servico nn::os |

## Importações por área

`Chamadores` conta funções distintas do main; `+lacunas` são sites adicionais fora da contagem FDE. SVCs são números locais do Horizon, identificados em `svc_sites.csv`.

### nn::fs

| Importação (demangled) | Chamadas | +lacunas | Chamadores | Serviços do catálogo | SVCs |
|---|---:|---:|---:|---|---|
| nn::fs::Unmount(char const*) | 15 | 0 | 6 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::CloseFile(nn::fs::FileHandle) | 14 | 1 | 15 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::FlushFile(nn::fs::FileHandle) | 13 | 0 | 13 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::OpenFile(nn::fs::FileHandle*, char const*, int) | 12 | 1 | 8 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::GetFileSize(long*, nn::fs::FileHandle) | 11 | 1 | 11 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::GetEntryType(nn::fs::DirectoryEntryType*, char const*) | 7 | 1 | 8 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::WriteFile(nn::fs::FileHandle, long, void const*, unsigned long, nn::fs::WriteOption const&) | 7 | 0 | 6 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::SetFileSize(nn::fs::FileHandle, long) | 6 | 0 | 4 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::CreateFile(char const*, long) | 5 | 0 | 4 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::ReadFile(unsigned long*, nn::fs::FileHandle, long, void*, unsigned long) | 5 | 1 | 6 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::ReadFile(nn::fs::FileHandle, long, void*, unsigned long) | 4 | 0 | 4 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::MountSaveData(char const*, nn::account::Uid const&) | 3 | 0 | 2 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::CreateDirectory(char const*) | 2 | 0 | 2 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::DeleteFile(char const*) | 2 | 0 | 2 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::SetCurrentThreadFsContext(nn::fs::FsContext const*) | 2 | 0 | 1 | — | — |
| nn::fs::CloseDirectory(nn::fs::DirectoryHandle) | 1 | 1 | 2 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::Commit(char const*) | 1 | 0 | 1 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::DeleteDirectoryRecursively(char const*) | 1 | 0 | 1 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::EnsureSaveData(nn::account::Uid const&) | 1 | 0 | 1 | fsp-srv, sm: | 0xb, 0x15, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36, 0x52 |
| nn::fs::GetCurrentThreadFsContext() | 1 | 0 | 1 | — | — |
| nn::fs::IsSaveDataExisting(nn::ApplicationId, nn::account::Uid const&) | 1 | 0 | 1 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::IsSaveDataExisting(nn::account::Uid const&) | 1 | 0 | 1 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::MountAddOnContent(char const*, int, void*, unsigned long) | 1 | 0 | 1 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::MountRom(char const*) | 1 | 0 | 1 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::MountRom(char const*, void*, unsigned long) | 1 | 0 | 1 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::MountSaveDataReadOnly(char const*, nn::ApplicationId, nn::account::Uid const&) | 1 | 0 | 1 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::OpenDirectory(nn::fs::DirectoryHandle*, char const*, int) | 1 | 1 | 2 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::QueryMountAddOnContentCacheSize(unsigned long*, int) | 1 | 0 | 1 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::QueryMountRomCacheSize(unsigned long*) | 1 | 0 | 1 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::ReadDirectory(long*, nn::fs::DirectoryEntry*, nn::fs::DirectoryHandle, long) | 1 | 1 | 2 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::SetAllocator(void* (*)(unsigned long), void (*)(void*, unsigned long)) | 1 | 0 | 1 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::fs::GetDirectoryEntryCount(long*, nn::fs::DirectoryHandle) | 0 | 1 | 1 | fsp-srv, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |

### nn::hid

| Importação (demangled) | Chamadas | +lacunas | Chamadores | Serviços do catálogo | SVCs |
|---|---:|---:|---:|---|---|
| nn::hid::GetNpadControllerColor(nn::hid::NpadControllerColor*, nn::hid::NpadControllerColor*, unsigned int const&) | 11 | 0 | 9 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::GetNpadStyleSet(unsigned int const&) | 7 | 0 | 6 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::GetNpadControllerColor(nn::hid::NpadControllerColor*, unsigned int const&) | 6 | 0 | 5 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::StopSixAxisSensor(nn::hid::SixAxisSensorHandle const&) | 6 | 0 | 2 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::StartSixAxisSensor(nn::hid::SixAxisSensorHandle const&) | 5 | 0 | 2 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::BindNpadStyleSetUpdateEvent(unsigned int const&, nn::os::SystemEventType*, nn::os::EventClearMode) | 3 | 0 | 2 | hid, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::GetSixAxisSensorState(nn::hid::SixAxisSensorState*, nn::hid::SixAxisSensorHandle const&) | 3 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::InitializeNpad() | 3 | 0 | 3 | hid, sm: | 0xb, 0x13, 0x14, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::SetSupportedNpadIdType(unsigned int const*, unsigned long) | 3 | 0 | 3 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::SetSupportedNpadStyleSet(nn::util::BitFlagSet<32, nn::hid::NpadStyleTag>) | 3 | 0 | 3 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::EnableSixAxisSensorFusion(nn::hid::SixAxisSensorHandle const&, bool) | 2 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::GetGyroscopeZeroDriftMode(nn::hid::SixAxisSensorHandle const&) | 2 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::GetNpadJoyAssignment(unsigned int const&) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::GetNpadState(nn::hid::NpadFullKeyState*, unsigned int const&) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::GetNpadState(nn::hid::NpadHandheldState*, unsigned int const&) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::GetNpadState(nn::hid::NpadJoyDualState*, unsigned int const&) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::GetNpadState(nn::hid::NpadJoyLeftState*, unsigned int const&) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::GetNpadState(nn::hid::NpadJoyRightState*, unsigned int const&) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::GetSixAxisSensorFusionParameters(float*, float*, nn::hid::SixAxisSensorHandle const&) | 2 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::GetSixAxisSensorStates(nn::hid::SixAxisSensorState*, int, nn::hid::SixAxisSensorHandle const&) | 2 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::IsSixAxisSensorAtRest(nn::hid::SixAxisSensorHandle const&) | 2 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::IsSixAxisSensorFusionEnabled(nn::hid::SixAxisSensorHandle const&) | 2 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::SendVibrationValue(nn::hid::VibrationDeviceHandle const&, nn::hid::VibrationValue const&) | 2 | 0 | 2 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::SetGyroscopeZeroDriftMode(nn::hid::SixAxisSensorHandle const&, nn::hid::GyroscopeZeroDriftMode const&) | 2 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::SetNpadHandheldActivationMode(nn::hid::NpadHandheldActivationMode) | 2 | 0 | 2 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::SetNpadJoyHoldType(nn::hid::NpadJoyHoldType) | 2 | 0 | 2 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::SetSixAxisSensorFusionParameters(nn::hid::SixAxisSensorHandle const&, float, float) | 2 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::DisconnectNpad(unsigned int const&) | 1 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::GetNpadCommunicationMode() | 1 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::GetNpadHandheldActivationMode() | 1 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::GetNpadJoyHoldType() | 1 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::GetNpadStates(nn::hid::NpadFullKeyState*, int, unsigned int const&) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::GetNpadStates(nn::hid::NpadHandheldState*, int, unsigned int const&) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::GetNpadStates(nn::hid::NpadJoyDualState*, int, unsigned int const&) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::GetNpadStates(nn::hid::NpadJoyLeftState*, int, unsigned int const&) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::GetNpadStates(nn::hid::NpadJoyRightState*, int, unsigned int const&) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::GetPlayerLedPattern(unsigned int const&) | 1 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::GetSixAxisSensorHandles(nn::hid::SixAxisSensorHandle*, int, unsigned int const&, nn::util::BitFlagSet<32, nn::hid::NpadStyleTag>) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::GetSupportedNpadStyleSet() | 1 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::GetVibrationDeviceHandles(nn::hid::VibrationDeviceHandle*, int, unsigned int const&, nn::util::BitFlagSet<32, nn::hid::NpadStyleTag>) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::hid::InitializeTouchScreen(int, int) | 1 | 0 | 1 | hid, sm: | 0xb, 0x13, 0x14, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::InitializeVibrationDevice(nn::hid::VibrationDeviceHandle const&) | 1 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::MergeSingleJoyAsDualJoy(unsigned int const&, unsigned int const&) | 1 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::SetNpadCommunicationMode(nn::hid::NpadCommunicationMode) | 1 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::SetNpadJoyAssignmentModeDual(unsigned int const&) | 1 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::SetNpadJoyAssignmentModeSingle(unsigned int const&) | 1 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::SetNpadJoyAssignmentModeSingle(unsigned int const&, nn::hid::NpadJoyDeviceType) | 1 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::SetNpadJoyAssignmentModeSingle(unsigned int*, unsigned int const&, nn::hid::NpadJoyDeviceType) | 1 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::ShowControllerStrapGuide() | 1 | 0 | 1 | hid, sm: | 0xb, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::SixAxisSensorState::GetQuaternion(nn::util::neon::Vector4fType*) const | 1 | 0 | 1 | — | — |
| nn::hid::StartLrAssignmentMode() | 1 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::StopLrAssignmentMode() | 1 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::hid::SwapNpadAssignment(unsigned int const&, unsigned int const&) | 1 | 0 | 1 | hid, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |

### nn::oe + nn::applet

| Importação (demangled) | Chamadas | +lacunas | Chamadores | Serviços do catálogo | SVCs |
|---|---:|---:|---:|---|---|
| nn::oe::SetUserInactivityDetectionTimeExtended(bool) | 6 | 0 | 6 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::oe::SetCpuBoostMode(nn::oe::CpuBoostMode) | 4 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::oe::SetPerformanceConfiguration(nn::oe::PerformanceMode, int) | 3 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::oe::GetDesiredLanguage() | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::oe::LeaveExitRequestHandlingSection() | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::oe::SetFocusHandlingMode(nn::oe::FocusHandlingMode) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::oe::DisableRecording() | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::oe::DisableScreenShot() | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::oe::EnableRecording() | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::oe::EnableScreenShot() | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::oe::EnterExitRequestHandlingSection() | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::oe::FinishStartupLogo() | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::oe::GetCurrentFocusState() | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::oe::GetPerformanceMode() | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::oe::GetPseudoDeviceId(nn::util::Uuid*) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::oe::Initialize() | 1 | 0 | 1 | appletOE, sm: | 0x5, 0x8, 0x9, 0xb, 0xf, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x25, 0x26, 0x29, 0x36 |
| nn::oe::IsUserInactivityDetectionTimeExtended() | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::oe::SetPerformanceModeChangedNotificationEnabled(bool) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::oe::TryPopNotificationMessage(unsigned int*) | 1 | 0 | 1 | — | 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |

### nn::account

| Importação (demangled) | Chamadas | +lacunas | Chamadores | Serviços do catálogo | SVCs |
|---|---:|---:|---:|---|---|
| nn::account::AsyncContext::GetResult() | 7 | 0 | 7 | — | — |
| nn::account::AsyncContext::~AsyncContext() | 7 | 0 | 5 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::account::AsyncContext::HasDone(bool*) | 6 | 0 | 6 | — | — |
| nn::account::AsyncContext::AsyncContext() | 5 | 0 | 5 | — | — |
| nn::account::AsyncContext::~AsyncContext() | 4 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::account::GetNetworkServiceAccountId(nn::account::NetworkServiceAccountId*, nn::account::UserHandle const&) | 4 | 0 | 4 | — | — |
| nn::account::GetUserId(nn::account::Uid*, nn::account::UserHandle const&) | 4 | 0 | 4 | — | — |
| nn::account::EnsureNetworkServiceAccountIdTokenCacheAsync(nn::account::AsyncContext*, nn::account::UserHandle const&) | 3 | 0 | 3 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::account::Initialize() | 3 | 0 | 3 | acc:u0, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::account::LoadNetworkServiceAccountIdTokenCache(unsigned long*, char*, unsigned long, nn::account::UserHandle const&) | 3 | 0 | 3 | — | — |
| nn::account::AsyncContext::GetSystemEvent(nn::os::SystemEvent*) | 2 | 0 | 2 | — | 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::account::AsyncContext::operator=(nn::account::AsyncContext&&) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::account::EnsureNetworkServiceAccountAvailable(nn::account::UserHandle const&) | 2 | 0 | 2 | — | 0xb, 0x15, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36, 0x52 |
| nn::account::AsyncContext::Cancel() | 1 | 0 | 1 | — | — |
| nn::account::AsyncNetworkServiceLicenseInfoContext::AsyncNetworkServiceLicenseInfoContext() | 1 | 0 | 1 | — | — |
| nn::account::AsyncNetworkServiceLicenseInfoContext::AsyncNetworkServiceLicenseInfoContext(nn::account::AsyncNetworkServiceLicenseInfoContext&&) | 1 | 0 | 1 | — | — |
| nn::account::AsyncNetworkServiceLicenseInfoContext::GetNetworkServiceLicenseKind() | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::account::AsyncNetworkServiceLicenseInfoContext::operator=(nn::account::AsyncNetworkServiceLicenseInfoContext&&) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::account::CloseUser(nn::account::UserHandle const&) | 1 | 0 | 1 | — | — |
| nn::account::GetNickname(nn::account::Nickname*, nn::account::Uid const&) | 1 | 0 | 1 | — | — |
| nn::account::GetUserExistence(bool*, nn::account::Uid const&) | 1 | 0 | 1 | — | — |
| nn::account::IsNetworkServiceAccountAvailable(bool*, nn::account::UserHandle const&) | 1 | 0 | 1 | — | — |
| nn::account::LoadNetworkServiceLicenseInfoAsync(nn::account::AsyncNetworkServiceLicenseInfoContext*, nn::account::UserHandle const&) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::account::ShowLicenseRequirementsForNetworkService(nn::account::UserHandle const&) | 1 | 0 | 1 | — | 0xb, 0x15, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36, 0x52 |
| nn::account::TryOpenPreselectedUser(nn::account::UserHandle*) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |

### nn::time

| Importação (demangled) | Chamadas | +lacunas | Chamadores | Serviços do catálogo | SVCs |
|---|---:|---:|---:|---|---|
| nn::time::ToCalendarTime(nn::time::CalendarTime*, nn::time::CalendarAdditionalInfo*, nn::time::PosixTime const&) | 8 | 0 | 8 | — | — |
| nn::time::Finalize() | 6 | 0 | 4 | — | 0x14, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::time::Initialize() | 6 | 0 | 6 | sm:, time:a, time:r, time:s, time:su, time:u | 0xb, 0x13, 0x14, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::time::IsInitialized() | 5 | 0 | 5 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::time::StandardUserSystemClock::GetCurrentTime(nn::time::PosixTime*) | 5 | 0 | 5 | — | — |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::~AsyncEnsureNetworkClockAvailabilityTask() | 3 | 0 | 1 | — | 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::time::StandardNetworkSystemClock::GetCurrentTime(nn::time::PosixTime*) | 3 | 0 | 3 | — | — |
| nn::time::GetDeviceLocationName(nn::time::LocationName*) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::time::LoadTimeZoneRule(nn::time::TimeZoneRule*, nn::time::LocationName const&) | 2 | 0 | 2 | — | — |
| nn::time::ToCalendarTimeInUtc(nn::time::PosixTime const&) | 2 | 0 | 2 | — | — |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::AsyncEnsureNetworkClockAvailabilityTask() | 1 | 0 | 1 | — | — |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::GetFinishEvent() | 1 | 0 | 1 | — | — |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::GetResult() | 1 | 0 | 1 | — | — |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::Initialize(nn::os::EventClearMode) | 1 | 0 | 1 | sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::StartTask() | 1 | 0 | 1 | — | — |
| nn::time::IsLeapYear(int) | 1 | 0 | 1 | — | — |
| nn::time::IsValidDate(int, int, int) | 1 | 0 | 1 | — | — |
| nn::time::ToPosixTime(int*, nn::time::PosixTime*, int, nn::time::CalendarTime const&, nn::time::TimeZoneRule const&) | 1 | 0 | 1 | — | — |

### nn::settings

| Importação (demangled) | Chamadas | +lacunas | Chamadores | Serviços do catálogo | SVCs |
|---|---:|---:|---:|---|---|
| nn::settings::GetFullKeyGenericPadMap(nn::settings::FullKeyGenericPadMap*, nn::settings::FullKeyIndex) | 8 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::settings::SetFullKeyGenericPadMap(nn::settings::FullKeyGenericPadMap const&, nn::settings::FullKeyIndex) | 8 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |

### nn::err

| Importação (demangled) | Chamadas | +lacunas | Chamadores | Serviços do catálogo | SVCs |
|---|---:|---:|---:|---|---|
| nn::err::ErrorResultVariant::ErrorResultVariant(nn::Result const&) | 3025 | 0 | 1073 | — | — |
| nn::err::ErrorResultVariant::operator nn::err::ErrorCode() const | 5 | 0 | 3 | — | — |
| nn::err::ErrorCode::IsValid() const | 3 | 0 | 3 | — | — |
| nn::err::ErrorResultVariant::ErrorResultVariant() | 3 | 0 | 3 | — | — |
| nn::err::ErrorResultVariant::GetState() const | 3 | 0 | 2 | — | — |
| nn::err::ErrorResultVariant::operator=(nn::err::ErrorCode const&) | 3 | 0 | 2 | — | — |
| nn::err::ErrorResultVariant::operator nn::Result() const | 2 | 0 | 2 | — | — |
| nn::err::ErrorResultVariant::operator=(nn::Result const&) | 2 | 0 | 2 | — | — |
| nn::err::ApplicationErrorArg::ApplicationErrorArg() | 1 | 0 | 1 | — | — |
| nn::err::ApplicationErrorArg::SetApplicationErrorCodeNumber(unsigned int) | 1 | 0 | 1 | — | — |
| nn::err::ApplicationErrorArg::SetDialogMessage(char const*) | 1 | 0 | 1 | — | — |
| nn::err::ApplicationErrorArg::SetFullScreenMessage(char const*) | 1 | 0 | 1 | — | — |
| nn::err::ApplicationErrorArg::SetLanguageCode(nn::settings::LanguageCode const&) | 1 | 0 | 1 | — | — |
| nn::err::NplnContext::Make(void const*, unsigned long) | 1 | 0 | 1 | — | — |
| nn::err::ShowApplicationError(nn::err::ApplicationErrorArg const&) | 1 | 0 | 1 | — | 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::err::ShowError(nn::Result) | 1 | 0 | 1 | — | 0xb, 0x15, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36, 0x52 |
| nn::err::ShowError(nn::err::ErrorCode) | 1 | 0 | 1 | — | 0xb, 0x15, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36, 0x52 |
| nn::err::ShowError(nn::err::ErrorResultVariant const&) | 1 | 0 | 1 | — | 0xb, 0x15, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36, 0x52 |

### nn::nifm

| Importação (demangled) | Chamadas | +lacunas | Chamadores | Serviços do catálogo | SVCs |
|---|---:|---:|---:|---|---|
| nn::nifm::IsNetworkAvailable() | 6 | 0 | 5 | — | 0x12, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::nifm::GetCurrentIpConfigInfo(in_addr*, in_addr*, in_addr*, in_addr*, in_addr*) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::nifm::Initialize() | 2 | 0 | 2 | nifm:a, nifm:s, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 |
| nn::nifm::SubmitNetworkRequestAndWait() | 2 | 0 | 1 | — | 0x12, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::nifm::GetCurrentProxySetting(nn::nifm::ProxySetting*) | 1 | 1 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::nifm::HandleNetworkRequestErrorResult() | 1 | 0 | 1 | — | 0x12, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::nifm::SetLocalNetworkMode(bool) | 1 | 0 | 1 | — | 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |

### nn::aoc

| Importação (demangled) | Chamadas | +lacunas | Chamadores | Serviços do catálogo | SVCs |
|---|---:|---:|---:|---|---|
| nn::aoc::ListAddOnContent(int*, int, int) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::aoc::CountAddOnContent() | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::aoc::GetAddOnContentListChangedEvent(nn::os::SystemEvent*) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |

### nn::pl

| Importação (demangled) | Chamadas | +lacunas | Chamadores | Serviços do catálogo | SVCs |
|---|---:|---:|---:|---|---|
| nn::pl::RequestSharedFontLoad(nn::pl::SharedFontType) | 2 | 0 | 2 | pl:u, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::pl::GetSharedFontAddress(nn::pl::SharedFontType) | 1 | 0 | 1 | pl:u, sm: | 0xb, 0x13, 0x14, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::pl::GetSharedFontLoadState(nn::pl::SharedFontType) | 1 | 0 | 1 | pl:u, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::pl::GetSharedFontSize(nn::pl::SharedFontType) | 1 | 0 | 1 | pl:u, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |

### nn::vi

| Importação (demangled) | Chamadas | +lacunas | Chamadores | Serviços do catálogo | SVCs |
|---|---:|---:|---:|---|---|
| nn::vi::CloseDisplay(nn::vi::Display*) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::vi::CreateLayer(nn::vi::Layer**, nn::vi::Display*) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::vi::DestroyLayer(nn::vi::Layer*) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::vi::Finalize() | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::vi::GetNativeWindow(void**, nn::vi::Layer*) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::vi::Initialize() | 2 | 0 | 2 | sm:, vi:m, vi:s, vi:u | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::vi::OpenDefaultDisplay(nn::vi::Display**) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |

### nn::audio

| Importação (demangled) | Chamadas | +lacunas | Chamadores | Serviços do catálogo | SVCs |
|---|---:|---:|---:|---|---|
| nn::audio::GetAudioOutBufferDataPointer(nn::audio::AudioOutBuffer const*) | 4 | 0 | 4 | — | — |
| nn::audio::GetActiveAudioDeviceChannelCountForOutput() | 3 | 0 | 3 | audren:u, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::audio::AcquireAudioDeviceNotificationForOutput(nn::os::SystemEvent*) | 2 | 0 | 2 | audren:u, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::audio::AppendAudioOutBuffer(nn::audio::AudioOut*, nn::audio::AudioOutBuffer*) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::audio::GetActiveAudioDeviceName(nn::audio::AudioDeviceName*) | 2 | 0 | 2 | audren:u, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::audio::GetActiveAudioOutputDeviceName(nn::audio::AudioDeviceName*) | 2 | 0 | 2 | audren:u, sm: | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::audio::SetAudioOutBufferInfo(nn::audio::AudioOutBuffer*, void*, unsigned long, unsigned long) | 2 | 0 | 2 | — | — |
| nn::audio::CloseAudioOut(nn::audio::AudioOut*) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::audio::GetAudioOutBufferCount(nn::audio::AudioOut const*) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::audio::GetAudioOutChannelCount(nn::audio::AudioOut const*) | 1 | 0 | 1 | — | — |
| nn::audio::GetAudioOutState(nn::audio::AudioOut const*) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::audio::GetReleasedAudioOutBuffer(nn::audio::AudioOut*) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::audio::GetRequiredBufferSizeForResampler(int) | 1 | 0 | 1 | — | — |
| nn::audio::GetResamplerOutputSampleCount(nn::audio::ResamplerType const*, int) | 1 | 0 | 1 | — | — |
| nn::audio::InitializeAudioOutParameter(nn::audio::AudioOutParameter*) | 1 | 0 | 1 | — | — |
| nn::audio::InitializeResampler(nn::audio::ResamplerType*, void*, unsigned long, int, int, int) | 1 | 0 | 1 | — | — |
| nn::audio::OpenDefaultAudioOut(nn::audio::AudioOut*, nn::os::SystemEvent*, nn::audio::AudioOutParameter const&) | 1 | 0 | 1 | audout:u, sm: | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 |
| nn::audio::ProcessResamplerBuffer(nn::audio::ResamplerType*, int*, short*, unsigned long, short const*, int) | 1 | 0 | 1 | — | — |
| nn::audio::StartAudioOut(nn::audio::AudioOut*) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::audio::StopAudioOut(nn::audio::AudioOut*) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |

### nn::os

| Importação (demangled) | Chamadas | +lacunas | Chamadores | Serviços do catálogo | SVCs |
|---|---:|---:|---:|---|---|
| nn::os::SignalLightEvent(nn::os::LightEventType*) | 8444 | 0 | 6630 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x35, 0x36 |
| nn::os::WaitLightEvent(nn::os::LightEventType*) | 6368 | 0 | 5016 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x34, 0x36 |
| nn::os::FinalizeLightEvent(nn::os::LightEventType*) | 2120 | 0 | 1416 | — | — |
| nn::os::UnlockMutex(nn::os::MutexType*) | 1988 | 0 | 1490 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::LockMutex(nn::os::MutexType*) | 1741 | 0 | 1472 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::InitializeLightEvent(nn::os::LightEventType*, bool, nn::os::EventClearMode) | 1560 | 0 | 737 | — | — |
| nn::os::GetCurrentThread() | 1063 | 0 | 855 | — | — |
| nn::os::FinalizeMutex(nn::os::MutexType*) | 521 | 0 | 470 | — | — |
| nn::os::InitializeMutex(nn::os::MutexType*, bool, int) | 470 | 0 | 397 | — | — |
| nn::os::SignalEvent(nn::os::EventType*) | 321 | 0 | 295 | — | 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::FinalizeEvent(nn::os::EventType*) | 318 | 0 | 317 | — | — |
| nn::os::InitializeEvent(nn::os::EventType*, bool, nn::os::EventClearMode) | 275 | 0 | 251 | — | — |
| nn::os::ReleaseReadLock(nn::os::ReaderWriterLockType*) | 270 | 0 | 190 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::GetSystemTick() | 251 | 0 | 117 | — | — |
| nn::os::ReleaseWriteLock(nn::os::ReaderWriterLockType*) | 229 | 0 | 164 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::AcquireReadLock(nn::os::ReaderWriterLockType*) | 217 | 0 | 190 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::AcquireWriteLock(nn::os::ReaderWriterLockType*) | 194 | 0 | 153 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::SignalConditionVariable(nn::os::ConditionVariableType*) | 124 | 0 | 90 | — | 0x1d |
| nn::os::WaitEvent(nn::os::EventType*) | 84 | 0 | 67 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::AcquireSemaphore(nn::os::SemaphoreType*) | 72 | 0 | 61 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::GetThreadId(nn::os::ThreadType const*) | 72 | 0 | 32 | — | — |
| nn::os::ReleaseSemaphore(nn::os::SemaphoreType*) | 65 | 0 | 48 | — | 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::SendLightMessageQueue(nn::os::LightMessageQueueType*, unsigned long) | 64 | 0 | 52 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x34, 0x35, 0x36 |
| nn::os::ConvertToTimeSpan(nn::os::Tick) | 63 | 0 | 49 | — | — |
| nn::os::InitializeMultiWaitHolder(nn::os::MultiWaitHolderType*, nn::os::EventType*) | 62 | 0 | 62 | — | — |
| nn::os::TimedWaitEvent(nn::os::EventType*, nn::TimeSpan) | 58 | 0 | 58 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::TryWaitEvent(nn::os::EventType*) | 58 | 0 | 58 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::FinalizeReaderWriterLock(nn::os::ReaderWriterLockType*) | 53 | 0 | 43 | — | — |
| nn::os::FinalizeSemaphore(nn::os::SemaphoreType*) | 51 | 0 | 50 | — | — |
| nn::os::InitializeReaderWriterLock(nn::os::ReaderWriterLockType*) | 42 | 0 | 29 | — | — |
| nn::os::IsMutexLockedByCurrentThread(nn::os::MutexType const*) | 37 | 0 | 36 | — | — |
| nn::os::ReleaseSemaphore(nn::os::SemaphoreType*, int) | 32 | 0 | 21 | — | 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::TryAcquireWriteLock(nn::os::ReaderWriterLockType*) | 32 | 0 | 12 | — | — |
| nn::os::SleepThread(nn::TimeSpan) | 29 | 0 | 26 | — | 0xb |
| nn::os::WaitConditionVariable(nn::os::ConditionVariableType*, nn::os::MutexType*) | 26 | 0 | 19 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::InitializeSemaphore(nn::os::SemaphoreType*, int, int) | 24 | 0 | 23 | — | — |
| nn::os::FinalizeConditionVariable(nn::os::ConditionVariableType*) | 22 | 0 | 19 | — | — |
| nn::os::TryAcquireSemaphore(nn::os::SemaphoreType*) | 21 | 0 | 18 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::TryWaitSystemEvent(nn::os::SystemEventType*) | 17 | 0 | 4 | — | 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::WaitThread(nn::os::ThreadType*) | 15 | 0 | 15 | — | 0x5, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::ChangeThreadPriority(nn::os::ThreadType*, int) | 14 | 0 | 7 | — | 0xd, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::InitializeConditionVariable(nn::os::ConditionVariableType*) | 13 | 0 | 10 | — | — |
| nn::os::FinalizeMultiWaitHolder(nn::os::MultiWaitHolderType*) | 12 | 0 | 6 | — | — |
| nn::os::DestroySystemEvent(nn::os::SystemEventType*) | 11 | 0 | 10 | — | 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::DestroyThread(nn::os::ThreadType*) | 9 | 0 | 9 | — | 0x5, 0x9, 0x16, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::GetTlsValue(nn::os::TlsSlot) | 9 | 0 | 8 | — | — |
| nn::os::SetThreadCoreMask(nn::os::ThreadType*, int, unsigned long) | 9 | 0 | 9 | — | 0xf, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::SetTlsValue(nn::os::TlsSlot, unsigned long) | 9 | 0 | 8 | — | — |
| nn::os::CreateThread(nn::os::ThreadType*, void (*)(void*), void*, void*, unsigned long, int) | 8 | 0 | 7 | — | 0x5, 0x8, 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x25, 0x26, 0x36 |
| nn::os::StartThread(nn::os::ThreadType*) | 8 | 0 | 8 | — | 0x9, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::SetThreadName(nn::os::ThreadType*, char const*) | 7 | 0 | 7 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::ClearEvent(nn::os::EventType*) | 6 | 0 | 6 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::GetCurrentCoreNumber() | 6 | 0 | 6 | — | 0x10 |
| nn::os::BroadcastConditionVariable(nn::os::ConditionVariableType*) | 5 | 0 | 5 | — | 0x1d |
| nn::os::FinalizeTimerEvent(nn::os::TimerEventType*) | 5 | 0 | 4 | — | — |
| nn::os::FreeTlsSlot(nn::os::TlsSlot) | 5 | 0 | 5 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::GetSystemTickFrequency() | 5 | 0 | 5 | — | — |
| nn::os::GetThreadAvailableCoreMask() | 5 | 0 | 5 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x29, 0x36 |
| nn::os::GetThreadPriority(nn::os::ThreadType const*) | 5 | 0 | 5 | — | — |
| nn::os::CreateThread(nn::os::ThreadType*, void (*)(void*), void*, void*, unsigned long, int, int) | 4 | 0 | 3 | — | 0x5, 0x8, 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x25, 0x26, 0x36 |
| nn::os::InitializeTimerEvent(nn::os::TimerEventType*, nn::os::EventClearMode) | 4 | 0 | 3 | — | — |
| nn::os::SwitchToFiber(nn::os::FiberType*) | 4 | 0 | 4 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::YieldThread() | 4 | 0 | 4 | — | 0xb |
| nn::os::AllocateTlsSlot(nn::os::TlsSlot*, void (*)(unsigned long)) | 3 | 0 | 3 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::GenerateRandomBytes(void*, unsigned long) | 3 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x29, 0x36 |
| nn::os::GetCurrentFiber() | 3 | 0 | 3 | — | — |
| nn::os::GetThreadNamePointer(nn::os::ThreadType const*) | 3 | 0 | 3 | — | — |
| nn::os::StartOneShotTimerEvent(nn::os::TimerEventType*, nn::TimeSpan) | 3 | 0 | 2 | — | 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::ClearSystemEvent(nn::os::SystemEventType*) | 2 | 0 | 1 | — | 0x12, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::FinalizeFiber(nn::os::FiberType*) | 2 | 0 | 2 | — | 0x5 |
| nn::os::FinalizeLightMessageQueue(nn::os::LightMessageQueueType*) | 2 | 0 | 2 | — | — |
| nn::os::InitializeLightMessageQueue(nn::os::LightMessageQueueType*, unsigned long*, unsigned long) | 2 | 0 | 2 | — | — |
| nn::os::JamLightMessageQueue(nn::os::LightMessageQueueType*, unsigned long) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x34, 0x35, 0x36 |
| nn::os::LinkMultiWaitHolder(nn::os::MultiWaitType*, nn::os::MultiWaitHolderType*) | 2 | 0 | 1 | — | — |
| nn::os::QueryMemoryInfo(nn::os::MemoryInfo*) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x29, 0x36 |
| nn::os::ReceiveLightMessageQueue(unsigned long*, nn::os::LightMessageQueueType*) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x34, 0x35, 0x36 |
| nn::os::SdkAllocateTlsSlot(nn::os::TlsSlot*, void (*)(unsigned long)) | 2 | 0 | 2 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::UnlinkMultiWaitHolder(nn::os::MultiWaitHolderType*) | 2 | 0 | 1 | — | — |
| nn::os::WaitSystemEvent(nn::os::SystemEventType*) | 2 | 0 | 2 | — | 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::FinalizeMessageQueue(nn::os::MessageQueueType*) | 1 | 0 | 1 | — | — |
| nn::os::FinalizeMultiWait(nn::os::MultiWaitType*) | 1 | 0 | 1 | — | — |
| nn::os::GetCurrentStackInfo(unsigned long*, unsigned long*) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::GetHostArgc() | 1 | 0 | 1 | — | — |
| nn::os::GetHostArgv() | 1 | 0 | 1 | — | — |
| nn::os::GetVirtualAddressMemoryResourceUsage() | 1 | 0 | 1 | — | 0x29 |
| nn::os::InitializeFiber(nn::os::FiberType*, nn::os::FiberType* (*)(void*), void*, void*, unsigned long, int) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::InitializeMessageQueue(nn::os::MessageQueueType*, unsigned long*, unsigned long) | 1 | 0 | 1 | — | — |
| nn::os::InitializeMultiWait(nn::os::MultiWaitType*) | 1 | 0 | 1 | — | — |
| nn::os::InitializeMultiWaitHolder(nn::os::MultiWaitHolderType*, nn::os::SystemEventType*) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::JamMessageQueue(nn::os::MessageQueueType*, unsigned long) | 1 | 0 | 1 | — | 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::ReceiveMessageQueue(unsigned long*, nn::os::MessageQueueType*) | 1 | 0 | 1 | — | 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::SendMessageQueue(nn::os::MessageQueueType*, unsigned long) | 1 | 0 | 1 | — | 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::SetDyingMessageRegion(unsigned long, unsigned long) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x29, 0x36 |
| nn::os::SetMemoryAllocatorForThreadLocal(void* (*)(unsigned long, unsigned long), void (*)(void*, unsigned long)) | 1 | 0 | 1 | — | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::SetThreadNamePointer(nn::os::ThreadType*, char const*) | 1 | 0 | 1 | — | — |
| nn::os::SignalTimerEvent(nn::os::TimerEventType*) | 1 | 0 | 1 | — | 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::StartPeriodicTimerEvent(nn::os::TimerEventType*, nn::TimeSpan, nn::TimeSpan) | 1 | 0 | 1 | — | 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::TryJamMessageQueue(nn::os::MessageQueueType*, unsigned long) | 1 | 0 | 1 | — | 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::TryReceiveMessageQueue(unsigned long*, nn::os::MessageQueueType*) | 1 | 0 | 1 | — | 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::TrySendMessageQueue(nn::os::MessageQueueType*, unsigned long) | 1 | 0 | 1 | — | 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::WaitAny(nn::os::MultiWaitType*) | 1 | 0 | 1 | — | 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |
| nn::os::WaitTimerEvent(nn::os::TimerEventType*) | 1 | 0 | 1 | — | 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 |

## Argumentos de montagem e caminho

Para Mount*/Commit*/CreateFile/DeleteFile, o argumento de string observado nas assinaturas é x0; para OpenFile/OpenDirectory/GetEntryType é x1. Só esse argumento é inspecionado, evitando tratar ponteiros de saída como caminhos.

A fatia reversa fica dentro da mesma função, nas 16 instruções anteriores à chamada BL ou tail B. Aceita ADR, ADRP+ADD, ADD imediato e cópias MOV de 64 bits; bloqueia sobrescritas, chamadas, transferências de controle e joins diretos conhecidos. Não segue LDR, argumentos de chamadores, strings formatadas, heap ou pilha; uma escrita W invalida um ponteiro X. Só exporta ASCII imprimível não vazio com NUL no `.rodata`, de até 4096 bytes.

Recuperadas: 3/38 chamadas selecionadas; sem string: 35. A ausência de literal local não indica ausência de arquivo ou montagem em runtime.

### Nomes de montagem distintos

Valores exatos recuperados em Mount* e Commit*; não normalizados nem inferidos de caminhos.

| Nome literal | Chamadas |
|---|---:|
| `linked-save` | 1 |
| `data` | 1 |

### Até 30 prefixos de caminho mais frequentes

Prefixo termina no segundo `/`, inclusive; quando há menos de dois, mantém o literal inteiro. Contagem por callsite recuperado, sem inventariar strings globais não passadas às APIs.

| Prefixo | Chamadas |
|---|---:|
| `_npln:/npln_config.json` | 1 |

### Callsites com string recuperada

| Chamada RVA | Importação (demangled) | Argumento | Literal |
|---|---|---|---|
| 0xabae14 | nn::fs::MountSaveDataReadOnly(char const*, nn::ApplicationId, nn::account::Uid const&) | x0 | `linked-save` |
| 0x193a6a4 | nn::fs::MountRom(char const*, void*, unsigned long) | x0 | `data` |
| 0x1e4c864 | nn::fs::OpenFile(nn::fs::FileHandle*, char const*, int) | x1 | `_npln:/npln_config.json` |

### Chamadas sem string por motivo

| Motivo | Chamadas |
|---|---:|
| barreira_controle | 13 |
| join_direto | 12 |
| sem_definicao_na_janela_16 | 7 |
| definicao_ldr_nao_suportada | 2 |
| definicao_csinc_nao_suportada | 1 |

## Sites adicionais fora dos FDEs

Conciliação explícita com `call_targets.csv`; o script não altera o censo anterior.

| Chamada RVA | Função RVA | Origem | Importação (demangled) |
|---|---|---|---|
| 0x1dbcdc8 | 0x1dbcd90 | lacuna | nn::fs::OpenFile(nn::fs::FileHandle*, char const*, int) |
| 0x1dbcdf8 | 0x1dbcd90 | lacuna | nn::fs::GetFileSize(long*, nn::fs::FileHandle) |
| 0x1dbce3c | 0x1dbcd90 | lacuna | nn::fs::ReadFile(unsigned long*, nn::fs::FileHandle, long, void*, unsigned long) |
| 0x1dbce50 | 0x1dbcd90 | lacuna | nn::fs::CloseFile(nn::fs::FileHandle) |
| 0x1dbcf34 | 0x1dbcf00 | lacuna | nn::fs::GetEntryType(nn::fs::DirectoryEntryType*, char const*) |
| 0x1dbcf80 | 0x1dbcf00 | lacuna | nn::fs::OpenDirectory(nn::fs::DirectoryHandle*, char const*, int) |
| 0x1dbcf94 | 0x1dbcf00 | lacuna | nn::fs::GetDirectoryEntryCount(long*, nn::fs::DirectoryHandle) |
| 0x1dbcfa4 | 0x1dbcf00 | lacuna | nn::fs::CloseDirectory(nn::fs::DirectoryHandle) |
| 0x1dbcfe4 | 0x1dbcf00 | lacuna | nn::fs::ReadDirectory(long*, nn::fs::DirectoryEntry*, nn::fs::DirectoryHandle, long) |
| 0x1dbd418 | 0x1dbd3f0 | lacuna | nn::nifm::GetCurrentProxySetting(nn::nifm::ProxySetting*) |

## Limites e cobertura de alcance

Importações selecionadas sem alcance no censo: 0. Nomes heurísticos distintos que não pertencem ao catálogo local: inf, nan, ntc.

As chamadas diretas não cobrem usos por GOT, vtables ou dispatchers resolvidos em runtime. Recomendações api exigem ABI, valores de retorno, handles e vida útil corretos; as contagens não justificam stubs de sucesso vazio.

## Saídas e autoverificações

- `r5_imports.csv`: superfície por API, contagens FDE/imagem, chamadores, alcance e status.
- `r5_callers.csv`: cada callsite e a função que o contém.
- `r5_fs_strings.csv`: exatamente chamada, importação, registrador e string.
- `r5_fs_string_audit.csv`: todas as tentativas, fontes do endereço e motivos de não recuperação.
- `r5_fs_prefixes.csv`, `r5_area_recommendations.csv`: agregações e critérios de planejamento.

1. Importações existem no inventário e têm PLT ou marcador sem_stub.
2. Contagens por API e área conferem com call_targets; a varredura independente dos FDEs também confere.
3. Todas as strings recuperadas foram verificadas por bytes, NUL e limites do rodata. Testes sintéticos cobrem ADRP+ADD com cópia para outro registrador, destruição do ponteiro por escrita W e joins diretos.

## Comandos de aceite

```text
build\venv\Scripts\python.exe -I analysis\recomp\r5_api_surface.py
Capstone 5.0.7; areas=13; importacoes de funcao selecionadas=310
Importacoes: 310; sem stub: 0; sem alcance: 0
Chamadas do censo: 31220; sites na imagem: 31230; adicionais fora dos FDEs: 10
FS selecionadas: 38 chamadas; strings recuperadas: 3; sem string: 35
Montagens literais distintas: 2; prefixos de caminho distintos: 1
AUTOVERIFICACAO 1: OK
AUTOVERIFICACAO 2: OK
AUTOVERIFICACAO 3: OK
Saidas: analysis/recomp/generated/r5_*.csv e analysis/recomp/r5_api_surface_report.md
Exit code: 0
```

```text
python tools\check_repo_safety.py
OK: 261 arquivos verificados

Exit code: 0
```

A verificação acima é invocada pelo script com o mesmo Python em modo isolado; o comando independente também deve ser executado no aceite. Sem commit ou push.
