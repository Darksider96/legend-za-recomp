# Censo de kernel e serviços para R5

Levantamento estático de `main` e `sdk`, sem execução ou relocação das imagens. RVAs são locais a cada módulo. Somente metadados estão neste relatório; evidências detalhadas ficam nos CSVs ignorados em `generated/`.

## Resultado

- SVCs: main=0; sdk=58; fora de funcao=0; desconhecidas=0
- Servicos: linhas=3301; nomes distintos=868; referencias=3552
- PLT main: 911 resolvidas no SDK; 6 nao resolvidas
- Grafo SDK: 82640 sites diretos resolvidos; 60872 arestas distintas; 928 sites nao resolvidos
- Importacoes funcao: 946 raizes resolvidas; 0 nao resolvidas; sem SVC ate profundidade 12=423

O conjunto presente no SDK é uma capacidade do módulo; o conjunto alcançável pelas importações é uma aproximação estática por chamadas diretas. Nenhum dos dois demonstra execução efetiva no boot.

## Método e fronteiras

- Capstone `5.0.7` confirma cada SVC identificado por máscara A64. Demangler observado: `c++filt.EXE`. Inventários de funções: main=223410, sdk=32097.
- Funções conhecidas usam `.eh_frame`; no main incluem também `gap_functions.csv`. Exportações são associadas pelo RVA exato para as SVCs. Um wrapper sem símbolo fica sem exportação; não recebe um nome inventado `nn::svc`.
- Números e nomes públicos das SVCs: [wrappers libnx](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/kernel/svc.s). Catálogo de serviços: [clientes libnx](https://github.com/switchbrew/libnx/tree/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/services) e nomes fornecidos na tarefa. Revisão fixada: `b156b4774fb2df7b93402169509266da7547fc4c`; 128 nomes no catálogo local.
- Busca em `.rodata`: ASCII de 2–8 bytes entre limites NUL. Nomes do catálogo incluem maiúsculas; nomes adicionais precisam corresponder a `[a-z0-9:-]{2,8}` e ter referência ADRP+ADD ou ADR. Esses adicionais são candidatos de formato, não serviços IPC confirmados. Strings conhecidas sem referência permanecem no inventário com função/exportação vazias e não alimentam o grafo.
- Referências ADRP+ADD permitem registradores de destino diferentes e até 12 instruções, dentro da mesma função, sem sobrescrita do registrador de página ou transferência de controle intermediária. Nomes montados por constantes imediatas, tabelas de ponteiros, cópias ou memória mutável não são recuperados.
- Para serviços, a exportação é exata quando coincide com a função; caso contrário é contida ou a anterior mais próxima. `service_name_evidence.csv` distingue esses casos e registra a distância. Nome próximo não é atribuição confirmada.
- Grafo do SDK: BL e B que sai da função; alvos são funções conhecidas. Stubs da PLT do próprio SDK são resolvidos pelo GOT/JUMP_SLOT e pelo nome exato da exportação, sem consumir nível adicional. Chamadas sem destino conhecido são auditadas, não silenciosamente tratadas como código puro.
- Busca BFS por importação de função resolvida: raiz no nível 0, até 12 arestas. O nível 12 é inspecionado, sem expansão. BR/BLR, callbacks, vtables, condicionais entre funções e retorno do SDK para callbacks do main não são seguidos. O grafo não é sensível a argumentos nem a caminhos executáveis.
- `servicos` no CSV de alcance contém os nomes de ambas as classes; o CSV de auditoria separa catálogo e formato. O relatório separa as classes para não promover strings genéricas a serviços reais.
- Ausência de SVC neste grafo limitado não prova pureza nem que uma API dispense suporte nativo. A lista abaixo significa somente ausência de SVC nas arestas analisadas.

## SVCs por número

| SVC | Nome Horizon | Main | SDK | Exportações dos wrappers / RVAs sem símbolo |
|---|---|---:|---:|---|
| 0x1 | SetHeapSize | 0 | 1 | sdk:0x16ffd4 |
| 0x2 | SetMemoryPermission | 0 | 1 | sdk:0x16ffec |
| 0x3 | SetMemoryAttribute | 0 | 1 | sdk:0x16fff4 |
| 0x4 | MapMemory | 0 | 1 | sdk:0x16fffc |
| 0x5 | UnmapMemory | 0 | 1 | sdk:0x170004 |
| 0x6 | QueryMemory | 0 | 1 | sdk:0x17000c |
| 0x7 | ExitProcess | 0 | 1 | sdk:0x170024 |
| 0x8 | CreateThread | 0 | 1 | sdk:0x17002c |
| 0x9 | StartThread | 0 | 1 | sdk:0x170044 |
| 0xa | ExitThread | 0 | 1 | sdk:0x17004c |
| 0xb | SleepThread | 0 | 1 | sdk:0x170054 |
| 0xc | GetThreadPriority | 0 | 1 | sdk:0x17005c |
| 0xd | SetThreadPriority | 0 | 1 | sdk:0x170074 |
| 0xe | GetThreadCoreMask | 0 | 1 | sdk:0x17007c |
| 0xf | SetThreadCoreMask | 0 | 1 | sdk:0x17009c |
| 0x10 | GetCurrentProcessorNumber | 0 | 1 | sdk:0x1700a4 |
| 0x11 | SignalEvent | 0 | 1 | sdk:0x1700ac |
| 0x12 | ClearEvent | 0 | 1 | sdk:0x1700b4 |
| 0x13 | MapSharedMemory | 0 | 1 | sdk:0x1700bc |
| 0x14 | UnmapSharedMemory | 0 | 1 | sdk:0x1700c4 |
| 0x15 | CreateTransferMemory | 0 | 1 | sdk:0x1700cc |
| 0x16 | CloseHandle | 0 | 1 | sdk:0x1700e4 |
| 0x17 | ResetSignal | 0 | 1 | sdk:0x1700ec |
| 0x18 | WaitSynchronization | 0 | 1 | sdk:0x1700f4 |
| 0x19 | CancelSynchronization | 0 | 1 | sdk:0x17010c |
| 0x1a | ArbitrateLock | 0 | 1 | sdk:0x170114 |
| 0x1b | ArbitrateUnlock | 0 | 1 | sdk:0x17011c |
| 0x1c | WaitProcessWideKeyAtomic | 0 | 1 | sdk:0x170124 |
| 0x1d | SignalProcessWideKey | 0 | 1 | sdk:0x17012c |
| 0x1f | ConnectToNamedPort | 0 | 1 | sdk:0x170134 |
| 0x21 | SendSyncRequest | 0 | 1 | sdk:0x17014c |
| 0x22 | SendSyncRequestWithUserBuffer | 0 | 1 | sdk:0x170154 |
| 0x23 | SendAsyncRequestWithUserBuffer | 0 | 1 | sdk:0x17015c |
| 0x25 | GetThreadId | 0 | 1 | sdk:0x170174 |
| 0x26 | Break | 0 | 1 | sdk:0x17018c |
| 0x27 | OutputDebugString | 0 | 1 | sdk:0x170194 |
| 0x28 | ReturnFromException | 0 | 1 | sdk:0x170324 |
| 0x29 | GetInfo | 0 | 1 | sdk:0x17019c |
| 0x2a | FlushEntireDataCache | 0 | 1 | sdk:0x1701b4 |
| 0x2c | MapPhysicalMemory | 0 | 1 | sdk:0x1701bc |
| 0x2d | UnmapPhysicalMemory | 0 | 1 | sdk:0x1701c4 |
| 0x2f | GetLastThreadInfo | 0 | 1 | sdk:0x1701cc |
| 0x32 | SetThreadActivity | 0 | 1 | sdk:0x170204 |
| 0x33 | GetThreadContext3 | 0 | 1 | sdk:0x17020c |
| 0x34 | WaitForAddress | 0 | 1 | sdk:0x17032c |
| 0x35 | SignalToAddress | 0 | 1 | sdk:0x170214 |
| 0x36 | SynchronizePreemptionState | 0 | 1 | sdk:0x17021c |
| 0x40 | CreateSession | 0 | 1 | sdk:0x170264 |
| 0x41 | AcceptSession | 0 | 1 | sdk:0x170284 |
| 0x43 | ReplyAndReceive | 0 | 1 | sdk:0x17029c |
| 0x44 | ReplyAndReceiveWithUserBuffer | 0 | 1 | sdk:0x1702b4 |
| 0x45 | CreateEvent | 0 | 1 | sdk:0x1702cc |
| 0x4b | CreateCodeMemory | 0 | 1 | sdk:0x170224 |
| 0x50 | CreateSharedMemory | 0 | 1 | sdk:0x17023c |
| 0x51 | MapTransferMemory | 0 | 1 | sdk:0x170254 |
| 0x52 | UnmapTransferMemory | 0 | 1 | sdk:0x17025c |
| 0x70 | CreatePort | 0 | 1 | sdk:0x1702ec |
| 0x72 | ConnectToPort | 0 | 1 | sdk:0x17030c |

SVCs fora de funções conhecidas (exceções explícitas):

Nenhuma.

## Serviços encontrados

| Módulo | Strings de catálogo (RVAs distintos) | Candidatos de formato referenciados |
|---|---:|---:|
| main | 8 | 640 |
| sdk | 69 | 201 |

Nomes do catálogo presentes e quantidade de funções que referenciam:

| Nome | Main | SDK |
|---|---:|---:|
| acc:su | 0 | 1 |
| acc:u0 | 0 | 1 |
| acc:u1 | 0 | 1 |
| aoc:u | 0 | 1 |
| appletAE | 0 | 6 |
| appletOE | 0 | 1 |
| aud:a | 0 | 1 |
| aud:d | 0 | 1 |
| audin:u | 0 | 1 |
| audout:u | 0 | 1 |
| audrec:u | 0 | 1 |
| audren:u | 0 | 1 |
| bcat:u | 0 | 1 |
| bsd:u | 0 | 1 |
| bt | 1 | 0 |
| caps:su | 0 | 1 |
| caps:u | 0 | 1 |
| friend:a | 0 | 1 |
| friend:m | 0 | 1 |
| friend:s | 0 | 1 |
| friend:u | 0 | 1 |
| friend:v | 0 | 1 |
| fsp-ldr | 0 | 1 |
| fsp-pr | 0 | 1 |
| fsp-srv | 0 | 1 |
| hid | 2 | 1 |
| hid:dbg | 0 | 1 |
| hid:sys | 0 | 2 |
| hidbus | 0 | 1 |
| hwopus | 0 | 1 |
| ldn:m | 0 | 1 |
| ldn:s | 0 | 1 |
| ldn:u | 0 | 2 |
| lr | 0 | 0 |
| mii:e | 0 | 1 |
| mii:u | 0 | 1 |
| miiimg | 0 | 1 |
| nfc:user | 0 | 1 |
| nfp:dbg | 0 | 1 |
| nfp:sys | 0 | 1 |
| nfp:user | 0 | 1 |
| nifm:a | 0 | 1 |
| nifm:s | 0 | 1 |
| notif:a | 0 | 1 |
| nvdrv | 0 | 0 |
| pctl | 0 | 0 |
| pctl:a | 0 | 0 |
| pctl:r | 0 | 0 |
| pctl:s | 0 | 0 |
| pl:s | 0 | 1 |
| pl:u | 0 | 1 |
| prepo:u | 0 | 1 |
| set | 1 | 1 |
| set:cal | 0 | 1 |
| set:fd | 0 | 1 |
| set:sys | 0 | 1 |
| sm: | 0 | 1 |
| sm:m | 0 | 1 |
| ssl | 1 | 1 |
| tc | 0 | 0 |
| time:a | 0 | 1 |
| time:r | 0 | 1 |
| time:s | 0 | 1 |
| time:su | 0 | 1 |
| time:u | 0 | 1 |
| ts | 0 | 0 |
| vi:m | 0 | 2 |
| vi:s | 0 | 4 |
| vi:u | 0 | 2 |

## 30 namespaces nn:: com mais chamadas diretas do main

Prioridade contada por `call_targets.csv` + `plt_map.csv`; inclui namespaces com zero chamadas quando necessário. SVCs/nomes são a união das importações do namespace, não multiplicados pela contagem de chamadas.

| Namespace | Chamadas | SVCs | Serviços do catálogo | Candidatos de formato |
|---|---:|---|---|---|
| nn::os | 27700 | 0x5, 0x8, 0x9, 0xb, 0xd, 0xf, 0x10, 0x12, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x25, 0x26, 0x29, 0x34, 0x35, 0x36 | — | — |
| nn::err | 3056 | 0xb, 0x15, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36, 0x52 | — | — |
| nn::async | 2609 | 0x5, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 | — | — |
| nn::util | 170 | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 | — | — |
| nn::diag | 156 | 0x6, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 | — | — |
| nn::fs | 124 | 0xb, 0x15, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36, 0x52 | fsp-srv, sm: | — |
| nn::hid | 110 | 0xb, 0x13, 0x14, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 | hid, sm: | — |
| nn::ldn | 79 | 0xb, 0x15, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36, 0x52 | ldn:u, sm: | — |
| nn::fontll | 74 | — | — | rb |
| nn::account | 64 | 0xb, 0x15, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36, 0x52 | acc:u0, sm: | — |
| nn::socket | 61 | 0xb, 0x15, 0x16, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36, 0x52 | bsd:u, sm: | bsd:a, bsd:s, sfdnsres |
| nn::time | 50 | 0xb, 0x13, 0x14, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 | sm:, time:a, time:r, time:s, time:su, time:u | ntc |
| nn::bcat | 41 | 0xb, 0x12, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 | bcat:u, fsp-srv, sm: | bcat:a, bcat:m, bcat:s |
| nn::crypto | 39 | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 | sm: | — |
| nn::oe | 32 | 0x5, 0x8, 0x9, 0xb, 0xf, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x25, 0x26, 0x29, 0x36 | appletOE, sm: | — |
| nn::audio | 30 | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 | audout:u, audren:u, sm: | — |
| nn::mem | 23 | 0x2, 0x3, 0x6, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x29, 0x2c, 0x2d, 0x36 | — | — |
| nn::ssl | 22 | 0xb, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 | sm:, ssl | ssl:s |
| nn::lmem | 18 | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 | — | — |
| nn::settings | 16 | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 | — | — |
| nn::nifm | 15 | 0xb, 0x12, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36 | nifm:a, nifm:s, sm: | — |
| nn::vi | 14 | 0xb, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 | sm:, vi:m, vi:s, vi:u | inf, nan |
| nn::detail | 12 | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 | — | — |
| nn::swkbd | 11 | 0x15, 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36, 0x52 | — | — |
| nn::prepo | 9 | 0xb, 0x11, 0x16, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36, 0x45 | prepo:u, sm: | prepo:a2, prepo:m, prepo:s |
| nn::web | 8 | 0x16, 0x17, 0x18, 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 | — | — |
| nn::codec | 8 | 0xb, 0x15, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x22, 0x26, 0x36, 0x52 | hwopus, sm: | — |
| nn::pia | 8 | — | — | — |
| nn::pl | 5 | 0xb, 0x13, 0x14, 0x16, 0x1a, 0x1b, 0x1c, 0x1d, 0x1f, 0x21, 0x26, 0x36 | pl:u, sm: | — |
| nn::aoc | 4 | 0x1a, 0x1b, 0x1c, 0x1d, 0x26, 0x36 | — | — |

## Importações sem SVC alcançada no grafo até nível 12

423 importações; não é uma prova de pureza. A auditoria registra cortes de profundidade, chamadas indiretas e fronteiras não resolvidas por importação.

- Libz_SigloMiddlewareInfo
- nn::fs::GetCurrentThreadFsContext()
- nn::fs::SetCurrentThreadFsContext(nn::fs::FsContext const*)
- nn::os::GetHostArgc()
- nn::os::GetHostArgv()
- nn::os::GetThreadId(nn::os::ThreadType const*)
- nn::os::GetTlsValue(nn::os::TlsSlot)
- nn::os::SetTlsValue(nn::os::TlsSlot, unsigned long)
- nn::os::FinalizeEvent(nn::os::EventType*)
- nn::os::FinalizeMutex(nn::os::MutexType*)
- nn::os::GetSystemTick()
- nn::os::GetCurrentFiber()
- nn::os::InitializeEvent(nn::os::EventType*, bool, nn::os::EventClearMode)
- nn::os::InitializeMutex(nn::os::MutexType*, bool, int)
- nn::os::GetCurrentThread()
- nn::os::ConvertToTimeSpan(nn::os::Tick)
- nn::os::FinalizeMultiWait(nn::os::MultiWaitType*)
- nn::os::FinalizeSemaphore(nn::os::SemaphoreType*)
- nn::os::GetThreadPriority(nn::os::ThreadType const*)
- nn::os::FinalizeLightEvent(nn::os::LightEventType*)
- nn::os::FinalizeTimerEvent(nn::os::TimerEventType*)
- nn::os::InitializeMultiWait(nn::os::MultiWaitType*)
- nn::os::InitializeSemaphore(nn::os::SemaphoreType*, int, int)
- nn::os::LinkMultiWaitHolder(nn::os::MultiWaitType*, nn::os::MultiWaitHolderType*)
- nn::os::TryAcquireWriteLock(nn::os::ReaderWriterLockType*)
- nn::os::FinalizeMessageQueue(nn::os::MessageQueueType*)
- nn::os::GetThreadNamePointer(nn::os::ThreadType const*)
- nn::os::InitializeLightEvent(nn::os::LightEventType*, bool, nn::os::EventClearMode)
- nn::os::InitializeTimerEvent(nn::os::TimerEventType*, nn::os::EventClearMode)
- nn::os::SetThreadNamePointer(nn::os::ThreadType*, char const*)
- nn::os::UnlinkMultiWaitHolder(nn::os::MultiWaitHolderType*)
- nn::os::GetSystemTickFrequency()
- nn::os::InitializeMessageQueue(nn::os::MessageQueueType*, unsigned long*, unsigned long)
- nn::os::FinalizeMultiWaitHolder(nn::os::MultiWaitHolderType*)
- nn::os::FinalizeReaderWriterLock(nn::os::ReaderWriterLockType*)
- nn::os::FinalizeConditionVariable(nn::os::ConditionVariableType*)
- nn::os::FinalizeLightMessageQueue(nn::os::LightMessageQueueType*)
- nn::os::InitializeMultiWaitHolder(nn::os::MultiWaitHolderType*, nn::os::EventType*)
- nn::os::InitializeReaderWriterLock(nn::os::ReaderWriterLockType*)
- nn::os::InitializeConditionVariable(nn::os::ConditionVariableType*)
- nn::os::InitializeLightMessageQueue(nn::os::LightMessageQueueType*, unsigned long*, unsigned long)
- nn::os::IsMutexLockedByCurrentThread(nn::os::MutexType const*)
- nn::err::NplnContext::Make(void const*, unsigned long)
- nn::err::ErrorResultVariant::ErrorResultVariant(nn::Result const&)
- nn::err::ErrorResultVariant::ErrorResultVariant()
- nn::err::ErrorResultVariant::operator=(nn::err::ErrorCode const&)
- nn::err::ErrorResultVariant::operator=(nn::Result const&)
- nn::err::ApplicationErrorArg::SetLanguageCode(nn::settings::LanguageCode const&)
- nn::err::ApplicationErrorArg::SetDialogMessage(char const*)
- nn::err::ApplicationErrorArg::SetFullScreenMessage(char const*)
- nn::err::ApplicationErrorArg::SetApplicationErrorCodeNumber(unsigned int)
- nn::err::ApplicationErrorArg::ApplicationErrorArg()
- nn::ldn::Disconnect()
- nn::ldn::OpenStation()
- nn::ldn::ScanPrivate(nn::ldn::NetworkInfo*, int*, int, nn::ldn::ScanFilter const&, int)
- nn::ldn::CloseStation()
- nn::ldn::CreateNetwork(nn::ldn::NetworkConfig const&, nn::ldn::SecurityConfig const&, nn::ldn::UserConfig const&)
- nn::ldn::DestroyNetwork()
- nn::ldn::GetIpv4Address(nn::ldn::Ipv4Address*, nn::ldn::SubnetMask*)
- nn::ldn::GetNetworkInfo(nn::ldn::NetworkInfo*)
- nn::ldn::OpenAccessPoint()
- nn::ldn::CloseAccessPoint()
- nn::ldn::SetAdvertiseData(void const*, unsigned long)
- nn::ldn::GetDisconnectReason()
- nn::ldn::AddAcceptFilterEntry(nn::ldn::MacAddress)
- nn::ldn::CreateNetworkPrivate(nn::ldn::NetworkConfig const&, nn::ldn::SecurityConfig const&, nn::ldn::SecurityParameter const&, nn::ldn::UserConfig const&, int, nn::ldn::AddressEntry const*)
- nn::ldn::GetSecurityParameter(nn::ldn::SecurityParameter*)
- nn::ldn::MakeBroadcastAddress(nn::ldn::Ipv4Address, nn::ldn::SubnetMask)
- nn::ldn::SetStationAcceptPolicy(nn::ldn::AcceptPolicy)
- nn::ldn::Scan(nn::ldn::NetworkInfo*, int*, int, nn::ldn::ScanFilter const&, int)
- nn::ldn::Reject(nn::ldn::Ipv4Address)
- nn::ldn::GetState()
- nn::mem::StandardAllocator::StandardAllocator()
- nn::ngc::ProfanityFilter::ProfanityFilter()
- nn::ssl::BuiltInManager::GetBuiltInCertificates(nn::ssl::BuiltInCertificateInfoBase**, unsigned char*, unsigned int, nn::ssl::CaCertificateId*, unsigned int)
- nn::ssl::BuiltInManager::GetBuiltInCertificateBufSize(unsigned int*, nn::ssl::CaCertificateId*, unsigned int)
- nn::ssl::GetSslResultFromValue(nn::Result*, char const*, unsigned int)
- nn::ssl::Context::ImportClientPki(unsigned long*, char const*, char const*, unsigned int, unsigned int)
- nn::ssl::Context::ImportServerPki(unsigned long*, char const*, unsigned int, nn::ssl::CertificateFormat)
- nn::ssl::Context::ImportCrl(unsigned long*, char const*, unsigned int)
- nn::ssl::Context::Context()
- nn::ssl::Context::~Context()
- nn::web::ShowWebPageArg::SetCallbackUrl(char const*)
- nn::web::ShowWebPageArg::SetDisplayUrlKind(nn::web::WebDisplayUrlKind)
- nn::web::ShowWebPageArg::ShowWebPageArg(char const*)
- nn::web::WebPageReturnValue::WebPageReturnValue()
- nn::bcat::DeliveryCacheFile::DeliveryCacheFile()
- nn::bcat::DeliveryCacheProgress::DeliveryCacheProgress()
- nn::bcat::DeliveryCacheDirectory::DeliveryCacheDirectory()
- nn::diag::InitializeAbortObserverHolder(nn::diag::AbortObserverHolder*, void (*)(nn::diag::AbortInfo const&))
- nn::diag::detail::NotNullImpl(unsigned long)
- nn::lmem::CreateExpHeap(void*, unsigned long, int)
- nn::lmem::CreateFrameHeap(void*, unsigned long, int)
- nn::lmem::DestroyFrameHeap(nn::lmem::detail::HeapHead*)
- nn::time::IsLeapYear(int)
- nn::time::IsValidDate(int, int, int)
- nn::time::ToPosixTime(int*, nn::time::PosixTime*, int, nn::time::CalendarTime const&, nn::time::TimeZoneRule const&)
- nn::time::ToCalendarTime(nn::time::CalendarTime*, nn::time::CalendarAdditionalInfo*, nn::time::PosixTime const&)
- nn::time::LoadTimeZoneRule(nn::time::TimeZoneRule*, nn::time::LocationName const&)
- nn::time::ToCalendarTimeInUtc(nn::time::PosixTime const&)
- nn::time::StandardUserSystemClock::GetCurrentTime(nn::time::PosixTime*)
- nn::time::StandardNetworkSystemClock::GetCurrentTime(nn::time::PosixTime*)
- nn::time::AsyncEnsureNetworkClockAvailabilityTask::GetFinishEvent()
- nn::time::AsyncEnsureNetworkClockAvailabilityTask::GetResult()
- nn::time::AsyncEnsureNetworkClockAvailabilityTask::StartTask()
- nn::time::AsyncEnsureNetworkClockAvailabilityTask::AsyncEnsureNetworkClockAvailabilityTask()
- nn::util::ReferSymbol(void const*)
- nn::util::RelocationTable::Unrelocate()
- nn::util::RelocationTable::Relocate()
- nn::util::BinaryFileHeader::GetFirstBlock()
- nn::util::BinaryFileHeader::GetRelocationTable()
- nn::util::ConvertCharacterUtf8ToUtf32(unsigned int*, char const*)
- nn::util::ConvertStringUtf16NativeToUtf8(char*, int, unsigned short const*)
- nn::util::PickOutCharacterFromUtf8String(char*, char const**)
- nn::util::TinyMt::Initialize(unsigned int const*, int)
- nn::util::TinyMt::GenerateRandomU32()
- nn::util::detail::IntrusiveDicImpl::MoveIter(nn::util::IntrusiveDicNode const*, int)
- nn::util::detail::IntrusiveDicImpl::EraseImpl(nn::util::IntrusiveDicNode*)
- nn::util::BitArray::reset()
- nn::async::Executable::Executable()
- nn::async::Executable::~Executable()
- nn::async::QueuingExecutor::QueuingExecutor(nn::MemoryResource*)
- nn::async::EnqueueExecutable(nn::async::Executor*, std::__1::shared_ptr<nn::async::Executable>, nn::async::CancellationToken const&, nn::async::ExecutionBlocker)
- nn::async::DumpAllTaskDebugData()
- nn::async::CancellationTokenSource::CancellationTokenSource(nn::MemoryResource*)
- nn::async::detail::FutureStateBase::FutureStateBase()
- nn::async::detail::TaskCoordinatorBase::GetSynchronousContinuationList()
- nn::async::detail::TaskCoordinatorBase::TaskCoordinatorBase(nn::MemoryResource*, nn::async::detail::FutureStateBase*, nn::async::detail::FutureStateBase*)
- nn::async::detail::FiberTaskCoordinatorBase::GetSynchronousContinuationList()
- nn::async::detail::SynchronousContinuationList::AddContinuation(std::__1::shared_ptr<nn::async::Executable>, bool, nn::async::CancellationToken const&)
- nn::async::detail::SynchronousContinuationList::SynchronousContinuationList(nn::MemoryResource*)
- nn::audio::InitializeResampler(nn::audio::ResamplerType*, void*, unsigned long, int, int, int)
- nn::audio::SetAudioOutBufferInfo(nn::audio::AudioOutBuffer*, void*, unsigned long, unsigned long)
- nn::audio::ProcessResamplerBuffer(nn::audio::ResamplerType*, int*, short*, unsigned long, short const*, int)
- nn::audio::GetAudioOutChannelCount(nn::audio::AudioOut const*)
- nn::audio::InitializeAudioOutParameter(nn::audio::AudioOutParameter*)
- nn::audio::GetAudioOutBufferDataPointer(nn::audio::AudioOutBuffer const*)
- nn::audio::GetResamplerOutputSampleCount(nn::audio::ResamplerType const*, int)
- nn::audio::GetRequiredBufferSizeForResampler(int)
- nn::codec::ResetHardwareOpusDecoder(nn::codec::HardwareOpusDecoderType*)
- nn::codec::FinalizeHardwareOpusDecoder(nn::codec::HardwareOpusDecoderType*)
- nn::codec::GetHardwareOpusDecoderSampleRate(nn::codec::HardwareOpusDecoderType const*)
- nn::codec::GetHardwareOpusDecoderChannelCount(nn::codec::HardwareOpusDecoderType const*)
- nn::prepo::PlayReport::SetEventId(char const*)
- nn::prepo::PlayReport::Clear()
- nn::prepo::PlayReport::SetBuffer(void*, unsigned long)
- nn::prepo::PlayReport::PlayReport()
- nn::swkbd::SetSubText(nn::swkbd::KeyboardConfig*, char16_t const*)
- nn::swkbd::SetHeaderText(nn::swkbd::KeyboardConfig*, char16_t const*)
- nn::swkbd::SetInitialText(nn::swkbd::ShowKeyboardArg*, char16_t const*)
- nn::swkbd::SetUserWordList(nn::swkbd::ShowKeyboardArg*, nn::swkbd::UserWord const*, int)
- nn::swkbd::SetTextCheckCallback(nn::swkbd::ShowKeyboardArg*, nn::swkbd::TextCheckResult (*)(void*, unsigned long*, nn::swkbd::String*))
- nn::swkbd::GetRequiredWorkBufferSize(bool)
- nn::swkbd::GetRequiredWorkBufferSize(int)
- nn::swkbd::GetRequiredStringBufferSize()
- nn::swkbd::GetRequiredTextCheckWorkBufferSize()
- nn::crypto::detail::Sha256Impl::Initialize()
- nn::crypto::detail::Sha256Impl::Update(void const*, unsigned long)
- nn::crypto::detail::Sha256Impl::GetHash(void*, unsigned long)
- nn::crypto::detail::Sha256Impl::~Sha256Impl()
- nn::crypto::detail::BigNum::ReserveStatic(unsigned int*, int)
- nn::crypto::detail::BigNum::Set(void const*, unsigned long)
- nn::crypto::detail::BigNum::Cleanse()
- nn::crypto::detail::BigNum::BigNum()
- nn::crypto::detail::BigNum::~BigNum()
- nn::crypto::detail::Sha1Impl::Initialize()
- nn::crypto::detail::Sha1Impl::Update(void const*, unsigned long)
- nn::crypto::detail::Sha1Impl::GetHash(void*, unsigned long)
- nn::crypto::detail::Sha1Impl::~Sha1Impl()
- nn::fontll::ScalableFontEngine::GetAdvance(short*, short*, int*, int*, unsigned int, unsigned short)
- nn::fontll::ScalableFontEngine::GetKerning(int*, int*, unsigned int, unsigned int)
- nn::fontll::ScalableFontEngine::Initialize(void*, unsigned int)
- nn::fontll::ScalableFontEngine::SetAutoHint(bool)
- nn::fontll::ScalableFontEngine::ReleasesGlyph(void*)
- nn::fontll::ScalableFontEngine::SetBoldWeight(int)
- nn::fontll::ScalableFontEngine::GetFontMetrics(nn::fontll::Metrics*)
- nn::fontll::ScalableFontEngine::AcquireGlyphmap(unsigned int, unsigned short)
- nn::fontll::ScalableFontEngine::CheckGlyphExist(unsigned int)
- nn::fontll::ScalableFontEngine::SetOutlineWidth(unsigned short)
- nn::fontll::ScalableFontEngine::AcquireOtfKerning(nn::fontll::OtfKerningTable const*, unsigned int, unsigned int, unsigned int)
- nn::fontll::ScalableFontEngine::AcquireOtfKerningLast(nn::fontll::OtfKerningTable const*, unsigned int, unsigned int)
- nn::fontll::ScalableFontEngine::AcquireOtfKerningFirst(nn::fontll::OtfKerningTable const*, unsigned int, unsigned int)
- nn::fontll::ScalableFontEngine::GetPointerToWorkBuffer()
- nn::fontll::ScalableFontEngine::FinalizeOtfKerningTable(nn::fontll::OtfKerningTable*, void (*)(void*, void*), void*)
- nn::fontll::ScalableFontEngine::InitializeOtfKerningTable(void* (*)(unsigned long, unsigned long, void*), void*, bool)
- nn::fontll::ScalableFontEngine::SetFont(char*)
- nn::fontll::ScalableFontEngine::Finalize()
- nn::fontll::ScalableFontEngine::GetError()
- nn::fontll::ScalableFontEngine::LoadFont(char*, void const*, unsigned int, unsigned int)
- nn::fontll::ScalableFontEngine::SetFlags(nn::fontll::ScalableFontEngine::Flags)
- nn::fontll::ScalableFontEngine::SetScale(int, int, int, int)
- nn::fontll::ScalableFontEngine::ScalableFontEngine()
- nn::fontll::ScalableFontEngineHelper::Decode(void const*, unsigned int, unsigned long (*)(void*, unsigned long, void const*, unsigned long, void const*, unsigned long, void const*, unsigned long), unsigned char const*)
- nn::socket::InetHtonl(unsigned int)
- nn::socket::InetHtons(unsigned short)
- nn::socket::InetNtohl(unsigned int)
- nn::socket::InetNtohs(unsigned short)
- nn::account::GetNickname(nn::account::Nickname*, nn::account::Uid const&)
- nn::account::AsyncContext::Cancel()
- nn::account::AsyncContext::HasDone(bool*)
- nn::account::AsyncContext::GetResult()
- nn::account::AsyncContext::AsyncContext()
- nn::account::GetUserExistence(bool*, nn::account::Uid const&)
- nn::account::GetNetworkServiceAccountId(nn::account::NetworkServiceAccountId*, nn::account::UserHandle const&)
- nn::account::IsNetworkServiceAccountAvailable(bool*, nn::account::UserHandle const&)
- nn::account::AsyncNetworkServiceLicenseInfoContext::AsyncNetworkServiceLicenseInfoContext(nn::account::AsyncNetworkServiceLicenseInfoContext&&)
- nn::account::AsyncNetworkServiceLicenseInfoContext::AsyncNetworkServiceLicenseInfoContext()
- nn::account::LoadNetworkServiceAccountIdTokenCache(unsigned long*, char*, unsigned long, nn::account::UserHandle const&)
- nn::account::CloseUser(nn::account::UserHandle const&)
- nn::account::GetUserId(nn::account::Uid*, nn::account::UserHandle const&)
- nn::err::ErrorResultVariant::GetState() const
- nn::err::ErrorResultVariant::operator nn::err::ErrorCode() const
- nn::err::ErrorResultVariant::operator nn::Result() const
- nn::err::ErrorCode::IsValid() const
- nn::hid::SixAxisSensorState::GetQuaternion(nn::util::neon::Vector4fType*) const
- nn::web::WebPageReturnValue::GetLastUrl() const
- nn::web::WebPageReturnValue::GetLastUrlSize() const
- nn::web::WebPageReturnValue::GetWebExitReason() const
- nn::util::BinaryFileHeader::IsRelocated() const
- nn::util::BinaryFileHeader::GetAlignment() const
- nn::util::BinaryFileHeader::GetFirstBlock() const
- nn::util::BinaryFileHeader::IsEndianReverse() const
- nn::util::BinaryFileHeader::IsAlignmentValid() const
- nn::util::BinaryFileHeader::IsSignatureValid(long) const
- nn::util::BinaryBlockHeader::GetBlockSize() const
- nn::util::BinaryBlockHeader::GetNextBlock() const
- nn::util::detail::IntrusiveDicImpl::FindImpl(nn::util::basic_string_view<char, std::__1::char_traits<char> > const&) const
- nn::util::detail::IntrusiveDicImpl::FindIter() const
- nn::async::CancellationToken::IsCancellationRequested() const
- nn::async::CancellationTokenSource::GetToken() const
- nn::prepo::PlayReport::GetCount() const
- nn::crypto::detail::BigNum::ModExp(void*, void const*, nn::crypto::detail::BigNum const&, unsigned long, unsigned int*, unsigned long) const
- std::logic_error::what() const
- std::__1::__shared_weak_count::__get_deleter(std::type_info const&) const
- std::__1::__match_any_but_newline<char>::__exec(std::__1::__state<char>&) const
- std::__1::ios_base::getloc() const
- std::exception::what() const
- std::logic_error::logic_error(std::logic_error const&)
- std::logic_error::~logic_error()
- std::out_of_range::~out_of_range()
- std::__1::__get_sp_mut(void const*)
- std::__1::bad_weak_ptr::~bad_weak_ptr()
- std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::~basic_string()
- std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::operator=(char)
- std::__1::basic_istream<char, std::__1::char_traits<char> >::~basic_istream()
- std::__1::basic_ostream<char, std::__1::char_traits<char> >::~basic_ostream()
- std::__1::random_device::~random_device()
- std::__1::basic_iostream<char, std::__1::char_traits<char> >::~basic_iostream()
- std::__1::__get_classname(char const*, bool)
- std::__1::basic_streambuf<char, std::__1::char_traits<char> >::sync()
- std::__1::basic_streambuf<char, std::__1::char_traits<char> >::imbue(std::__1::locale const&)
- std::__1::basic_streambuf<char, std::__1::char_traits<char> >::uflow()
- std::__1::basic_streambuf<char, std::__1::char_traits<char> >::setbuf(char*, long)
- std::__1::basic_streambuf<char, std::__1::char_traits<char> >::xsgetn(char*, long)
- std::__1::basic_streambuf<char, std::__1::char_traits<char> >::xsputn(char const*, long)
- std::__1::basic_streambuf<char, std::__1::char_traits<char> >::showmanyc()
- std::__1::basic_streambuf<char, std::__1::char_traits<char> >::~basic_streambuf()
- std::__1::__shared_weak_count::__release_weak()
- std::__1::__shared_weak_count::lock()
- std::__1::__shared_weak_count::~__shared_weak_count()
- std::__1::__get_collation_name(char const*)
- std::__1::align(unsigned long, unsigned long, void*&, unsigned long&)
- void std::__1::__sort<std::__1::__less<int, int>&, int*>(int*, int*, std::__1::__less<int, int>&)
- void std::__1::__sort<std::__1::__less<long, long>&, long*>(long*, long*, std::__1::__less<long, long>&)
- void std::__1::__sort<std::__1::__less<unsigned long, unsigned long>&, unsigned long*>(unsigned long*, unsigned long*, std::__1::__less<unsigned long, unsigned long>&)
- std::__1::chrono::system_clock::from_time_t(long)
- std::__1::locale::locale(std::__1::locale const&)
- std::__1::locale::~locale()
- std::__1::locale::operator=(std::__1::locale const&)
- std::exception::~exception()
- non-virtual thunk to nn::async::detail::TaskCoordinatorBase::GetSynchronousContinuationList()
- non-virtual thunk to nn::async::detail::FiberTaskCoordinatorBase::GetSynchronousContinuationList()
- operator delete[](void*)
- operator delete(void*, std::align_val_t)
- __errno_location
- _longjmp
- _setjmp
- acos
- acosf
- asin
- asinf
- atan2
- atan2f
- atanf
- atof
- atoi
- atol
- bsearch
- clock
- clock_gettime
- cos
- cosf
- cosh
- exp
- exp2
- exp2f
- expf
- fmod
- fmodf
- frexp
- getenv
- getpid
- gettimeofday
- gmtime
- gmtime_r
- htons
- hypotf
- isalnum
- isalpha
- iscntrl
- isprint
- ispunct
- isxdigit
- ldexp
- localeconv
- localtime
- localtime_r
- log
- log10
- log10f
- log2
- logf
- longjmp
- memalign
- memchr
- memcmp
- memcpy
- memmove
- memset
- memset_s
- mktime
- modff
- nextafterf
- nnResultGetDescription
- nnResultGetModule
- nnResultIsFailure
- nnResultIsSuccess
- nnosConvertTickToNanoSeconds
- nnosFinalizeEvent
- nnosFinalizeMutex
- nnosGetCurrentThread
- nnosGetSystemTick
- nnosGetSystemTickFrequency
- nnosGetThreadPriority
- nnosInitializeEvent
- nnosInitializeMutex
- nnosSetThreadNamePointer
- nnsocketInetHtonl
- nnsocketInetHtons
- nnsocketInetNtohl
- nnsocketInetNtohs
- nnsslConnectionDoHandshakeWithCertBuffer
- nnsslConnectionGetConnectionId
- nnsslConnectionGetNextAlpnProto
- nnsslConnectionGetVerifyCertError
- nnsslConnectionPeek
- nnsslConnectionPending
- nnsslConnectionRead
- nnsslConnectionSetHostName
- nnsslConnectionSetIoMode
- nnsslConnectionSetNextAlpnProto
- nnsslConnectionSetOption
- nnsslConnectionSetSessionCacheMode
- nnsslConnectionSetSocketDescriptor
- nnsslConnectionSetVerifyOption
- nnsslConnectionWrite
- nnsslContextGetContextId
- ntohs
- nvnBootstrapLoader
- pow
- powf
- pthread_cond_destroy
- pthread_cond_init
- pthread_condattr_init
- pthread_mutex_destroy
- pthread_mutex_init
- pthread_mutexattr_destroy
- pthread_mutexattr_init
- pthread_rwlock_destroy
- pthread_rwlock_init
- pthread_self
- qsort
- rand
- setjmp
- sin
- sinf
- sinh
- sqrt
- sqrtf
- strcasecmp
- strcat
- strchr
- strcmp
- strcoll
- strcpy
- strcspn
- strdup
- strerror
- strerror_r
- strlen
- strncasecmp
- strncat
- strncmp
- strncpy
- strnlen
- strpbrk
- strrchr
- strspn
- strstr
- strtod
- strtof
- strtok
- strtol
- strtoll
- strtoul
- strtoull
- tan
- tanf
- tanh
- time
- tolower
- toupper
- wmemchr

## Resolução das 917 importações com stub

911 resolvidas; 6 não resolvidas pelo nome de exportação de função do SDK.

- _ZTHN9grpc_core26ApplicationCallbackExecCtx18callback_exec_ctx_E (0x2fc6710)
- _ZTHN9grpc_core7ExecCtx9exec_ctx_E (0x2fc6720)
- _ZN2nn3pia27ZSTD_trace_decompress_beginEPKNS0_11ZSTD_DCtx_sE (0x2fc6f20)
- _ZN2nn3pia25ZSTD_trace_decompress_endEyPKNS0_10ZSTD_TraceE (0x2fc6f30)
- _ZN2nn3pia25ZSTD_trace_compress_beginEPKNS0_11ZSTD_CCtx_sE (0x2fc6f40)
- _ZN2nn3pia23ZSTD_trace_compress_endEyPKNS0_10ZSTD_TraceE (0x2fc6f50)

Importações de função sem raiz conhecida no SDK (incluindo as sem stub):

Nenhuma.

## Arquivos e autoverificações

- `svc_sites.csv`, `service_names.csv`, `import_kernel_reach.csv`: entregas principais.
- `service_name_evidence.csv`: referências, origem do nome e qualidade da exportação associada.
- `main_plt_sdk_resolution.csv`, `sdk_plt_resolution.csv`: resolução por símbolo e GOT.
- `sdk_direct_call_edges.csv`, `sdk_unresolved_direct_edges.csv`: arestas e fronteiras do grafo.
- `import_kernel_reach_audit.csv`, `import_kernel_paths.csv`: limites e caminhos mínimos por evento.
- `kernel_namespace_reach.csv`: prioridade e alcance por namespace.

1. SVCs dentro de função conhecida ou listadas na seção de exceções; nenhuma exceção é ocultada.
2. Todo número recebe nome público ou o marcador `desconhecida`.
3. Resolução das 917 entradas da PLT contada e lista completa das não resolvidas.
4. CSVs principais relidos e comparados integralmente, incluindo ordem das colunas e contagens. Testes sintéticos validam B/BL, ADRP, profundidade inclusiva 12, ciclos e caminhos mínimos.

## Comandos de aceite

```text
build\venv\Scripts\python.exe -I analysis\recomp\kernel_census.py
Capstone 5.0.7; profundidade maxima SDK=12
SDK PLT: 7961 stubs; resolvidos internos=7951
main: SVC=0; strings candidatas brutas=5929; referencias=3113
sdk: SVC=58; strings candidatas brutas=2358; referencias=439
SVCs: main=0; sdk=58; fora de funcao=0; desconhecidas=0
Servicos: linhas=3301; nomes distintos=868; referencias=3552
PLT main: 911 resolvidas no SDK; 6 nao resolvidas
Grafo SDK: 82640 sites diretos resolvidos; 60872 arestas distintas; 928 sites nao resolvidos
Importacoes funcao: 946 raizes resolvidas; 0 nao resolvidas; sem SVC ate profundidade 12=423
AUTOVERIFICACAO 1: OK
AUTOVERIFICACAO 2: OK
AUTOVERIFICACAO 3: OK
AUTOVERIFICACAO 4: OK
Saidas: analysis/recomp/generated/*.csv e analysis/recomp/kernel_census_report.md
Exit code: 0
```

```text
python tools\check_repo_safety.py
OK: 259 arquivos verificados

Exit code: 0
```

A verificação acima é invocada pelo script com o mesmo Python em modo isolado; o comando independente deve ser repetido no aceite. Sem commit ou push.
