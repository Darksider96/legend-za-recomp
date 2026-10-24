# Ficha de ABI das APIs da R5

Classificação mecânica de argumentos, declarações públicas e limites explícitos. Não é uma ficha integralmente confirmada: retornos ausentes/conflitantes e tipos sem tamanho continuam bloqueados. Nenhum arquivo de tarefa anterior foi alterado.

## Totais

- Funcoes: 208; linhas ABI: 524; tipos documentados: 64
- Argumentos por classe: agregado=16; enum=13; flutuante=2; inteiro=51; ponteiro=121; precisa_prototipo=8; precisa_tamanho=4; referencia=75; this=26
- Retornos indiretos x8 confirmados por tipo: 0; candidatos x8 na entrada SDK: 0
- Funcoes com precisa_tamanho: 107; this nao confirmado: 8
- Retornos sem declaracao: 104; conflitos publicos: 2

## Fontes e regras

- [AAPCS64 2025Q1](https://github.com/ARM-software/abi-aa/blob/2025Q1/aapcs64/aapcs64.rst) e [C++ ABI 2025Q1](https://github.com/ARM-software/abi-aa/blob/2025Q1/cppabi64/cppabi64.rst): bancos independentes x0–x7/v0–v7, pilha, alinhamento e x8.
- Cabeçalhos comunitários [nnheaders](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/README.md), revisão `246c3b482d045934ed004f50c5ada37f3b10b703`. Bindings cleanroom [nnsdk-rs](https://github.com/ultimate-research/nnsdk-rs/blob/d23f131663ec72a87c84914e6e738999f3ab403d/Cargo.toml), revisão `d23f131663ec72a87c84914e6e738999f3ab403d`. São reconstruções públicas de outras versões, não declarações originais deste SDK.
- As declarações Rust são ligadas pelo símbolo mangled exato; a alternativa C++ exige escopo, tipos de argumentos normalizados e distinção de membro/estático. Declarações discordantes não são resolvidas por preferência silenciosa. Hashes dos arquivos públicos estão em r5_abi_sources.csv; nenhuma fonte é buscada pela branch móvel no script.
- A assinatura demangled e o mangled de funções ordinárias não fornecem o retorno. Sem declaração compatível, `nao_declarado` é marcador de metadados, não um tipo C++ inventado. Conversões codificam seu tipo alvo; construtores/destrutores seguem a regra C++ de retorno void.
- Referências, inclusive &&, são ponteiros. Callback é ponteiro. this ocupa x0 em membros não estáticos; um escopo de classe sozinho não prova que o método seja de instância. Quando faltam fontes, `this?` e os argumentos ficam indeterminados em vez de deslocar os registradores por suposição.
- Inteiros/enums usam os bits baixos do próximo x; a classe e o tipo indicam a largura. Retornos de até 32 bits usam w0. Estruturas pequenas usam um/dois x; objetos não triviais para chamadas usam cópia indireta. Alinhamento 16 ajusta o próximo x para índice par. Excedentes ficam na pilha, relativa ao SP de entrada.
- HFA usa até quatro v, inclusive no retorno: uma HFA de quatro doubles continua em v0–v3 mesmo com 32 bytes. Outros agregados grandes/não triviais retornam por x8, sem deslocar os argumentos de x0. Tipos desconhecidos bloqueiam a posição dos argumentos seguintes porque podem consumir x, v ou uma cópia indireta.
- Sem candidato/declaração de x8 não significa que todas as APIs desconhecidas estejam provadas como retorno direto. A observação binária de x8 só percorre o bloco inicial do SDK, até a primeira transferência de controle.

## Uma linha por função

Índices de argumentos explícitos começam em 0; this é separado. `?` indica posição indeterminada. A tabela CSV preserva os tipos e classes completos.

### nn::fs

| Função | Argumentos → locais | Retorno |
|---|---|---|
| nn::fs::Unmount(char const*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::fs::CloseFile(nn::fs::FileHandle) | 0:x0 | void → sem_resultado (void) |
| nn::fs::FlushFile(nn::fs::FileHandle) | 0:x0 | nn::Result → w0 (resultado) |
| nn::fs::OpenFile(nn::fs::FileHandle*, char const*, int) | 0:x0; 1:x1; 2:x2 | nn::Result → w0 (resultado) |
| nn::fs::GetFileSize(long*, nn::fs::FileHandle) | 0:x0; 1:x1 | nn::Result → w0 (resultado) |
| nn::fs::GetEntryType(nn::fs::DirectoryEntryType*, char const*) | 0:x0; 1:x1 | nn::Result → w0 (resultado) |
| nn::fs::WriteFile(nn::fs::FileHandle, long, void const*, unsigned long, nn::fs::WriteOption const&) | 0:x0; 1:x1; 2:x2; 3:x3; 4:x4 | nn::Result → w0 (resultado) |
| nn::fs::SetFileSize(nn::fs::FileHandle, long) | 0:x0; 1:x1 | nn::Result → w0 (resultado) |
| nn::fs::CreateFile(char const*, long) | 0:x0; 1:x1 | nn::Result → w0 (resultado) |
| nn::fs::ReadFile(unsigned long*, nn::fs::FileHandle, long, void*, unsigned long) | 0:x0; 1:x1; 2:x2; 3:x3; 4:x4 | nn::Result → w0 (resultado) |
| nn::fs::ReadFile(nn::fs::FileHandle, long, void*, unsigned long) | 0:x0; 1:x1; 2:x2; 3:x3 | nn::Result → w0 (resultado) |
| nn::fs::MountSaveData(char const*, nn::account::Uid const&) | 0:x0; 1:x1 | nn::Result → w0 (resultado) |
| nn::fs::CreateDirectory(char const*) | 0:x0 | nn::Result → w0 (resultado) |
| nn::fs::DeleteFile(char const*) | 0:x0 | nn::Result → w0 (resultado) |
| nn::fs::SetCurrentThreadFsContext(nn::fs::FsContext const*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::fs::CloseDirectory(nn::fs::DirectoryHandle) | 0:x0 | void → sem_resultado (void) |
| nn::fs::Commit(char const*) | 0:x0 | nn::Result → w0 (resultado) |
| nn::fs::DeleteDirectoryRecursively(char const*) | 0:x0 | nn::Result → w0 (resultado) |
| nn::fs::EnsureSaveData(nn::account::Uid const&) | 0:x0 | nn::Result → w0 (resultado) |
| nn::fs::GetCurrentThreadFsContext() | sem argumentos | nao_declarado → indeterminado (precisa_tamanho) |
| nn::fs::IsSaveDataExisting(nn::ApplicationId, nn::account::Uid const&) | 0:x0; 1:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::fs::IsSaveDataExisting(nn::account::Uid const&) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::fs::MountAddOnContent(char const*, int, void*, unsigned long) | 0:x0; 1:x1; 2:x2; 3:x3 | nn::Result → w0 (resultado) |
| nn::fs::MountRom(char const*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::fs::MountRom(char const*, void*, unsigned long) | 0:x0; 1:x1; 2:x2 | nn::Result → w0 (resultado) |
| nn::fs::MountSaveDataReadOnly(char const*, nn::ApplicationId, nn::account::Uid const&) | 0:x0; 1:x1; 2:x2 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::fs::OpenDirectory(nn::fs::DirectoryHandle*, char const*, int) | 0:x0; 1:x1; 2:x2 | nn::Result → w0 (resultado) |
| nn::fs::QueryMountAddOnContentCacheSize(unsigned long*, int) | 0:x0; 1:x1 | nn::Result → w0 (resultado) |
| nn::fs::QueryMountRomCacheSize(unsigned long*) | 0:x0 | nn::Result → w0 (resultado) |
| nn::fs::ReadDirectory(long*, nn::fs::DirectoryEntry*, nn::fs::DirectoryHandle, long) | 0:x0; 1:x1; 2:x2; 3:x3 | nn::Result → w0 (resultado) |
| nn::fs::SetAllocator(void* (*)(unsigned long), void (*)(void*, unsigned long)) | 0:x0; 1:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::fs::GetDirectoryEntryCount(long*, nn::fs::DirectoryHandle) | 0:x0; 1:x1 | nn::Result → w0 (resultado) |

### nn::hid

| Função | Argumentos → locais | Retorno |
|---|---|---|
| nn::hid::GetNpadControllerColor(nn::hid::NpadControllerColor*, nn::hid::NpadControllerColor*, unsigned int const&) | 0:x0; 1:x1; 2:x2 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::GetNpadStyleSet(unsigned int const&) | 0:x0 | nn::util::BitFlagSet<32, nn::hid::NpadStyleTag> → x0 (agregado) |
| nn::hid::GetNpadControllerColor(nn::hid::NpadControllerColor*, unsigned int const&) | 0:x0; 1:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::StopSixAxisSensor(nn::hid::SixAxisSensorHandle const&) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::StartSixAxisSensor(nn::hid::SixAxisSensorHandle const&) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::BindNpadStyleSetUpdateEvent(unsigned int const&, nn::os::SystemEventType*, nn::os::EventClearMode) | 0:x0; 1:x1; 2:x2 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::GetSixAxisSensorState(nn::hid::SixAxisSensorState*, nn::hid::SixAxisSensorHandle const&) | 0:x0; 1:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::InitializeNpad() | sem argumentos | void → sem_resultado (void) |
| nn::hid::SetSupportedNpadIdType(unsigned int const*, unsigned long) | 0:x0; 1:x1 | void → sem_resultado (void) |
| nn::hid::SetSupportedNpadStyleSet(nn::util::BitFlagSet<32, nn::hid::NpadStyleTag>) | 0:x0 | void → sem_resultado (void) |
| nn::hid::EnableSixAxisSensorFusion(nn::hid::SixAxisSensorHandle const&, bool) | 0:x0; 1:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::GetGyroscopeZeroDriftMode(nn::hid::SixAxisSensorHandle const&) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::GetNpadJoyAssignment(unsigned int const&) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::GetNpadState(nn::hid::NpadFullKeyState*, unsigned int const&) | 0:x0; 1:x1 | void → sem_resultado (void) |
| nn::hid::GetNpadState(nn::hid::NpadHandheldState*, unsigned int const&) | 0:x0; 1:x1 | void → sem_resultado (void) |
| nn::hid::GetNpadState(nn::hid::NpadJoyDualState*, unsigned int const&) | 0:x0; 1:x1 | void → sem_resultado (void) |
| nn::hid::GetNpadState(nn::hid::NpadJoyLeftState*, unsigned int const&) | 0:x0; 1:x1 | void → sem_resultado (void) |
| nn::hid::GetNpadState(nn::hid::NpadJoyRightState*, unsigned int const&) | 0:x0; 1:x1 | void → sem_resultado (void) |
| nn::hid::GetSixAxisSensorFusionParameters(float*, float*, nn::hid::SixAxisSensorHandle const&) | 0:x0; 1:x1; 2:x2 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::GetSixAxisSensorStates(nn::hid::SixAxisSensorState*, int, nn::hid::SixAxisSensorHandle const&) | 0:x0; 1:x1; 2:x2 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::IsSixAxisSensorAtRest(nn::hid::SixAxisSensorHandle const&) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::IsSixAxisSensorFusionEnabled(nn::hid::SixAxisSensorHandle const&) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::SendVibrationValue(nn::hid::VibrationDeviceHandle const&, nn::hid::VibrationValue const&) | 0:x0; 1:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::SetGyroscopeZeroDriftMode(nn::hid::SixAxisSensorHandle const&, nn::hid::GyroscopeZeroDriftMode const&) | 0:x0; 1:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::SetNpadHandheldActivationMode(nn::hid::NpadHandheldActivationMode) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::SetNpadJoyHoldType(nn::hid::NpadJoyHoldType) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::SetSixAxisSensorFusionParameters(nn::hid::SixAxisSensorHandle const&, float, float) | 0:x0; 1:v0; 2:v1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::DisconnectNpad(unsigned int const&) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::GetNpadCommunicationMode() | sem argumentos | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::GetNpadHandheldActivationMode() | sem argumentos | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::GetNpadJoyHoldType() | sem argumentos | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::GetNpadStates(nn::hid::NpadFullKeyState*, int, unsigned int const&) | 0:x0; 1:x1; 2:x2 | int → w0 (inteiro) |
| nn::hid::GetNpadStates(nn::hid::NpadHandheldState*, int, unsigned int const&) | 0:x0; 1:x1; 2:x2 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::GetNpadStates(nn::hid::NpadJoyDualState*, int, unsigned int const&) | 0:x0; 1:x1; 2:x2 | int → w0 (inteiro) |
| nn::hid::GetNpadStates(nn::hid::NpadJoyLeftState*, int, unsigned int const&) | 0:x0; 1:x1; 2:x2 | int → w0 (inteiro) |
| nn::hid::GetNpadStates(nn::hid::NpadJoyRightState*, int, unsigned int const&) | 0:x0; 1:x1; 2:x2 | int → w0 (inteiro) |
| nn::hid::GetPlayerLedPattern(unsigned int const&) | 0:x0 | void → sem_resultado (void) |
| nn::hid::GetSixAxisSensorHandles(nn::hid::SixAxisSensorHandle*, int, unsigned int const&, nn::util::BitFlagSet<32, nn::hid::NpadStyleTag>) | 0:x0; 1:x1; 2:x2; 3:x3 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::GetSupportedNpadStyleSet() | sem argumentos | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::GetVibrationDeviceHandles(nn::hid::VibrationDeviceHandle*, int, unsigned int const&, nn::util::BitFlagSet<32, nn::hid::NpadStyleTag>) | 0:x0; 1:x1; 2:x2; 3:x3 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::InitializeTouchScreen(int, int) | 0:x0; 1:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::InitializeVibrationDevice(nn::hid::VibrationDeviceHandle const&) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::MergeSingleJoyAsDualJoy(unsigned int const&, unsigned int const&) | 0:x0; 1:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::SetNpadCommunicationMode(nn::hid::NpadCommunicationMode) | 0:indeterminado | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::SetNpadJoyAssignmentModeDual(unsigned int const&) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::SetNpadJoyAssignmentModeSingle(unsigned int const&) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::SetNpadJoyAssignmentModeSingle(unsigned int const&, nn::hid::NpadJoyDeviceType) | 0:x0; 1:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::SetNpadJoyAssignmentModeSingle(unsigned int*, unsigned int const&, nn::hid::NpadJoyDeviceType) | 0:x0; 1:x1; 2:x2 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::ShowControllerStrapGuide() | sem argumentos | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::SixAxisSensorState::GetQuaternion(nn::util::neon::Vector4fType*) const | this:x0; 0:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::StartLrAssignmentMode() | sem argumentos | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::StopLrAssignmentMode() | sem argumentos | nao_declarado → indeterminado (precisa_tamanho) |
| nn::hid::SwapNpadAssignment(unsigned int const&, unsigned int const&) | 0:x0; 1:x1 | nao_declarado → indeterminado (precisa_tamanho) |

### nn::oe + nn::applet

| Função | Argumentos → locais | Retorno |
|---|---|---|
| nn::oe::SetUserInactivityDetectionTimeExtended(bool) | 0:x0 | void → sem_resultado (void) |
| nn::oe::SetCpuBoostMode(nn::oe::CpuBoostMode) | 0:x0 | void → sem_resultado (void) |
| nn::oe::SetPerformanceConfiguration(nn::oe::PerformanceMode, int) | 0:x0; 1:x1 | void → sem_resultado (void) |
| nn::oe::GetDesiredLanguage() | sem argumentos | nn::settings::LanguageCode → x0 (agregado) |
| nn::oe::LeaveExitRequestHandlingSection() | sem argumentos | nao_declarado → indeterminado (precisa_tamanho) |
| nn::oe::SetFocusHandlingMode(nn::oe::FocusHandlingMode) | 0:x0 | void → sem_resultado (void) |
| nn::oe::DisableRecording() | sem argumentos | nao_declarado → indeterminado (precisa_tamanho) |
| nn::oe::DisableScreenShot() | sem argumentos | nao_declarado → indeterminado (precisa_tamanho) |
| nn::oe::EnableRecording() | sem argumentos | nao_declarado → indeterminado (precisa_tamanho) |
| nn::oe::EnableScreenShot() | sem argumentos | nao_declarado → indeterminado (precisa_tamanho) |
| nn::oe::EnterExitRequestHandlingSection() | sem argumentos | nao_declarado → indeterminado (precisa_tamanho) |
| nn::oe::FinishStartupLogo() | sem argumentos | void → sem_resultado (void) |
| nn::oe::GetCurrentFocusState() | sem argumentos | nn::oe::FocusState → w0 (enum) |
| nn::oe::GetPerformanceMode() | sem argumentos | nn::oe::PerformanceMode → w0 (enum) |
| nn::oe::GetPseudoDeviceId(nn::util::Uuid*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::oe::Initialize() | sem argumentos | void → sem_resultado (void) |
| nn::oe::IsUserInactivityDetectionTimeExtended() | sem argumentos | bool → w0 (inteiro) |
| nn::oe::SetPerformanceModeChangedNotificationEnabled(bool) | 0:x0 | void → sem_resultado (void) |
| nn::oe::TryPopNotificationMessage(unsigned int*) | 0:x0 | bool → w0 (inteiro) |

### nn::account

| Função | Argumentos → locais | Retorno |
|---|---|---|
| nn::account::AsyncContext::GetResult() | this:x0 | nn::Result → w0 (resultado) |
| nn::account::AsyncContext::~AsyncContext() | this:x0 | void → sem_resultado (void) |
| nn::account::AsyncContext::HasDone(bool*) | this:x0; 0:x1 | nn::Result → w0 (resultado) |
| nn::account::AsyncContext::AsyncContext() | this:x0 | void → sem_resultado (void) |
| nn::account::AsyncContext::~AsyncContext() | this:x0 | void → sem_resultado (void) |
| nn::account::GetNetworkServiceAccountId(nn::account::NetworkServiceAccountId*, nn::account::UserHandle const&) | 0:x0; 1:x1 | nn::Result → w0 (resultado) |
| nn::account::GetUserId(nn::account::Uid*, nn::account::UserHandle const&) | 0:x0; 1:x1 | nn::Result → w0 (resultado) |
| nn::account::EnsureNetworkServiceAccountIdTokenCacheAsync(nn::account::AsyncContext*, nn::account::UserHandle const&) | 0:x0; 1:x1 | nn::Result → w0 (resultado) |
| nn::account::Initialize() | sem argumentos | void → sem_resultado (void) |
| nn::account::LoadNetworkServiceAccountIdTokenCache(unsigned long*, char*, unsigned long, nn::account::UserHandle const&) | 0:x0; 1:x1; 2:x2; 3:x3 | nn::Result → w0 (resultado) |
| nn::account::AsyncContext::GetSystemEvent(nn::os::SystemEvent*) | this:x0; 0:x1 | nn::Result → w0 (resultado) |
| nn::account::AsyncContext::operator=(nn::account::AsyncContext&&) | this:x0; 0:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::account::EnsureNetworkServiceAccountAvailable(nn::account::UserHandle const&) | 0:x0 | nn::Result → w0 (resultado) |
| nn::account::AsyncContext::Cancel() | this:x0 | nn::Result → w0 (resultado) |
| nn::account::AsyncNetworkServiceLicenseInfoContext::AsyncNetworkServiceLicenseInfoContext() | this:x0 | void → sem_resultado (void) |
| nn::account::AsyncNetworkServiceLicenseInfoContext::AsyncNetworkServiceLicenseInfoContext(nn::account::AsyncNetworkServiceLicenseInfoContext&&) | this:x0; 0:x1 | void → sem_resultado (void) |
| nn::account::AsyncNetworkServiceLicenseInfoContext::GetNetworkServiceLicenseKind() | this?:indeterminado | nao_declarado → indeterminado (precisa_tamanho) |
| nn::account::AsyncNetworkServiceLicenseInfoContext::operator=(nn::account::AsyncNetworkServiceLicenseInfoContext&&) | this:x0; 0:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::account::CloseUser(nn::account::UserHandle const&) | 0:x0 | void → sem_resultado (void) |
| nn::account::GetNickname(nn::account::Nickname*, nn::account::Uid const&) | 0:x0; 1:x1 | nn::Result → w0 (resultado) |
| nn::account::GetUserExistence(bool*, nn::account::Uid const&) | 0:x0; 1:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::account::IsNetworkServiceAccountAvailable(bool*, nn::account::UserHandle const&) | 0:x0; 1:x1 | nn::Result → w0 (resultado) |
| nn::account::LoadNetworkServiceLicenseInfoAsync(nn::account::AsyncNetworkServiceLicenseInfoContext*, nn::account::UserHandle const&) | 0:x0; 1:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::account::ShowLicenseRequirementsForNetworkService(nn::account::UserHandle const&) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::account::TryOpenPreselectedUser(nn::account::UserHandle*) | 0:x0 | bool → w0 (inteiro) |

### nn::time

| Função | Argumentos → locais | Retorno |
|---|---|---|
| nn::time::ToCalendarTime(nn::time::CalendarTime*, nn::time::CalendarAdditionalInfo*, nn::time::PosixTime const&) | 0:x0; 1:x1; 2:x2 | nn::Result → w0 (resultado) |
| nn::time::Finalize() | sem argumentos | nn::Result → w0 (resultado) |
| nn::time::Initialize() | sem argumentos | nn::Result → w0 (resultado) |
| nn::time::IsInitialized() | sem argumentos | bool → w0 (inteiro) |
| nn::time::StandardUserSystemClock::GetCurrentTime(nn::time::PosixTime*) | 0:x0 | nn::Result → w0 (resultado) |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::~AsyncEnsureNetworkClockAvailabilityTask() | this:x0 | void → sem_resultado (void) |
| nn::time::StandardNetworkSystemClock::GetCurrentTime(nn::time::PosixTime*) | this?:indeterminado; 0:indeterminado_apos_tipo_desconhecido | nao_declarado → indeterminado (precisa_tamanho) |
| nn::time::GetDeviceLocationName(nn::time::LocationName*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::time::LoadTimeZoneRule(nn::time::TimeZoneRule*, nn::time::LocationName const&) | 0:x0; 1:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::time::ToCalendarTimeInUtc(nn::time::PosixTime const&) | 0:x0 | nn::time::CalendarTime → x0 (agregado) |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::AsyncEnsureNetworkClockAvailabilityTask() | this:x0 | void → sem_resultado (void) |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::GetFinishEvent() | this?:indeterminado | nao_declarado → indeterminado (precisa_tamanho) |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::GetResult() | this?:indeterminado | nao_declarado → indeterminado (precisa_tamanho) |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::Initialize(nn::os::EventClearMode) | this?:indeterminado; 0:indeterminado_apos_tipo_desconhecido | nao_declarado → indeterminado (precisa_tamanho) |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::StartTask() | this?:indeterminado | nao_declarado → indeterminado (precisa_tamanho) |
| nn::time::IsLeapYear(int) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::time::IsValidDate(int, int, int) | 0:x0; 1:x1; 2:x2 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::time::ToPosixTime(int*, nn::time::PosixTime*, int, nn::time::CalendarTime const&, nn::time::TimeZoneRule const&) | 0:x0; 1:x1; 2:x2; 3:x3; 4:x4 | nao_declarado → indeterminado (precisa_tamanho) |

### nn::settings

| Função | Argumentos → locais | Retorno |
|---|---|---|
| nn::settings::GetFullKeyGenericPadMap(nn::settings::FullKeyGenericPadMap*, nn::settings::FullKeyIndex) | 0:x0; 1:indeterminado | nao_declarado → indeterminado (precisa_tamanho) |
| nn::settings::SetFullKeyGenericPadMap(nn::settings::FullKeyGenericPadMap const&, nn::settings::FullKeyIndex) | 0:x0; 1:indeterminado | nao_declarado → indeterminado (precisa_tamanho) |

### nn::err

| Função | Argumentos → locais | Retorno |
|---|---|---|
| nn::err::ErrorResultVariant::ErrorResultVariant(nn::Result const&) | this:x0; 0:x1 | void → sem_resultado (void) |
| nn::err::ErrorResultVariant::operator nn::err::ErrorCode() const | this:x0 | nn::err::ErrorCode → indeterminado (precisa_tamanho) |
| nn::err::ErrorCode::IsValid() const | this:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::err::ErrorResultVariant::ErrorResultVariant() | this:x0 | void → sem_resultado (void) |
| nn::err::ErrorResultVariant::GetState() const | this:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::err::ErrorResultVariant::operator=(nn::err::ErrorCode const&) | this:x0; 0:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::err::ErrorResultVariant::operator nn::Result() const | this:x0 | nn::Result → w0 (resultado) |
| nn::err::ErrorResultVariant::operator=(nn::Result const&) | this:x0; 0:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::err::ApplicationErrorArg::ApplicationErrorArg() | this:x0 | void → sem_resultado (void) |
| nn::err::ApplicationErrorArg::SetApplicationErrorCodeNumber(unsigned int) | this:x0; 0:x1 | void → sem_resultado (void) |
| nn::err::ApplicationErrorArg::SetDialogMessage(char const*) | this:x0; 0:x1 | void → sem_resultado (void) |
| nn::err::ApplicationErrorArg::SetFullScreenMessage(char const*) | this:x0; 0:x1 | void → sem_resultado (void) |
| nn::err::ApplicationErrorArg::SetLanguageCode(nn::settings::LanguageCode const&) | this?:indeterminado; 0:indeterminado_apos_tipo_desconhecido | nao_declarado → indeterminado (precisa_tamanho) |
| nn::err::NplnContext::Make(void const*, unsigned long) | this?:indeterminado; 0:indeterminado_apos_tipo_desconhecido; 1:indeterminado_apos_tipo_desconhecido | nao_declarado → indeterminado (precisa_tamanho) |
| nn::err::ShowApplicationError(nn::err::ApplicationErrorArg const&) | 0:x0 | void → sem_resultado (void) |
| nn::err::ShowError(nn::Result) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::err::ShowError(nn::err::ErrorCode) | 0:indeterminado | nao_declarado → indeterminado (precisa_tamanho) |
| nn::err::ShowError(nn::err::ErrorResultVariant const&) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |

### nn::nifm

| Função | Argumentos → locais | Retorno |
|---|---|---|
| nn::nifm::IsNetworkAvailable() | sem argumentos | bool → w0 (inteiro) |
| nn::nifm::GetCurrentIpConfigInfo(in_addr*, in_addr*, in_addr*, in_addr*, in_addr*) | 0:x0; 1:x1; 2:x2; 3:x3; 4:x4 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::nifm::Initialize() | sem argumentos | nn::Result → w0 (resultado) |
| nn::nifm::SubmitNetworkRequestAndWait() | sem argumentos | void → sem_resultado (void) |
| nn::nifm::GetCurrentProxySetting(nn::nifm::ProxySetting*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::nifm::HandleNetworkRequestErrorResult() | sem argumentos | nao_declarado → indeterminado (precisa_tamanho) |
| nn::nifm::SetLocalNetworkMode(bool) | 0:x0 | void → sem_resultado (void) |

### nn::aoc

| Função | Argumentos → locais | Retorno |
|---|---|---|
| nn::aoc::ListAddOnContent(int*, int, int) | 0:x0; 1:x1; 2:x2 | int → w0 (inteiro) |
| nn::aoc::CountAddOnContent() | sem argumentos | nao_declarado → indeterminado (precisa_tamanho) |
| nn::aoc::GetAddOnContentListChangedEvent(nn::os::SystemEvent*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |

### nn::pl

| Função | Argumentos → locais | Retorno |
|---|---|---|
| nn::pl::RequestSharedFontLoad(nn::pl::SharedFontType) | 0:x0 | void → sem_resultado (void) |
| nn::pl::GetSharedFontAddress(nn::pl::SharedFontType) | 0:x0 | void* → x0 (ponteiro) |
| nn::pl::GetSharedFontLoadState(nn::pl::SharedFontType) | 0:x0 | nn::pl::LoadState → w0 (enum) |
| nn::pl::GetSharedFontSize(nn::pl::SharedFontType) | 0:x0 | unsigned int → w0 (inteiro) |

### nn::vi

| Função | Argumentos → locais | Retorno |
|---|---|---|
| nn::vi::CloseDisplay(nn::vi::Display*) | 0:x0 | nn::Result → w0 (resultado) |
| nn::vi::CreateLayer(nn::vi::Layer**, nn::vi::Display*) | 0:x0; 1:x1 | nn::Result → w0 (resultado) |
| nn::vi::DestroyLayer(nn::vi::Layer*) | 0:x0 | void → sem_resultado (void) |
| nn::vi::Finalize() | sem argumentos | void → sem_resultado (void) |
| nn::vi::GetNativeWindow(void**, nn::vi::Layer*) | 0:x0; 1:x1 | nn::Result → w0 (resultado) |
| nn::vi::Initialize() | sem argumentos | void → sem_resultado (void) |
| nn::vi::OpenDefaultDisplay(nn::vi::Display**) | 0:x0 | nn::Result → w0 (resultado) |

### nn::audio

| Função | Argumentos → locais | Retorno |
|---|---|---|
| nn::audio::GetAudioOutBufferDataPointer(nn::audio::AudioOutBuffer const*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::GetActiveAudioDeviceChannelCountForOutput() | sem argumentos | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::AcquireAudioDeviceNotificationForOutput(nn::os::SystemEvent*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::AppendAudioOutBuffer(nn::audio::AudioOut*, nn::audio::AudioOutBuffer*) | 0:x0; 1:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::GetActiveAudioDeviceName(nn::audio::AudioDeviceName*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::GetActiveAudioOutputDeviceName(nn::audio::AudioDeviceName*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::SetAudioOutBufferInfo(nn::audio::AudioOutBuffer*, void*, unsigned long, unsigned long) | 0:x0; 1:x1; 2:x2; 3:x3 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::CloseAudioOut(nn::audio::AudioOut*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::GetAudioOutBufferCount(nn::audio::AudioOut const*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::GetAudioOutChannelCount(nn::audio::AudioOut const*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::GetAudioOutState(nn::audio::AudioOut const*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::GetReleasedAudioOutBuffer(nn::audio::AudioOut*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::GetRequiredBufferSizeForResampler(int) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::GetResamplerOutputSampleCount(nn::audio::ResamplerType const*, int) | 0:x0; 1:x1 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::InitializeAudioOutParameter(nn::audio::AudioOutParameter*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::InitializeResampler(nn::audio::ResamplerType*, void*, unsigned long, int, int, int) | 0:x0; 1:x1; 2:x2; 3:x3; 4:x4; 5:x5 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::OpenDefaultAudioOut(nn::audio::AudioOut*, nn::os::SystemEvent*, nn::audio::AudioOutParameter const&) | 0:x0; 1:x1; 2:x2 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::ProcessResamplerBuffer(nn::audio::ResamplerType*, int*, short*, unsigned long, short const*, int) | 0:x0; 1:x1; 2:x2; 3:x3; 4:x4; 5:x5 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::StartAudioOut(nn::audio::AudioOut*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |
| nn::audio::StopAudioOut(nn::audio::AudioOut*) | 0:x0 | nao_declarado → indeterminado (precisa_tamanho) |

## Tipos e status

Tamanhos/alinhamentos são do modelo público AArch64 LP64. Tipos de buffers e referências encontrados nos argumentos também aparecem como apoio, mas seu tamanho não é necessário para colocar o ponteiro em registrador. Nomes apenas opacos não recebem tamanho presumido. Campos são descrições de armazenamento, não cópia de fonte SDK.

`transporte_corrobora_sem_prova_sizeof` significa que o binário copia/transporta a largura pública, sem provar sozinho a extensão completa ou o alinhamento do objeto. Não há promoção automática para sizeof confirmado.

| Tipo | Tamanho | Alinhamento | Campos / status | Fonte pública | Evidência main RVA |
|---|---:|---:|---|---|---|
| nao_declarado | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | [fonte](https://github.com/ARM-software/abi-aa/blob/2025Q1/cppabi64/cppabi64.rst) | — |
| nn::ApplicationId | 8 | 8 | 0x00: identificador u64 | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/nn.h) | — |
| nn::Result | 4 | 4 | 0x00: resultado de 32 bits; classe C++ completa nao reconstruida | [fonte](https://github.com/ultimate-research/nnsdk-rs/blob/d23f131663ec72a87c84914e6e738999f3ab403d/src/common_types.rs) | — |
| nn::account::AsyncContext | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::account::AsyncNetworkServiceLicenseInfoContext | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::account::NetworkServiceAccountId | 8 | 8 | 0x00: identificador u64 | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/account.h) | — |
| nn::account::Nickname | 33 | 1 | 0x00: char[0x21] | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/account.h) | — |
| nn::account::Uid | 16 | 8 | 0x00: u64[2] | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/account.h) | — |
| nn::account::UserHandle | 24 | 8 | 0x00: Uid (16); 0x10: ponteiro (8) | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/account.h) | — |
| nn::audio::AudioDeviceName | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::audio::AudioOut | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::audio::AudioOutBuffer | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::audio::AudioOutParameter | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::audio::ResamplerType | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::err::ApplicationErrorArg | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::err::ErrorCode | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::err::ErrorResultVariant | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::err::NplnContext | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::fs::DirectoryEntry | 784 | 8 | 0x000: char[0x301]; 0x301: padding[3]; 0x304: tipo u32; 0x308: tamanho s64 | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/fs/fs_types.h) | — |
| nn::fs::DirectoryEntryType | 4 | 4 | enum; transporte int; valores na fonte | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/fs/fs_types.h) | — |
| nn::fs::DirectoryHandle | 8 | 8 | 0x00: armazenamento u64 | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/fs/fs_types.h) | — |
| nn::fs::FileHandle | 8 | 8 | 0x00: armazenamento u64 | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/fs/fs_types.h) | — |
| nn::fs::FsContext | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::fs::WriteOption | 4 | 4 | 0x00: flags int | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/fs/fs_types.h) | — |
| nn::hid::GyroscopeZeroDriftMode | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::hid::NpadCommunicationMode | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::hid::NpadControllerColor | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::hid::NpadFullKeyState | 40 | 8 | 0x00: sampling u64; 0x08: botoes u64; 0x10/0x18: analogicos s32[2]; 0x20: atributos u32; 0x24: padding | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/hid.h) | — |
| nn::hid::NpadHandheldActivationMode | 4 | 4 | enum; transporte int; valores na fonte | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/hid.h) | — |
| nn::hid::NpadHandheldState | 40 | 8 | 0x00: sampling u64; 0x08: botoes u64; 0x10/0x18: analogicos s32[2]; 0x20: atributos u32; 0x24: padding | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/hid.h) | — |
| nn::hid::NpadJoyDeviceType | 4 | 4 | enum; transporte int; valores na fonte | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/hid.h) | — |
| nn::hid::NpadJoyDualState | 40 | 8 | 0x00: sampling u64; 0x08: botoes u64; 0x10/0x18: analogicos s32[2]; 0x20: atributos u32; 0x24: padding | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/hid.h) | — |
| nn::hid::NpadJoyHoldType | 4 | 4 | enum; transporte int; valores na fonte | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/hid.h) | — |
| nn::hid::NpadJoyLeftState | 40 | 8 | 0x00: sampling u64; 0x08: botoes u64; 0x10/0x18: analogicos s32[2]; 0x20: atributos u32; 0x24: padding | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/hid.h) | — |
| nn::hid::NpadJoyRightState | 40 | 8 | 0x00: sampling u64; 0x08: botoes u64; 0x10/0x18: analogicos s32[2]; 0x20: atributos u32; 0x24: padding | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/hid.h) | — |
| nn::hid::SixAxisSensorHandle | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::hid::SixAxisSensorState | 96 | 8 | 0x00/08: delta/sampling u64; 0x10/1c/28: aceleracao/velocidade/angulo float[3]; 0x34: matriz float[3][3]; 0x58: atributos u32; 0x5c: padding | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/hid.h) | — |
| nn::hid::VibrationDeviceHandle | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::hid::VibrationValue | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::nifm::ProxySetting | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::oe::CpuBoostMode | 4 | 4 | enum repr(C); valores pequenos na fonte publica | [fonte](https://github.com/ultimate-research/nnsdk-rs/blob/d23f131663ec72a87c84914e6e738999f3ab403d/src/nn/oe.rs) | — |
| nn::oe::FocusHandlingMode | 4 | 4 | enum; transporte int; valores na fonte | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/oe.h) | — |
| nn::oe::FocusState | 4 | 4 | enum; transporte int; valores na fonte | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/oe.h) | — |
| nn::oe::PerformanceMode | 4 | 4 | enum; transporte int; valores na fonte | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/oe.h) | — |
| nn::os::EventClearMode | 4 | 4 | alias inteiro de 32 bits na fonte publica | [fonte](https://github.com/ultimate-research/nnsdk-rs/blob/d23f131663ec72a87c84914e6e738999f3ab403d/src/nn/os.rs) | — |
| nn::os::SystemEvent | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::os::SystemEventType | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::pl::LoadState | 4 | 4 | enum; transporte int; valores na fonte | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/pl.h) | — |
| nn::pl::SharedFontType | 4 | 4 | enum; transporte int; valores na fonte | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/pl.h) | — |
| nn::settings::FullKeyGenericPadMap | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::settings::FullKeyIndex | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::settings::LanguageCode | 8 | 1 | 0x00: char[8] | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/settings.h) | 0x1690da4 |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::time::CalendarAdditionalInfo | 24 | 4 | 0x00: dia da semana int; 0x04: dia do ano s32; 0x08: char[8]; 0x10: bool; 0x11: padding[3]; 0x14: desvio UTC s32 | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/time.h) | — |
| nn::time::CalendarTime | 8 | 2 | 0x00: s16 ano; 0x02/03/04/05/06: s8 mes/dia/hora/minuto/segundo; 0x07: padding | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/time.h) | — |
| nn::time::LocationName | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::time::PosixTime | 8 | 8 | 0x00: contador de 64 bits (fonte C++ usa u64) | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/time.h) | — |
| nn::time::StandardNetworkSystemClock | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::time::TimeZoneRule | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::util::BitFlagSet<32, nn::hid::NpadStyleTag> | 4 | 4 | 0x00: uint32_t _storage[1] | [fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/util/util_BitFlagSet.h) | 0x19dca38 |
| nn::util::Uuid | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::util::neon::Vector4fType | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::vi::Display | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |
| nn::vi::Layer | ? | ? | nao_confirmado; nao substitua por estrutura/enum de tamanho presumido | nao_confirmado | — |

## Declarações conflitantes e escopos pendentes

- nn::fs::Unmount(char const*): nn::Result ([fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/fs/fs_mount.h)); void ([fonte](https://github.com/ultimate-research/nnsdk-rs/blob/d23f131663ec72a87c84914e6e738999f3ab403d/src/nn/fs.rs#L276))
- nn::hid::GetNpadStates(nn::hid::NpadHandheldState*, int, unsigned int const&): int ([fonte](https://github.com/open-ead/nnheaders/blob/246c3b482d045934ed004f50c5ada37f3b10b703/include/nn/hid.h)); void ([fonte](https://github.com/ultimate-research/nnsdk-rs/blob/d23f131663ec72a87c84914e6e738999f3ab403d/src/nn/hid.rs#L88))

this/estático sem confirmação:

- nn::account::AsyncNetworkServiceLicenseInfoContext::GetNetworkServiceLicenseKind()
- nn::time::StandardNetworkSystemClock::GetCurrentTime(nn::time::PosixTime*)
- nn::time::AsyncEnsureNetworkClockAvailabilityTask::GetFinishEvent()
- nn::time::AsyncEnsureNetworkClockAvailabilityTask::GetResult()
- nn::time::AsyncEnsureNetworkClockAvailabilityTask::Initialize(nn::os::EventClearMode)
- nn::time::AsyncEnsureNetworkClockAvailabilityTask::StartTask()
- nn::err::ApplicationErrorArg::SetLanguageCode(nn::settings::LanguageCode const&)
- nn::err::NplnContext::Make(void const*, unsigned long)

## Evidências binárias de transporte

| Chamada main RVA | Tipo/evento | Bytes | Mnemônico | Limite |
|---|---|---:|---|---|
| 0x19dca38 | nn::util::BitFlagSet<32, nn::hid::NpadStyleTag> | 4 | mov | transporte no subregistrador w0; nao prova sizeof isoladamente |
| 0x1690da4 | nn::settings::LanguageCode | 8 | str | copia de retorno de 8 bytes; alinhamento e campos dependem da fonte publica |
| 0x1e36630 | retorno | 16 | stp | transporte de x0+x1; nao prova tipo nominal ou sizeof completo |

Candidatos que leem x8 de entrada antes de redefinir (não promovidos a retorno indireto sem declaração):

Nenhum no bloco inicial inspecionado.

## Saídas e autoverificações

- r5_abi.csv e r5_types.csv: colunas e classificação solicitadas.
- r5_public_abi.csv e r5_abi_function_status.csv: declarações, fontes e status por símbolo.
- r5_abi_binary_evidence.csv e r5_abi_x8_candidates.csv: observações estáticas sem listar instruções.
- r5_abi_sources.csv: URLs fixadas e SHA-256 das fontes públicas.

1. Cobertura exata das importações selecionadas, uma linha de retorno por símbolo; CSV relido e comparado integralmente.
2. Nenhum argumento nomeia x8+ ou v8+ como registrador ordinário; testes sintéticos cobrem nove/dez argumentos, bancos independentes, alinhamento 16, HFA de 32 bytes, cópia não trivial e resultado x8.
3. Todo precisa_tamanho está na tabela de tipos, incluindo o marcador nao_declarado para protótipo de retorno ausente.

## Comandos de aceite

```text
build\venv\Scripts\python.exe -I analysis\recomp\r5_abi.py
Importacoes selecionadas: 208; nn::os excluidas: 102
Fontes publicas fixadas: 34 arquivos; sem tentativas repetidas
Funcoes: 208; linhas ABI: 524; tipos documentados: 64
Argumentos por classe: agregado=16; enum=13; flutuante=2; inteiro=51; ponteiro=121; precisa_prototipo=8; precisa_tamanho=4; referencia=75; this=26
Retornos indiretos x8 confirmados por tipo: 0; candidatos x8 na entrada SDK: 0
Funcoes com precisa_tamanho: 107; this nao confirmado: 8
Retornos sem declaracao: 104; conflitos publicos: 2
AUTOVERIFICACAO 1: OK
AUTOVERIFICACAO 2: OK
AUTOVERIFICACAO 3: OK
Saidas: analysis/recomp/generated/r5_abi*.csv, r5_types.csv, r5_public_abi.csv e r5_abi_report.md
Exit code: 0
```

```text
python tools\check_repo_safety.py
OK: 263 arquivos verificados

Exit code: 0
```

A segurança acima é executada com o mesmo interpretador em modo isolado; o comando independente também deve ser executado no aceite. Sem commit ou push.
