# Prioridade das Funções do SDK para a Camada HLE

## Resumo Geral
- **Importações com stub (PLT):** 917
- **Total de chamadas diretas a importações:** 130557
- **Importações chamadas diretamente:** 854
- **Importações com 0 chamadas diretas:** 63 (usadas via ponteiro ou inicializadores)
- **Namespaces identificados:** 44

## Top 50 Importações Mais Chamadas

| # | Importação (demangled) | Namespace | Chamadas |
| :--- | :--- | :--- | :--- |
| 1 | abort | c | 29339 |
| 2 | __cxa_guard_acquire | c | 15899 |
| 3 | __cxa_guard_release | c | 15889 |
| 4 | nn::os::SignalLightEvent(nn::os::LightEventType*) | nn::os | 8444 |
| 5 | std::__1::__shared_weak_count::__release_weak() | std | 7407 |
| 6 | nn::os::WaitLightEvent(nn::os::LightEventType*) | nn::os | 6368 |
| 7 | memcpy | c | 4991 |
| 8 | strlen | c | 4089 |
| 9 | memset | c | 3758 |
| 10 | nn::err::ErrorResultVariant::ErrorResultVariant(nn::Result const&) | nn::err | 3025 |
| 11 | memmove | c | 2985 |
| 12 | _Unwind_Resume | c | 2132 |
| 13 | nn::os::FinalizeLightEvent(nn::os::LightEventType*) | nn::os | 2120 |
| 14 | nn::os::UnlockMutex(nn::os::MutexType*) | nn::os | 1988 |
| 15 | nn::os::LockMutex(nn::os::MutexType*) | nn::os | 1741 |
| 16 | nn::os::InitializeLightEvent(nn::os::LightEventType*, bool, nn::os::EventClearMode) | nn::os | 1560 |
| 17 | std::__1::__next_prime(unsigned long) | std | 1420 |
| 18 | snprintf | c | 1286 |
| 19 | nn::os::GetCurrentThread() | nn::os | 1063 |
| 20 | std::__1::__shared_weak_count::~__shared_weak_count() | std | 950 |
| 21 | strcmp | c | 866 |
| 22 | memcmp | c | 828 |
| 23 | nn::async::Executable::~Executable() | nn::async | 623 |
| 24 | nn::os::FinalizeMutex(nn::os::MutexType*) | nn::os | 521 |
| 25 | nn::os::InitializeMutex(nn::os::MutexType*, bool, int) | nn::os | 470 |
| 26 | std::__1::__call_once(unsigned long volatile&, void*, void (*)(void*)) | std | 360 |
| 27 | nn::os::SignalEvent(nn::os::EventType*) | nn::os | 321 |
| 28 | nn::os::FinalizeEvent(nn::os::EventType*) | nn::os | 318 |
| 29 | nn::async::detail::SynchronousContinuationList::~SynchronousContinuationList() | nn::async | 286 |
| 30 | nn::os::InitializeEvent(nn::os::EventType*, bool, nn::os::EventClearMode) | nn::os | 275 |
| 31 | nn::os::ReleaseReadLock(nn::os::ReaderWriterLockType*) | nn::os | 270 |
| 32 | nn::os::GetSystemTick() | nn::os | 251 |
| 33 | nn::async::detail::FutureStateBase::FutureStateBase() | nn::async | 248 |
| 34 | TLS init function for grpc_core::ExecCtx::exec_ctx_ | grpc_core | 247 |
| 35 | nn::os::ReleaseWriteLock(nn::os::ReaderWriterLockType*) | nn::os | 229 |
| 36 | nn::os::AcquireReadLock(nn::os::ReaderWriterLockType*) | nn::os | 217 |
| 37 | nn::async::detail::GroupWaitHolderOne::~GroupWaitHolderOne() | nn::async | 204 |
| 38 | nn::async::ExecutionResult::ExecutionResult(nn::async::ExecutionResultCode, nn::async::ResumeBlocker*) | nn::async | 201 |
| 39 | nn::os::AcquireWriteLock(nn::os::ReaderWriterLockType*) | nn::os | 194 |
| 40 | strrchr | c | 154 |
| 41 | nn::async::detail::FiberTaskCoordinatorBase::GetSynchronousContinuationList() | nn::async | 150 |
| 42 | strncmp | c | 148 |
| 43 | nn::async::detail::AwaitImpl(nn::async::ResumeBlocker*) | nn::async | 136 |
| 44 | nn::async::detail::TaskCoordinatorBase::TaskCoordinatorBase(nn::MemoryResource*, nn::async::detail::FutureStateBase*, nn::async::detail::FutureStateBase*) | nn::async | 126 |
| 45 | exp2f | c | 124 |
| 46 | nn::os::SignalConditionVariable(nn::os::ConditionVariableType*) | nn::os | 124 |
| 47 | sinf | c | 123 |
| 48 | std::__1::__shared_weak_count::lock() | std | 117 |
| 49 | memchr | c | 109 |
| 50 | fmodf | c | 107 |

## Importações por Namespace

### Namespace `c` (84551 chamadas, 252 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| abort | 29339 |
| __cxa_guard_acquire | 15899 |
| __cxa_guard_release | 15889 |
| memcpy | 4991 |
| strlen | 4089 |
| memset | 3758 |
| memmove | 2985 |
| _Unwind_Resume | 2132 |
| snprintf | 1286 |
| strcmp | 866 |
| memcmp | 828 |
| strrchr | 154 |
| strncmp | 148 |
| exp2f | 124 |
| sinf | 123 |
| memchr | 109 |
| fmodf | 107 |
| sqrtf | 104 |
| cosf | 101 |
| strchr | 84 |
| strncpy | 83 |
| strstr | 79 |
| powf | 67 |
| __errno_location | 65 |
| strcpy | 56 |
| acosf | 49 |
| atoi | 45 |
| tolower | 40 |
| tanf | 31 |
| __cxa_guard_abort | 30 |
| strcat | 27 |
| bsearch | 24 |
| expf | 23 |
| time | 23 |
| sched_yield | 21 |
| atan2f | 20 |
| strspn | 20 |
| wmemchr | 20 |
| toupper | 19 |
| vsnprintf | 18 |
| fmod | 17 |
| modff | 17 |
| pthread_mutex_unlock | 17 |
| qsort | 17 |
| nnsocketClose | 16 |
| strtol | 16 |
| fwrite | 14 |
| strtoul | 14 |
| cos | 13 |
| pthread_mutex_lock | 13 |
| pthread_rwlock_unlock | 13 |
| __cxa_begin_catch | 12 |
| __cxa_end_catch | 12 |
| atof | 12 |
| exp2 | 12 |
| hypotf | 12 |
| strcasecmp | 12 |
| strcoll | 12 |
| strncat | 12 |
| nnsocketSetSockOpt | 10 |
| pthread_rwlock_wrlock | 10 |
| rand | 10 |
| asinf | 9 |
| fread | 9 |
| fseek | 9 |
| nextafterf | 9 |
| strtod | 9 |
| __cxa_rethrow | 8 |
| localtime_r | 8 |
| sprintf | 8 |
| atanf | 7 |
| fclose | 7 |
| getenv | 7 |
| log10f | 7 |
| strftime | 7 |
| exp | 6 |
| fopen | 6 |
| getc | 6 |
| log | 6 |
| logf | 6 |
| nnsocketGetSockOpt | 6 |
| sscanf | 6 |
| strtof | 6 |
| fflush | 5 |
| fputc | 5 |
| fputs | 5 |
| log10 | 5 |
| nnsocketSocket | 5 |
| sin | 5 |
| strerror | 5 |
| strncasecmp | 5 |
| strtoll | 5 |
| frexp | 4 |
| localeconv | 4 |
| localtime | 4 |
| mktime | 4 |
| nnsocketGetSockName | 4 |
| pow | 4 |
| pthread_rwlock_destroy | 4 |
| pthread_rwlock_init | 4 |
| strtok | 4 |
| __cxa_thread_atexit | 3 |
| atol | 3 |
| clock | 3 |
| clock_gettime | 3 |
| fseeko | 3 |
| fstat | 3 |
| getentropy | 3 |
| isalnum | 3 |
| ldexp | 3 |
| log2 | 3 |
| nnsocketPoll | 3 |
| pthread_cond_broadcast | 3 |
| pthread_once | 3 |
| pthread_self | 3 |
| sqrt | 3 |
| strnlen | 3 |
| strtoull | 3 |
| Libz_SigloMiddlewareInfo | 2 |
| __cxa_allocate_exception | 2 |
| __cxa_throw | 2 |
| at_quick_exit | 2 |
| close | 2 |
| ferror | 2 |
| fprintf | 2 |
| ftell | 2 |
| getpid | 2 |
| gmtime_r | 2 |
| inet_ntop | 2 |
| isalpha | 2 |
| iscntrl | 2 |
| nnsocketBind | 2 |
| nnsocketGAIStrError | 2 |
| nnsocketGetAddrInfo | 2 |
| nnsocketSendMsg | 2 |
| pthread_mutex_destroy | 2 |
| pthread_mutex_init | 2 |
| pthread_rwlock_rdlock | 2 |
| strpbrk | 2 |
| tan | 2 |
| __nnmusl_init_dso | 1 |
| _longjmp | 1 |
| _setjmp | 1 |
| acos | 1 |
| asin | 1 |
| atan2 | 1 |
| closedir | 1 |
| cosh | 1 |
| feof | 1 |
| freopen | 1 |
| ftello | 1 |
| htons | 1 |
| inet_pton | 1 |
| isprint | 1 |
| ispunct | 1 |
| isxdigit | 1 |
| longjmp | 1 |
| memalign | 1 |
| memset_s | 1 |
| nanosleep | 1 |
| nnosConvertTickToNanoSeconds | 1 |
| nnosGetSystemTick | 1 |
| nnosGetSystemTickFrequency | 1 |
| nnsocketAccept | 1 |
| nnsocketConnect | 1 |
| nnsocketFcntl | 1 |
| nnsocketFreeAddrInfo | 1 |
| nnsocketGetPeerName | 1 |
| nnsocketListen | 1 |
| nnsocketRecvMsg | 1 |
| nnsocketShutdown | 1 |
| ntohs | 1 |
| open | 1 |
| opendir | 1 |
| printf | 1 |
| pthread_cond_destroy | 1 |
| pthread_cond_init | 1 |
| pthread_cond_signal | 1 |
| pthread_cond_timedwait | 1 |
| pthread_cond_wait | 1 |
| pthread_condattr_init | 1 |
| pthread_join | 1 |
| pthread_mutexattr_destroy | 1 |
| pthread_mutexattr_init | 1 |
| read | 1 |
| readdir | 1 |
| rewind | 1 |
| setjmp | 1 |
| sinh | 1 |
| stat | 1 |
| strcspn | 1 |
| strerror_r | 1 |
| tanh | 1 |
| vfprintf | 1 |
| fgets | 0 |
| gettimeofday | 0 |
| gmtime | 0 |
| nnResultGetDescription | 0 |
| nnResultGetModule | 0 |
| nnResultIsFailure | 0 |
| nnResultIsSuccess | 0 |
| nndetailDiagAbortNoMessageImpl | 0 |
| nnosCreateThread | 0 |
| nnosDestroyThread | 0 |
| nnosFinalizeEvent | 0 |
| nnosFinalizeMutex | 0 |
| nnosGetCurrentThread | 0 |
| nnosGetThreadCoreMask | 0 |
| nnosGetThreadPriority | 0 |
| nnosInitializeEvent | 0 |
| nnosInitializeMutex | 0 |
| nnosLockMutex | 0 |
| nnosSetThreadCoreMask | 0 |
| nnosSetThreadNamePointer | 0 |
| nnosSignalEvent | 0 |
| nnosSleepThread | 0 |
| nnosStartThread | 0 |
| nnosUnlockMutex | 0 |
| nnosWaitEvent | 0 |
| nnosWaitThread | 0 |
| nnsocketGetHostByNameWithOptions | 0 |
| nnsocketGetLastErrno | 0 |
| nnsocketInetHtonl | 0 |
| nnsocketInetHtons | 0 |
| nnsocketInetNtohl | 0 |
| nnsocketInetNtohs | 0 |
| nnsocketRecv | 0 |
| nnsocketResolverGetOption | 0 |
| nnsocketResolverSetOption | 0 |
| nnsocketSend | 0 |
| nnsocketSetLastErrno | 0 |
| nnsslConnectionCreate | 0 |
| nnsslConnectionDestroy | 0 |
| nnsslConnectionDoHandshakeWithCertBuffer | 0 |
| nnsslConnectionGetConnectionId | 0 |
| nnsslConnectionGetNextAlpnProto | 0 |
| nnsslConnectionGetVerifyCertError | 0 |
| nnsslConnectionPeek | 0 |
| nnsslConnectionPending | 0 |
| nnsslConnectionRead | 0 |
| nnsslConnectionSetHostName | 0 |
| nnsslConnectionSetIoMode | 0 |
| nnsslConnectionSetNextAlpnProto | 0 |
| nnsslConnectionSetOption | 0 |
| nnsslConnectionSetSessionCacheMode | 0 |
| nnsslConnectionSetSocketDescriptor | 0 |
| nnsslConnectionSetVerifyOption | 0 |
| nnsslConnectionWrite | 0 |
| nnsslContextDestroy | 0 |
| nnsslContextGetContextId | 0 |
| nnsslFinalize | 0 |
| nnsslInitialize | 0 |

### Namespace `nn::os` (27700 chamadas, 102 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::os::SignalLightEvent(nn::os::LightEventType*) | 8444 |
| nn::os::WaitLightEvent(nn::os::LightEventType*) | 6368 |
| nn::os::FinalizeLightEvent(nn::os::LightEventType*) | 2120 |
| nn::os::UnlockMutex(nn::os::MutexType*) | 1988 |
| nn::os::LockMutex(nn::os::MutexType*) | 1741 |
| nn::os::InitializeLightEvent(nn::os::LightEventType*, bool, nn::os::EventClearMode) | 1560 |
| nn::os::GetCurrentThread() | 1063 |
| nn::os::FinalizeMutex(nn::os::MutexType*) | 521 |
| nn::os::InitializeMutex(nn::os::MutexType*, bool, int) | 470 |
| nn::os::SignalEvent(nn::os::EventType*) | 321 |
| nn::os::FinalizeEvent(nn::os::EventType*) | 318 |
| nn::os::InitializeEvent(nn::os::EventType*, bool, nn::os::EventClearMode) | 275 |
| nn::os::ReleaseReadLock(nn::os::ReaderWriterLockType*) | 270 |
| nn::os::GetSystemTick() | 251 |
| nn::os::ReleaseWriteLock(nn::os::ReaderWriterLockType*) | 229 |
| nn::os::AcquireReadLock(nn::os::ReaderWriterLockType*) | 217 |
| nn::os::AcquireWriteLock(nn::os::ReaderWriterLockType*) | 194 |
| nn::os::SignalConditionVariable(nn::os::ConditionVariableType*) | 124 |
| nn::os::WaitEvent(nn::os::EventType*) | 84 |
| nn::os::AcquireSemaphore(nn::os::SemaphoreType*) | 72 |
| nn::os::GetThreadId(nn::os::ThreadType const*) | 72 |
| nn::os::ReleaseSemaphore(nn::os::SemaphoreType*) | 65 |
| nn::os::SendLightMessageQueue(nn::os::LightMessageQueueType*, unsigned long) | 64 |
| nn::os::ConvertToTimeSpan(nn::os::Tick) | 63 |
| nn::os::InitializeMultiWaitHolder(nn::os::MultiWaitHolderType*, nn::os::EventType*) | 62 |
| nn::os::TimedWaitEvent(nn::os::EventType*, nn::TimeSpan) | 58 |
| nn::os::TryWaitEvent(nn::os::EventType*) | 58 |
| nn::os::FinalizeReaderWriterLock(nn::os::ReaderWriterLockType*) | 53 |
| nn::os::FinalizeSemaphore(nn::os::SemaphoreType*) | 51 |
| nn::os::InitializeReaderWriterLock(nn::os::ReaderWriterLockType*) | 42 |
| nn::os::IsMutexLockedByCurrentThread(nn::os::MutexType const*) | 37 |
| nn::os::ReleaseSemaphore(nn::os::SemaphoreType*, int) | 32 |
| nn::os::TryAcquireWriteLock(nn::os::ReaderWriterLockType*) | 32 |
| nn::os::SleepThread(nn::TimeSpan) | 29 |
| nn::os::WaitConditionVariable(nn::os::ConditionVariableType*, nn::os::MutexType*) | 26 |
| nn::os::InitializeSemaphore(nn::os::SemaphoreType*, int, int) | 24 |
| nn::os::FinalizeConditionVariable(nn::os::ConditionVariableType*) | 22 |
| nn::os::TryAcquireSemaphore(nn::os::SemaphoreType*) | 21 |
| nn::os::TryWaitSystemEvent(nn::os::SystemEventType*) | 17 |
| nn::os::WaitThread(nn::os::ThreadType*) | 15 |
| nn::os::ChangeThreadPriority(nn::os::ThreadType*, int) | 14 |
| nn::os::InitializeConditionVariable(nn::os::ConditionVariableType*) | 13 |
| nn::os::FinalizeMultiWaitHolder(nn::os::MultiWaitHolderType*) | 12 |
| nn::os::DestroySystemEvent(nn::os::SystemEventType*) | 11 |
| nn::os::DestroyThread(nn::os::ThreadType*) | 9 |
| nn::os::GetTlsValue(nn::os::TlsSlot) | 9 |
| nn::os::SetThreadCoreMask(nn::os::ThreadType*, int, unsigned long) | 9 |
| nn::os::SetTlsValue(nn::os::TlsSlot, unsigned long) | 9 |
| nn::os::CreateThread(nn::os::ThreadType*, void (*)(void*), void*, void*, unsigned long, int) | 8 |
| nn::os::StartThread(nn::os::ThreadType*) | 8 |
| nn::os::SetThreadName(nn::os::ThreadType*, char const*) | 7 |
| nn::os::ClearEvent(nn::os::EventType*) | 6 |
| nn::os::GetCurrentCoreNumber() | 6 |
| nn::os::BroadcastConditionVariable(nn::os::ConditionVariableType*) | 5 |
| nn::os::FinalizeTimerEvent(nn::os::TimerEventType*) | 5 |
| nn::os::FreeTlsSlot(nn::os::TlsSlot) | 5 |
| nn::os::GetSystemTickFrequency() | 5 |
| nn::os::GetThreadAvailableCoreMask() | 5 |
| nn::os::GetThreadPriority(nn::os::ThreadType const*) | 5 |
| nn::os::CreateThread(nn::os::ThreadType*, void (*)(void*), void*, void*, unsigned long, int, int) | 4 |
| nn::os::InitializeTimerEvent(nn::os::TimerEventType*, nn::os::EventClearMode) | 4 |
| nn::os::SwitchToFiber(nn::os::FiberType*) | 4 |
| nn::os::YieldThread() | 4 |
| nn::os::AllocateTlsSlot(nn::os::TlsSlot*, void (*)(unsigned long)) | 3 |
| nn::os::GenerateRandomBytes(void*, unsigned long) | 3 |
| nn::os::GetCurrentFiber() | 3 |
| nn::os::GetThreadNamePointer(nn::os::ThreadType const*) | 3 |
| nn::os::StartOneShotTimerEvent(nn::os::TimerEventType*, nn::TimeSpan) | 3 |
| nn::os::ClearSystemEvent(nn::os::SystemEventType*) | 2 |
| nn::os::FinalizeFiber(nn::os::FiberType*) | 2 |
| nn::os::FinalizeLightMessageQueue(nn::os::LightMessageQueueType*) | 2 |
| nn::os::InitializeLightMessageQueue(nn::os::LightMessageQueueType*, unsigned long*, unsigned long) | 2 |
| nn::os::JamLightMessageQueue(nn::os::LightMessageQueueType*, unsigned long) | 2 |
| nn::os::LinkMultiWaitHolder(nn::os::MultiWaitType*, nn::os::MultiWaitHolderType*) | 2 |
| nn::os::QueryMemoryInfo(nn::os::MemoryInfo*) | 2 |
| nn::os::ReceiveLightMessageQueue(unsigned long*, nn::os::LightMessageQueueType*) | 2 |
| nn::os::SdkAllocateTlsSlot(nn::os::TlsSlot*, void (*)(unsigned long)) | 2 |
| nn::os::UnlinkMultiWaitHolder(nn::os::MultiWaitHolderType*) | 2 |
| nn::os::WaitSystemEvent(nn::os::SystemEventType*) | 2 |
| nn::os::FinalizeMessageQueue(nn::os::MessageQueueType*) | 1 |
| nn::os::FinalizeMultiWait(nn::os::MultiWaitType*) | 1 |
| nn::os::GetCurrentStackInfo(unsigned long*, unsigned long*) | 1 |
| nn::os::GetHostArgc() | 1 |
| nn::os::GetHostArgv() | 1 |
| nn::os::GetVirtualAddressMemoryResourceUsage() | 1 |
| nn::os::InitializeFiber(nn::os::FiberType*, nn::os::FiberType* (*)(void*), void*, void*, unsigned long, int) | 1 |
| nn::os::InitializeMessageQueue(nn::os::MessageQueueType*, unsigned long*, unsigned long) | 1 |
| nn::os::InitializeMultiWait(nn::os::MultiWaitType*) | 1 |
| nn::os::InitializeMultiWaitHolder(nn::os::MultiWaitHolderType*, nn::os::SystemEventType*) | 1 |
| nn::os::JamMessageQueue(nn::os::MessageQueueType*, unsigned long) | 1 |
| nn::os::ReceiveMessageQueue(unsigned long*, nn::os::MessageQueueType*) | 1 |
| nn::os::SendMessageQueue(nn::os::MessageQueueType*, unsigned long) | 1 |
| nn::os::SetDyingMessageRegion(unsigned long, unsigned long) | 1 |
| nn::os::SetMemoryAllocatorForThreadLocal(void* (*)(unsigned long, unsigned long), void (*)(void*, unsigned long)) | 1 |
| nn::os::SetThreadNamePointer(nn::os::ThreadType*, char const*) | 1 |
| nn::os::SignalTimerEvent(nn::os::TimerEventType*) | 1 |
| nn::os::StartPeriodicTimerEvent(nn::os::TimerEventType*, nn::TimeSpan, nn::TimeSpan) | 1 |
| nn::os::TryJamMessageQueue(nn::os::MessageQueueType*, unsigned long) | 1 |
| nn::os::TryReceiveMessageQueue(unsigned long*, nn::os::MessageQueueType*) | 1 |
| nn::os::TrySendMessageQueue(nn::os::MessageQueueType*, unsigned long) | 1 |
| nn::os::WaitAny(nn::os::MultiWaitType*) | 1 |
| nn::os::WaitTimerEvent(nn::os::TimerEventType*) | 1 |

### Namespace `std` (11070 chamadas, 68 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| std::__1::__shared_weak_count::__release_weak() | 7407 |
| std::__1::__next_prime(unsigned long) | 1420 |
| std::__1::__shared_weak_count::~__shared_weak_count() | 950 |
| std::__1::__call_once(unsigned long volatile&, void*, void (*)(void*)) | 360 |
| std::__1::__shared_weak_count::lock() | 117 |
| std::__1::basic_ios<char, std::__1::char_traits<char> >::~basic_ios() | 88 |
| std::__1::basic_streambuf<char, std::__1::char_traits<char> >::~basic_streambuf() | 60 |
| std::__1::__throw_system_error(int, char const*) | 51 |
| std::__1::basic_iostream<char, std::__1::char_traits<char> >::~basic_iostream() | 51 |
| std::__1::chrono::steady_clock::now() | 46 |
| std::__1::basic_ostream<char, std::__1::char_traits<char> >::operator<<(int) | 42 |
| std::__1::mutex::unlock() | 42 |
| std::__1::__sp_mut::unlock() | 37 |
| std::__1::basic_streambuf<char, std::__1::char_traits<char> >::basic_streambuf() | 33 |
| std::__1::ios_base::init(void*) | 32 |
| std::__1::__get_sp_mut(void const*) | 29 |
| std::__1::__sp_mut::lock() | 29 |
| std::__1::locale::~locale() | 26 |
| std::__1::chrono::system_clock::from_time_t(long) | 24 |
| std::__1::mutex::lock() | 23 |
| std::__1::__libcpp_verbose_abort(char const*, ...) | 22 |
| std::__1::basic_ostream<char, std::__1::char_traits<char> >::~basic_ostream() | 20 |
| std::__1::to_string(int) | 17 |
| std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::operator=(char) | 15 |
| std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::__init(char const*, unsigned long) | 13 |
| std::__1::locale::use_facet(std::__1::locale::id&) const | 12 |
| std::__1::locale::locale(std::__1::locale const&) | 10 |
| std::__1::basic_ostream<char, std::__1::char_traits<char> >::operator<<(unsigned long) | 9 |
| std::__1::random_device::operator()() | 9 |
| std::__1::stoi(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&, unsigned long*, int) | 7 |
| std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::assign(char const*, unsigned long) | 6 |
| std::__1::basic_istream<char, std::__1::char_traits<char> >::~basic_istream() | 4 |
| std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::operator=(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&) | 4 |
| std::__1::locale::locale() | 4 |
| std::__1::stof(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&, unsigned long*) | 4 |
| std::__1::this_thread::sleep_for(std::__1::chrono::duration<long long, std::__1::ratio<1l, 1000000000l> > const&) | 4 |
| std::__1::to_string(unsigned int) | 3 |
| std::logic_error::logic_error(char const*) | 3 |
| std::__1::__get_collation_name(char const*) | 2 |
| std::__1::basic_ostream<char, std::__1::char_traits<char> >::sentry::~sentry() | 2 |
| std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::replace(unsigned long, unsigned long, char const*, unsigned long) | 2 |
| std::__1::chrono::system_clock::now() | 2 |
| std::__1::ios_base::getloc() const | 2 |
| std::__1::random_device::random_device(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&) | 2 |
| std::exception::~exception() | 2 |
| std::__1::__get_classname(char const*, bool) | 1 |
| std::__1::align(unsigned long, unsigned long, void*&, unsigned long&) | 1 |
| std::__1::basic_ostream<char, std::__1::char_traits<char> >::operator<<(unsigned short) | 1 |
| std::__1::basic_ostream<char, std::__1::char_traits<char> >::operator<<(void const*) | 1 |
| std::__1::basic_ostream<char, std::__1::char_traits<char> >::sentry::sentry(std::__1::basic_ostream<char, std::__1::char_traits<char> >&) | 1 |
| std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > std::__1::operator+<char, std::__1::char_traits<char>, std::__1::allocator<char> >(char const*, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&) | 1 |
| std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::append(char const*) | 1 |
| std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::append(char const*, unsigned long) | 1 |
| std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::basic_string(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&, unsigned long, unsigned long, std::__1::allocator<char> const&) | 1 |
| std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::push_back(char) | 1 |
| std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::resize(unsigned long, char) | 1 |
| std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >::~basic_string() | 1 |
| std::__1::ios_base::__set_badbit_and_consider_rethrow() | 1 |
| std::__1::ios_base::clear(unsigned int) | 1 |
| std::__1::locale::has_facet(std::__1::locale::id&) const | 1 |
| std::__1::locale::name() const | 1 |
| std::__1::locale::operator=(std::__1::locale const&) | 1 |
| std::__1::to_string(long) | 1 |
| std::__1::to_string(unsigned long) | 1 |
| std::logic_error::logic_error(std::logic_error const&) | 1 |
| std::logic_error::~logic_error() | 1 |
| std::out_of_range::~out_of_range() | 1 |
| std::terminate() | 1 |

### Namespace `nn::err` (3056 chamadas, 18 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::err::ErrorResultVariant::ErrorResultVariant(nn::Result const&) | 3025 |
| nn::err::ErrorResultVariant::operator nn::err::ErrorCode() const | 5 |
| nn::err::ErrorCode::IsValid() const | 3 |
| nn::err::ErrorResultVariant::ErrorResultVariant() | 3 |
| nn::err::ErrorResultVariant::GetState() const | 3 |
| nn::err::ErrorResultVariant::operator=(nn::err::ErrorCode const&) | 3 |
| nn::err::ErrorResultVariant::operator nn::Result() const | 2 |
| nn::err::ErrorResultVariant::operator=(nn::Result const&) | 2 |
| nn::err::ApplicationErrorArg::ApplicationErrorArg() | 1 |
| nn::err::ApplicationErrorArg::SetApplicationErrorCodeNumber(unsigned int) | 1 |
| nn::err::ApplicationErrorArg::SetDialogMessage(char const*) | 1 |
| nn::err::ApplicationErrorArg::SetFullScreenMessage(char const*) | 1 |
| nn::err::ApplicationErrorArg::SetLanguageCode(nn::settings::LanguageCode const&) | 1 |
| nn::err::NplnContext::Make(void const*, unsigned long) | 1 |
| nn::err::ShowApplicationError(nn::err::ApplicationErrorArg const&) | 1 |
| nn::err::ShowError(nn::Result) | 1 |
| nn::err::ShowError(nn::err::ErrorCode) | 1 |
| nn::err::ShowError(nn::err::ErrorResultVariant const&) | 1 |

### Namespace `nn::async` (2609 chamadas, 41 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::async::Executable::~Executable() | 623 |
| nn::async::detail::SynchronousContinuationList::~SynchronousContinuationList() | 286 |
| nn::async::detail::FutureStateBase::FutureStateBase() | 248 |
| nn::async::detail::GroupWaitHolderOne::~GroupWaitHolderOne() | 204 |
| nn::async::ExecutionResult::ExecutionResult(nn::async::ExecutionResultCode, nn::async::ResumeBlocker*) | 201 |
| nn::async::detail::FiberTaskCoordinatorBase::GetSynchronousContinuationList() | 150 |
| nn::async::detail::AwaitImpl(nn::async::ResumeBlocker*) | 136 |
| nn::async::detail::TaskCoordinatorBase::TaskCoordinatorBase(nn::MemoryResource*, nn::async::detail::FutureStateBase*, nn::async::detail::FutureStateBase*) | 126 |
| nn::async::Executable::Executable() | 75 |
| nn::async::Executable::Execute(nn::async::CancellationToken const&) | 75 |
| nn::async::detail::FiberTaskCoordinatorBase::FiberTaskCoordinatorBase(nn::MemoryResource*, nn::async::detail::FutureStateBase*, nn::async::detail::FutureStateBase*, void*, unsigned long, nn::os::FiberFlag) | 75 |
| nn::async::detail::FiberTaskCoordinatorBase::FinalizeFiber() | 75 |
| nn::async::EnqueueExecutable(nn::async::Executor*, std::__1::shared_ptr<nn::async::Executable>, nn::async::CancellationToken const&, nn::async::ExecutionBlocker) | 45 |
| nn::async::GetCurrentExecutor() | 37 |
| nn::async::detail::ExecuteTaskInline(nn::async::Executable*, bool, nn::async::CancellationToken const&) | 33 |
| nn::async::detail::SynchronousContinuationList::AddContinuation(std::__1::shared_ptr<nn::async::Executable>, bool, nn::async::CancellationToken const&) | 33 |
| nn::async::detail::SynchronousContinuationList::BeginAddingContinuation() | 33 |
| nn::async::detail::SynchronousContinuationList::EndAddingContinuation() | 33 |
| nn::async::CancellationToken::IsCancellationRequested() const | 18 |
| nn::async::CancellationTokenSource::CancellationTokenSource(nn::MemoryResource*) | 16 |
| nn::async::CancellationTokenSource::RequestCancel() | 14 |
| nn::async::detail::SynchronousContinuationList::ExecuteContinuations() | 14 |
| nn::async::CancellationTokenSource::GetToken() const | 12 |
| nn::async::detail::GroupWait::UnlinkGroupWaitHolder(nn::async::detail::GroupWaitHolder*) | 10 |
| nn::async::detail::SynchronousContinuationList::SynchronousContinuationList(nn::MemoryResource*) | 5 |
| nn::async::CancellationObserver::~CancellationObserver() | 4 |
| nn::async::MakeReadyFuture(nn::MemoryResource*) | 4 |
| nn::async::Await(nn::os::TimerEventType*) | 3 |
| nn::async::QueuingExecutor::~QueuingExecutor() | 3 |
| nn::async::UnregisterCancellationObserver(nn::async::CancellationToken const&, nn::async::CancellationObserver*) | 3 |
| nn::async::Await(nn::os::SystemEventType*) | 2 |
| nn::async::QueuingExecutor::ExecuteAll() | 2 |
| nn::async::QueuingExecutor::QueuingExecutor(nn::MemoryResource*) | 2 |
| nn::async::RegisterCancellationObserver(nn::async::CancellationToken const&, nn::async::CancellationObserver*) | 2 |
| nn::async::Await(nn::os::EventType*) | 1 |
| nn::async::DumpAllTaskDebugData() | 1 |
| nn::async::IsAwaitable() | 1 |
| nn::async::QueuingExecutor::LoopExecution() | 1 |
| nn::async::QueuingExecutor::RequestLoopExecutionExit() | 1 |
| nn::async::QueuingExecutor::TryExecuteOne() | 1 |
| nn::async::detail::AwaitAnyImpl(nn::async::ResumeBlocker*) | 1 |

### Namespace `grpc_core` (312 chamadas, 2 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| TLS init function for grpc_core::ExecCtx::exec_ctx_ | 247 |
| TLS init function for grpc_core::ApplicationCallbackExecCtx::callback_exec_ctx_ | 65 |

### Namespace `nn::util` (170 chamadas, 30 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::util::SNPrintf(char*, unsigned long, char const*, ...) | 41 |
| nn::util::BinaryFileHeader::GetFirstBlock() | 36 |
| nn::util::ReferSymbol(void const*) | 21 |
| nn::util::PickOutCharacterFromUtf8String(char*, char const**) | 8 |
| nn::util::BinaryFileHeader::GetFirstBlock() const | 7 |
| nn::util::ConvertCharacterUtf8ToUtf32(unsigned int*, char const*) | 6 |
| nn::util::BinaryBlockHeader::GetNextBlock() const | 5 |
| nn::util::BinaryFileHeader::GetAlignment() const | 5 |
| nn::util::BinaryFileHeader::GetRelocationTable() | 5 |
| nn::util::BinaryFileHeader::IsRelocated() const | 5 |
| nn::util::BinaryBlockHeader::GetBlockSize() const | 4 |
| nn::util::RelocationTable::Relocate() | 4 |
| nn::util::TinyMt::GenerateRandomU32() | 4 |
| nn::util::BinaryFileHeader::IsAlignmentValid() const | 2 |
| nn::util::VSNPrintf(char*, unsigned long, char const*, std::__va_list) | 2 |
| nn::util::BinaryFileHeader::IsEndianReverse() const | 1 |
| nn::util::BinaryFileHeader::IsSignatureValid(long) const | 1 |
| nn::util::BitArray::reset() | 1 |
| nn::util::ConvertStringUtf16NativeToUtf8(char*, int, unsigned short const*) | 1 |
| nn::util::DecompressZlib(void*, unsigned long, void const*, unsigned long, void*, unsigned long) | 1 |
| nn::util::RelocationTable::Unrelocate() | 1 |
| nn::util::TSNPrintf(char*, unsigned long, char const*, ...) | 1 |
| nn::util::TinyMt::Initialize(unsigned int const*, int) | 1 |
| nn::util::detail::IntrusiveDicImpl::EraseImpl(nn::util::IntrusiveDicNode*) | 1 |
| nn::util::detail::IntrusiveDicImpl::FindImpl(nn::util::basic_string_view<char, std::__1::char_traits<char> > const&) const | 1 |
| nn::util::detail::IntrusiveDicImpl::FindIter() const | 1 |
| nn::util::detail::IntrusiveDicImpl::MoveIter(nn::util::IntrusiveDicNode const*, int) | 1 |
| nn::util::pmr::GetDefaultResource() | 1 |
| nn::util::pmr::NewDeleteResource() | 1 |
| nn::util::pmr::SetDefaultResource(nn::MemoryResource*) | 1 |

### Namespace `nn::diag` (156 chamadas, 7 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::diag::detail::AbortImpl(nn::Result const*) | 87 |
| nn::diag::detail::AbortImpl() | 63 |
| nn::diag::detail::NotNullImpl(unsigned long) | 2 |
| nn::diag::GetBacktrace(unsigned long*, int) | 1 |
| nn::diag::InitializeAbortObserverHolder(nn::diag::AbortObserverHolder*, void (*)(nn::diag::AbortInfo const&)) | 1 |
| nn::diag::RegisterAbortObserver(nn::diag::AbortObserverHolder*) | 1 |
| nn::diag::UnregisterAbortObserver(nn::diag::AbortObserverHolder*) | 1 |

### Namespace `nn::fs` (124 chamadas, 32 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::fs::Unmount(char const*) | 15 |
| nn::fs::CloseFile(nn::fs::FileHandle) | 14 |
| nn::fs::FlushFile(nn::fs::FileHandle) | 13 |
| nn::fs::OpenFile(nn::fs::FileHandle*, char const*, int) | 12 |
| nn::fs::GetFileSize(long*, nn::fs::FileHandle) | 11 |
| nn::fs::GetEntryType(nn::fs::DirectoryEntryType*, char const*) | 7 |
| nn::fs::WriteFile(nn::fs::FileHandle, long, void const*, unsigned long, nn::fs::WriteOption const&) | 7 |
| nn::fs::SetFileSize(nn::fs::FileHandle, long) | 6 |
| nn::fs::CreateFile(char const*, long) | 5 |
| nn::fs::ReadFile(unsigned long*, nn::fs::FileHandle, long, void*, unsigned long) | 5 |
| nn::fs::ReadFile(nn::fs::FileHandle, long, void*, unsigned long) | 4 |
| nn::fs::MountSaveData(char const*, nn::account::Uid const&) | 3 |
| nn::fs::CreateDirectory(char const*) | 2 |
| nn::fs::DeleteFile(char const*) | 2 |
| nn::fs::SetCurrentThreadFsContext(nn::fs::FsContext const*) | 2 |
| nn::fs::CloseDirectory(nn::fs::DirectoryHandle) | 1 |
| nn::fs::Commit(char const*) | 1 |
| nn::fs::DeleteDirectoryRecursively(char const*) | 1 |
| nn::fs::EnsureSaveData(nn::account::Uid const&) | 1 |
| nn::fs::GetCurrentThreadFsContext() | 1 |
| nn::fs::IsSaveDataExisting(nn::ApplicationId, nn::account::Uid const&) | 1 |
| nn::fs::IsSaveDataExisting(nn::account::Uid const&) | 1 |
| nn::fs::MountAddOnContent(char const*, int, void*, unsigned long) | 1 |
| nn::fs::MountRom(char const*) | 1 |
| nn::fs::MountRom(char const*, void*, unsigned long) | 1 |
| nn::fs::MountSaveDataReadOnly(char const*, nn::ApplicationId, nn::account::Uid const&) | 1 |
| nn::fs::OpenDirectory(nn::fs::DirectoryHandle*, char const*, int) | 1 |
| nn::fs::QueryMountAddOnContentCacheSize(unsigned long*, int) | 1 |
| nn::fs::QueryMountRomCacheSize(unsigned long*) | 1 |
| nn::fs::ReadDirectory(long*, nn::fs::DirectoryEntry*, nn::fs::DirectoryHandle, long) | 1 |
| nn::fs::SetAllocator(void* (*)(unsigned long), void (*)(void*, unsigned long)) | 1 |
| nn::fs::GetDirectoryEntryCount(long*, nn::fs::DirectoryHandle) | 0 |

### Namespace `nn::hid` (110 chamadas, 53 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::hid::GetNpadControllerColor(nn::hid::NpadControllerColor*, nn::hid::NpadControllerColor*, unsigned int const&) | 11 |
| nn::hid::GetNpadStyleSet(unsigned int const&) | 7 |
| nn::hid::GetNpadControllerColor(nn::hid::NpadControllerColor*, unsigned int const&) | 6 |
| nn::hid::StopSixAxisSensor(nn::hid::SixAxisSensorHandle const&) | 6 |
| nn::hid::StartSixAxisSensor(nn::hid::SixAxisSensorHandle const&) | 5 |
| nn::hid::BindNpadStyleSetUpdateEvent(unsigned int const&, nn::os::SystemEventType*, nn::os::EventClearMode) | 3 |
| nn::hid::GetSixAxisSensorState(nn::hid::SixAxisSensorState*, nn::hid::SixAxisSensorHandle const&) | 3 |
| nn::hid::InitializeNpad() | 3 |
| nn::hid::SetSupportedNpadIdType(unsigned int const*, unsigned long) | 3 |
| nn::hid::SetSupportedNpadStyleSet(nn::util::BitFlagSet<32, nn::hid::NpadStyleTag>) | 3 |
| nn::hid::EnableSixAxisSensorFusion(nn::hid::SixAxisSensorHandle const&, bool) | 2 |
| nn::hid::GetGyroscopeZeroDriftMode(nn::hid::SixAxisSensorHandle const&) | 2 |
| nn::hid::GetNpadJoyAssignment(unsigned int const&) | 2 |
| nn::hid::GetNpadState(nn::hid::NpadFullKeyState*, unsigned int const&) | 2 |
| nn::hid::GetNpadState(nn::hid::NpadHandheldState*, unsigned int const&) | 2 |
| nn::hid::GetNpadState(nn::hid::NpadJoyDualState*, unsigned int const&) | 2 |
| nn::hid::GetNpadState(nn::hid::NpadJoyLeftState*, unsigned int const&) | 2 |
| nn::hid::GetNpadState(nn::hid::NpadJoyRightState*, unsigned int const&) | 2 |
| nn::hid::GetSixAxisSensorFusionParameters(float*, float*, nn::hid::SixAxisSensorHandle const&) | 2 |
| nn::hid::GetSixAxisSensorStates(nn::hid::SixAxisSensorState*, int, nn::hid::SixAxisSensorHandle const&) | 2 |
| nn::hid::IsSixAxisSensorAtRest(nn::hid::SixAxisSensorHandle const&) | 2 |
| nn::hid::IsSixAxisSensorFusionEnabled(nn::hid::SixAxisSensorHandle const&) | 2 |
| nn::hid::SendVibrationValue(nn::hid::VibrationDeviceHandle const&, nn::hid::VibrationValue const&) | 2 |
| nn::hid::SetGyroscopeZeroDriftMode(nn::hid::SixAxisSensorHandle const&, nn::hid::GyroscopeZeroDriftMode const&) | 2 |
| nn::hid::SetNpadHandheldActivationMode(nn::hid::NpadHandheldActivationMode) | 2 |
| nn::hid::SetNpadJoyHoldType(nn::hid::NpadJoyHoldType) | 2 |
| nn::hid::SetSixAxisSensorFusionParameters(nn::hid::SixAxisSensorHandle const&, float, float) | 2 |
| nn::hid::DisconnectNpad(unsigned int const&) | 1 |
| nn::hid::GetNpadCommunicationMode() | 1 |
| nn::hid::GetNpadHandheldActivationMode() | 1 |
| nn::hid::GetNpadJoyHoldType() | 1 |
| nn::hid::GetNpadStates(nn::hid::NpadFullKeyState*, int, unsigned int const&) | 1 |
| nn::hid::GetNpadStates(nn::hid::NpadHandheldState*, int, unsigned int const&) | 1 |
| nn::hid::GetNpadStates(nn::hid::NpadJoyDualState*, int, unsigned int const&) | 1 |
| nn::hid::GetNpadStates(nn::hid::NpadJoyLeftState*, int, unsigned int const&) | 1 |
| nn::hid::GetNpadStates(nn::hid::NpadJoyRightState*, int, unsigned int const&) | 1 |
| nn::hid::GetPlayerLedPattern(unsigned int const&) | 1 |
| nn::hid::GetSixAxisSensorHandles(nn::hid::SixAxisSensorHandle*, int, unsigned int const&, nn::util::BitFlagSet<32, nn::hid::NpadStyleTag>) | 1 |
| nn::hid::GetSupportedNpadStyleSet() | 1 |
| nn::hid::GetVibrationDeviceHandles(nn::hid::VibrationDeviceHandle*, int, unsigned int const&, nn::util::BitFlagSet<32, nn::hid::NpadStyleTag>) | 1 |
| nn::hid::InitializeTouchScreen(int, int) | 1 |
| nn::hid::InitializeVibrationDevice(nn::hid::VibrationDeviceHandle const&) | 1 |
| nn::hid::MergeSingleJoyAsDualJoy(unsigned int const&, unsigned int const&) | 1 |
| nn::hid::SetNpadCommunicationMode(nn::hid::NpadCommunicationMode) | 1 |
| nn::hid::SetNpadJoyAssignmentModeDual(unsigned int const&) | 1 |
| nn::hid::SetNpadJoyAssignmentModeSingle(unsigned int const&) | 1 |
| nn::hid::SetNpadJoyAssignmentModeSingle(unsigned int const&, nn::hid::NpadJoyDeviceType) | 1 |
| nn::hid::SetNpadJoyAssignmentModeSingle(unsigned int*, unsigned int const&, nn::hid::NpadJoyDeviceType) | 1 |
| nn::hid::ShowControllerStrapGuide() | 1 |
| nn::hid::SixAxisSensorState::GetQuaternion(nn::util::neon::Vector4fType*) const | 1 |
| nn::hid::StartLrAssignmentMode() | 1 |
| nn::hid::StopLrAssignmentMode() | 1 |
| nn::hid::SwapNpadAssignment(unsigned int const&, unsigned int const&) | 1 |

### Namespace `nn::ldn` (79 chamadas, 25 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::ldn::GetState() | 14 |
| nn::ldn::GetIpv4Address(nn::ldn::Ipv4Address*, nn::ldn::SubnetMask*) | 9 |
| nn::ldn::AddAcceptFilterEntry(nn::ldn::MacAddress) | 8 |
| nn::ldn::GetNetworkInfo(nn::ldn::NetworkInfo*) | 6 |
| nn::ldn::SetAdvertiseData(void const*, unsigned long) | 5 |
| nn::ldn::MakeBroadcastAddress(nn::ldn::Ipv4Address, nn::ldn::SubnetMask) | 4 |
| nn::ldn::CloseStation() | 3 |
| nn::ldn::Finalize() | 3 |
| nn::ldn::OpenStation() | 3 |
| nn::ldn::SetStationAcceptPolicy(nn::ldn::AcceptPolicy) | 3 |
| nn::ldn::Connect(nn::ldn::NetworkInfo const&, nn::ldn::SecurityConfig const&, nn::ldn::UserConfig const&, int, nn::ldn::ConnectOption) | 2 |
| nn::ldn::ConnectPrivate(nn::ldn::NetworkConfig const&, nn::ldn::SecurityConfig const&, nn::ldn::SecurityParameter const&, nn::ldn::UserConfig const&, int, nn::ldn::ConnectOption) | 2 |
| nn::ldn::CreateNetworkPrivate(nn::ldn::NetworkConfig const&, nn::ldn::SecurityConfig const&, nn::ldn::SecurityParameter const&, nn::ldn::UserConfig const&, int, nn::ldn::AddressEntry const*) | 2 |
| nn::ldn::GetDisconnectReason() | 2 |
| nn::ldn::Initialize() | 2 |
| nn::ldn::OpenAccessPoint() | 2 |
| nn::ldn::AttachStateChangeEvent(nn::os::SystemEventType*) | 1 |
| nn::ldn::CloseAccessPoint() | 1 |
| nn::ldn::CreateNetwork(nn::ldn::NetworkConfig const&, nn::ldn::SecurityConfig const&, nn::ldn::UserConfig const&) | 1 |
| nn::ldn::DestroyNetwork() | 1 |
| nn::ldn::Disconnect() | 1 |
| nn::ldn::GetSecurityParameter(nn::ldn::SecurityParameter*) | 1 |
| nn::ldn::Reject(nn::ldn::Ipv4Address) | 1 |
| nn::ldn::Scan(nn::ldn::NetworkInfo*, int*, int, nn::ldn::ScanFilter const&, int) | 1 |
| nn::ldn::ScanPrivate(nn::ldn::NetworkInfo*, int*, int, nn::ldn::ScanFilter const&, int) | 1 |

### Namespace `nn::fontll` (74 chamadas, 24 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::fontll::ScalableFontEngine::GetError() | 26 |
| nn::fontll::ScalableFontEngine::SetFlags(nn::fontll::ScalableFontEngine::Flags) | 5 |
| nn::fontll::ScalableFontEngine::SetScale(int, int, int, int) | 5 |
| nn::fontll::ScalableFontEngine::ReleasesGlyph(void*) | 4 |
| nn::fontll::ScalableFontEngine::SetBoldWeight(int) | 3 |
| nn::fontll::ScalableFontEngine::SetFont(char*) | 3 |
| nn::fontll::ScalableFontEngine::SetOutlineWidth(unsigned short) | 3 |
| nn::fontll::ScalableFontEngine::AcquireGlyphmap(unsigned int, unsigned short) | 2 |
| nn::fontll::ScalableFontEngine::CheckGlyphExist(unsigned int) | 2 |
| nn::fontll::ScalableFontEngine::Finalize() | 2 |
| nn::fontll::ScalableFontEngine::GetAdvance(short*, short*, int*, int*, unsigned int, unsigned short) | 2 |
| nn::fontll::ScalableFontEngine::GetPointerToWorkBuffer() | 2 |
| nn::fontll::ScalableFontEngine::Initialize(void*, unsigned int) | 2 |
| nn::fontll::ScalableFontEngine::LoadFont(char*, void const*, unsigned int, unsigned int) | 2 |
| nn::fontll::ScalableFontEngine::ScalableFontEngine() | 2 |
| nn::fontll::ScalableFontEngine::AcquireOtfKerning(nn::fontll::OtfKerningTable const*, unsigned int, unsigned int, unsigned int) | 1 |
| nn::fontll::ScalableFontEngine::AcquireOtfKerningFirst(nn::fontll::OtfKerningTable const*, unsigned int, unsigned int) | 1 |
| nn::fontll::ScalableFontEngine::AcquireOtfKerningLast(nn::fontll::OtfKerningTable const*, unsigned int, unsigned int) | 1 |
| nn::fontll::ScalableFontEngine::FinalizeOtfKerningTable(nn::fontll::OtfKerningTable*, void (*)(void*, void*), void*) | 1 |
| nn::fontll::ScalableFontEngine::GetFontMetrics(nn::fontll::Metrics*) | 1 |
| nn::fontll::ScalableFontEngine::GetKerning(int*, int*, unsigned int, unsigned int) | 1 |
| nn::fontll::ScalableFontEngine::InitializeOtfKerningTable(void* (*)(unsigned long, unsigned long, void*), void*, bool) | 1 |
| nn::fontll::ScalableFontEngine::SetAutoHint(bool) | 1 |
| nn::fontll::ScalableFontEngineHelper::Decode(void const*, unsigned int, unsigned long (*)(void*, unsigned long, void const*, unsigned long, void const*, unsigned long, void const*, unsigned long), unsigned char const*) | 1 |

### Namespace `nn::account` (64 chamadas, 25 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::account::AsyncContext::GetResult() | 7 |
| nn::account::AsyncContext::~AsyncContext() | 7 |
| nn::account::AsyncContext::HasDone(bool*) | 6 |
| nn::account::AsyncContext::AsyncContext() | 5 |
| nn::account::AsyncContext::~AsyncContext() | 4 |
| nn::account::GetNetworkServiceAccountId(nn::account::NetworkServiceAccountId*, nn::account::UserHandle const&) | 4 |
| nn::account::GetUserId(nn::account::Uid*, nn::account::UserHandle const&) | 4 |
| nn::account::EnsureNetworkServiceAccountIdTokenCacheAsync(nn::account::AsyncContext*, nn::account::UserHandle const&) | 3 |
| nn::account::Initialize() | 3 |
| nn::account::LoadNetworkServiceAccountIdTokenCache(unsigned long*, char*, unsigned long, nn::account::UserHandle const&) | 3 |
| nn::account::AsyncContext::GetSystemEvent(nn::os::SystemEvent*) | 2 |
| nn::account::AsyncContext::operator=(nn::account::AsyncContext&&) | 2 |
| nn::account::EnsureNetworkServiceAccountAvailable(nn::account::UserHandle const&) | 2 |
| nn::account::AsyncContext::Cancel() | 1 |
| nn::account::AsyncNetworkServiceLicenseInfoContext::AsyncNetworkServiceLicenseInfoContext() | 1 |
| nn::account::AsyncNetworkServiceLicenseInfoContext::AsyncNetworkServiceLicenseInfoContext(nn::account::AsyncNetworkServiceLicenseInfoContext&&) | 1 |
| nn::account::AsyncNetworkServiceLicenseInfoContext::GetNetworkServiceLicenseKind() | 1 |
| nn::account::AsyncNetworkServiceLicenseInfoContext::operator=(nn::account::AsyncNetworkServiceLicenseInfoContext&&) | 1 |
| nn::account::CloseUser(nn::account::UserHandle const&) | 1 |
| nn::account::GetNickname(nn::account::Nickname*, nn::account::Uid const&) | 1 |
| nn::account::GetUserExistence(bool*, nn::account::Uid const&) | 1 |
| nn::account::IsNetworkServiceAccountAvailable(bool*, nn::account::UserHandle const&) | 1 |
| nn::account::LoadNetworkServiceLicenseInfoAsync(nn::account::AsyncNetworkServiceLicenseInfoContext*, nn::account::UserHandle const&) | 1 |
| nn::account::ShowLicenseRequirementsForNetworkService(nn::account::UserHandle const&) | 1 |
| nn::account::TryOpenPreselectedUser(nn::account::UserHandle*) | 1 |

### Namespace `nn::socket` (61 chamadas, 29 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::socket::InetHtonl(unsigned int) | 12 |
| nn::socket::GetLastError() | 9 |
| nn::socket::GetLastErrno() | 4 |
| nn::socket::SetSockOpt(int, nn::socket::Level, nn::socket::Option, void const*, unsigned int) | 4 |
| nn::socket::GetAddrInfo(char const*, char const*, addrinfo const*, addrinfo**) | 3 |
| nn::socket::InetHtons(unsigned short) | 3 |
| nn::socket::InetNtohl(unsigned int) | 3 |
| nn::socket::Close(int) | 2 |
| nn::socket::Bind(int, nn::socket::SockAddr const*, unsigned int) | 1 |
| nn::socket::Bind(int, sockaddr const*, unsigned int) | 1 |
| nn::socket::EventFd(unsigned long, nn::socket::EventFdFlags) | 1 |
| nn::socket::EventFdRead(int, unsigned long*) | 1 |
| nn::socket::EventFdWrite(int, unsigned long) | 1 |
| nn::socket::FreeAddrInfo(addrinfo*) | 1 |
| nn::socket::FreeAddrInfo(nn::socket::AddrInfo*) | 1 |
| nn::socket::GAIStrError(nn::socket::AiErrno) | 1 |
| nn::socket::GetAddrInfo(char const*, char const*, nn::socket::AddrInfo const*, nn::socket::AddrInfo**) | 1 |
| nn::socket::InetNtohs(unsigned short) | 1 |
| nn::socket::InetNtop(int, void const*, char*, unsigned int) | 1 |
| nn::socket::InetPton(int, char const*, void*) | 1 |
| nn::socket::Initialize(nn::socket::Config const&) | 1 |
| nn::socket::Poll(pollfd*, unsigned long, int) | 1 |
| nn::socket::RecvFrom(int, void*, unsigned long, int, sockaddr*, unsigned int*) | 1 |
| nn::socket::RecvFrom(int, void*, unsigned long, nn::socket::MsgFlag, nn::socket::SockAddr*, unsigned int*) | 1 |
| nn::socket::SendTo(int, void const*, unsigned long, int, sockaddr const*, unsigned int) | 1 |
| nn::socket::SendTo(int, void const*, unsigned long, nn::socket::MsgFlag, nn::socket::SockAddr const*, unsigned int) | 1 |
| nn::socket::Shutdown(int, int) | 1 |
| nn::socket::Socket(int, int, int) | 1 |
| nn::socket::Socket(nn::socket::Family, nn::socket::Type, nn::socket::Protocol) | 1 |

### Namespace `nn::time` (50 chamadas, 18 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::time::ToCalendarTime(nn::time::CalendarTime*, nn::time::CalendarAdditionalInfo*, nn::time::PosixTime const&) | 8 |
| nn::time::Finalize() | 6 |
| nn::time::Initialize() | 6 |
| nn::time::IsInitialized() | 5 |
| nn::time::StandardUserSystemClock::GetCurrentTime(nn::time::PosixTime*) | 5 |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::~AsyncEnsureNetworkClockAvailabilityTask() | 3 |
| nn::time::StandardNetworkSystemClock::GetCurrentTime(nn::time::PosixTime*) | 3 |
| nn::time::GetDeviceLocationName(nn::time::LocationName*) | 2 |
| nn::time::LoadTimeZoneRule(nn::time::TimeZoneRule*, nn::time::LocationName const&) | 2 |
| nn::time::ToCalendarTimeInUtc(nn::time::PosixTime const&) | 2 |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::AsyncEnsureNetworkClockAvailabilityTask() | 1 |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::GetFinishEvent() | 1 |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::GetResult() | 1 |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::Initialize(nn::os::EventClearMode) | 1 |
| nn::time::AsyncEnsureNetworkClockAvailabilityTask::StartTask() | 1 |
| nn::time::IsLeapYear(int) | 1 |
| nn::time::IsValidDate(int, int, int) | 1 |
| nn::time::ToPosixTime(int*, nn::time::PosixTime*, int, nn::time::CalendarTime const&, nn::time::TimeZoneRule const&) | 1 |

### Namespace `nn::bcat` (41 chamadas, 22 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::bcat::DeliveryCacheDirectory::~DeliveryCacheDirectory() | 5 |
| nn::bcat::DeliveryCacheFile::~DeliveryCacheFile() | 5 |
| nn::bcat::DeliveryCacheDirectory::Close() | 3 |
| nn::bcat::DeliveryCacheProgress::GetStatus() const | 3 |
| nn::bcat::DeliveryCacheDirectory::DeliveryCacheDirectory() | 2 |
| nn::bcat::DeliveryCacheDirectory::Read(int*, nn::bcat::DeliveryCacheDirectoryEntry*, int) | 2 |
| nn::bcat::DeliveryCacheFile::DeliveryCacheFile() | 2 |
| nn::bcat::DeliveryCacheProgress::GetResult() const | 2 |
| nn::bcat::DeliveryCacheProgress::~DeliveryCacheProgress() | 2 |
| nn::bcat::EnumerateDeliveryCacheDirectory(int*, nn::bcat::DirectoryName*, int) | 2 |
| nn::bcat::MountDeliveryCacheStorage() | 2 |
| nn::bcat::DeliveryCacheDirectory::Open(char const*) | 1 |
| nn::bcat::DeliveryCacheDirectory::Open(nn::bcat::DirectoryName const&) | 1 |
| nn::bcat::DeliveryCacheFile::Close() | 1 |
| nn::bcat::DeliveryCacheFile::GetSize() | 1 |
| nn::bcat::DeliveryCacheFile::Open(char const*) | 1 |
| nn::bcat::DeliveryCacheFile::Read(unsigned long*, long, void*, unsigned long) | 1 |
| nn::bcat::DeliveryCacheProgress::DeliveryCacheProgress() | 1 |
| nn::bcat::DeliveryCacheProgress::Update() | 1 |
| nn::bcat::Initialize() | 1 |
| nn::bcat::RequestSyncDeliveryCache(nn::bcat::DeliveryCacheProgress*) | 1 |
| nn::bcat::UnmountDeliveryCacheStorage() | 1 |

### Namespace `nn::crypto` (39 chamadas, 15 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::crypto::GenerateCryptographicallyRandomBytes(void*, unsigned long) | 13 |
| nn::crypto::detail::Sha256Impl::Update(void const*, unsigned long) | 4 |
| nn::crypto::detail::Sha256Impl::~Sha256Impl() | 4 |
| nn::crypto::detail::BigNum::BigNum() | 2 |
| nn::crypto::detail::BigNum::ReserveStatic(unsigned int*, int) | 2 |
| nn::crypto::detail::BigNum::Set(void const*, unsigned long) | 2 |
| nn::crypto::detail::BigNum::~BigNum() | 2 |
| nn::crypto::detail::Sha256Impl::GetHash(void*, unsigned long) | 2 |
| nn::crypto::detail::Sha256Impl::Initialize() | 2 |
| nn::crypto::detail::BigNum::Cleanse() | 1 |
| nn::crypto::detail::BigNum::ModExp(void*, void const*, nn::crypto::detail::BigNum const&, unsigned long, unsigned int*, unsigned long) const | 1 |
| nn::crypto::detail::Sha1Impl::GetHash(void*, unsigned long) | 1 |
| nn::crypto::detail::Sha1Impl::Initialize() | 1 |
| nn::crypto::detail::Sha1Impl::Update(void const*, unsigned long) | 1 |
| nn::crypto::detail::Sha1Impl::~Sha1Impl() | 1 |

### Namespace `c++` (33 chamadas, 4 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| operator delete[](void*) | 15 |
| operator new(unsigned long, std::nothrow_t const&) | 9 |
| operator new[](unsigned long) | 8 |
| operator delete(void*, std::align_val_t) | 1 |

### Namespace `nn::oe` (32 chamadas, 19 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::oe::SetUserInactivityDetectionTimeExtended(bool) | 6 |
| nn::oe::SetCpuBoostMode(nn::oe::CpuBoostMode) | 4 |
| nn::oe::SetPerformanceConfiguration(nn::oe::PerformanceMode, int) | 3 |
| nn::oe::GetDesiredLanguage() | 2 |
| nn::oe::LeaveExitRequestHandlingSection() | 2 |
| nn::oe::SetFocusHandlingMode(nn::oe::FocusHandlingMode) | 2 |
| nn::oe::DisableRecording() | 1 |
| nn::oe::DisableScreenShot() | 1 |
| nn::oe::EnableRecording() | 1 |
| nn::oe::EnableScreenShot() | 1 |
| nn::oe::EnterExitRequestHandlingSection() | 1 |
| nn::oe::FinishStartupLogo() | 1 |
| nn::oe::GetCurrentFocusState() | 1 |
| nn::oe::GetPerformanceMode() | 1 |
| nn::oe::GetPseudoDeviceId(nn::util::Uuid*) | 1 |
| nn::oe::Initialize() | 1 |
| nn::oe::IsUserInactivityDetectionTimeExtended() | 1 |
| nn::oe::SetPerformanceModeChangedNotificationEnabled(bool) | 1 |
| nn::oe::TryPopNotificationMessage(unsigned int*) | 1 |

### Namespace `nn::audio` (30 chamadas, 20 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::audio::GetAudioOutBufferDataPointer(nn::audio::AudioOutBuffer const*) | 4 |
| nn::audio::GetActiveAudioDeviceChannelCountForOutput() | 3 |
| nn::audio::AcquireAudioDeviceNotificationForOutput(nn::os::SystemEvent*) | 2 |
| nn::audio::AppendAudioOutBuffer(nn::audio::AudioOut*, nn::audio::AudioOutBuffer*) | 2 |
| nn::audio::GetActiveAudioDeviceName(nn::audio::AudioDeviceName*) | 2 |
| nn::audio::GetActiveAudioOutputDeviceName(nn::audio::AudioDeviceName*) | 2 |
| nn::audio::SetAudioOutBufferInfo(nn::audio::AudioOutBuffer*, void*, unsigned long, unsigned long) | 2 |
| nn::audio::CloseAudioOut(nn::audio::AudioOut*) | 1 |
| nn::audio::GetAudioOutBufferCount(nn::audio::AudioOut const*) | 1 |
| nn::audio::GetAudioOutChannelCount(nn::audio::AudioOut const*) | 1 |
| nn::audio::GetAudioOutState(nn::audio::AudioOut const*) | 1 |
| nn::audio::GetReleasedAudioOutBuffer(nn::audio::AudioOut*) | 1 |
| nn::audio::GetRequiredBufferSizeForResampler(int) | 1 |
| nn::audio::GetResamplerOutputSampleCount(nn::audio::ResamplerType const*, int) | 1 |
| nn::audio::InitializeAudioOutParameter(nn::audio::AudioOutParameter*) | 1 |
| nn::audio::InitializeResampler(nn::audio::ResamplerType*, void*, unsigned long, int, int, int) | 1 |
| nn::audio::OpenDefaultAudioOut(nn::audio::AudioOut*, nn::os::SystemEvent*, nn::audio::AudioOutParameter const&) | 1 |
| nn::audio::ProcessResamplerBuffer(nn::audio::ResamplerType*, int*, short*, unsigned long, short const*, int) | 1 |
| nn::audio::StartAudioOut(nn::audio::AudioOut*) | 1 |
| nn::audio::StopAudioOut(nn::audio::AudioOut*) | 1 |

### Namespace `nn::mem` (23 chamadas, 11 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::mem::StandardAllocator::StandardAllocator() | 9 |
| nn::mem::StandardAllocator::Allocate(unsigned long, unsigned long) | 2 |
| nn::mem::StandardAllocator::Free(void*) | 2 |
| nn::mem::StandardAllocator::GetSizeOf(void const*) const | 2 |
| nn::mem::StandardAllocator::Reallocate(void*, unsigned long) | 2 |
| nn::mem::StandardAllocator::Dump() const | 1 |
| nn::mem::StandardAllocator::Finalize() | 1 |
| nn::mem::StandardAllocator::GetAllocatableSize() const | 1 |
| nn::mem::StandardAllocator::GetTotalFreeSize() const | 1 |
| nn::mem::StandardAllocator::Hash() const | 1 |
| nn::mem::StandardAllocator::Initialize(void*, unsigned long, bool) | 1 |

### Namespace `nn::ssl` (22 chamadas, 12 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::ssl::Context::~Context() | 7 |
| nn::ssl::Context::Destroy() | 4 |
| nn::ssl::BuiltInManager::GetBuiltInCertificateBufSize(unsigned int*, nn::ssl::CaCertificateId*, unsigned int) | 2 |
| nn::ssl::BuiltInManager::GetBuiltInCertificates(nn::ssl::BuiltInCertificateInfoBase**, unsigned char*, unsigned int, nn::ssl::CaCertificateId*, unsigned int) | 2 |
| nn::ssl::Context::Create(nn::ssl::Context::SslVersion) | 2 |
| nn::ssl::Initialize() | 2 |
| nn::ssl::Context::Context() | 1 |
| nn::ssl::Finalize() | 1 |
| nn::ssl::GetSslResultFromValue(nn::Result*, char const*, unsigned int) | 1 |
| nn::ssl::Context::ImportClientPki(unsigned long*, char const*, char const*, unsigned int, unsigned int) | 0 |
| nn::ssl::Context::ImportCrl(unsigned long*, char const*, unsigned int) | 0 |
| nn::ssl::Context::ImportServerPki(unsigned long*, char const*, unsigned int, nn::ssl::CertificateFormat) | 0 |

### Namespace `nn::lmem` (18 chamadas, 6 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::lmem::AllocateFromFrameHeap(nn::lmem::detail::HeapHead*, unsigned long, int) | 9 |
| nn::lmem::AllocateFromExpHeap(nn::lmem::detail::HeapHead*, unsigned long) | 3 |
| nn::lmem::FreeToExpHeap(nn::lmem::detail::HeapHead*, void*) | 3 |
| nn::lmem::CreateExpHeap(void*, unsigned long, int) | 1 |
| nn::lmem::CreateFrameHeap(void*, unsigned long, int) | 1 |
| nn::lmem::DestroyFrameHeap(nn::lmem::detail::HeapHead*) | 1 |

### Namespace `nn::settings` (16 chamadas, 2 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::settings::GetFullKeyGenericPadMap(nn::settings::FullKeyGenericPadMap*, nn::settings::FullKeyIndex) | 8 |
| nn::settings::SetFullKeyGenericPadMap(nn::settings::FullKeyGenericPadMap const&, nn::settings::FullKeyIndex) | 8 |

### Namespace `nn::nifm` (15 chamadas, 7 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::nifm::IsNetworkAvailable() | 6 |
| nn::nifm::GetCurrentIpConfigInfo(in_addr*, in_addr*, in_addr*, in_addr*, in_addr*) | 2 |
| nn::nifm::Initialize() | 2 |
| nn::nifm::SubmitNetworkRequestAndWait() | 2 |
| nn::nifm::GetCurrentProxySetting(nn::nifm::ProxySetting*) | 1 |
| nn::nifm::HandleNetworkRequestErrorResult() | 1 |
| nn::nifm::SetLocalNetworkMode(bool) | 1 |

### Namespace `nn::vi` (14 chamadas, 7 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::vi::CloseDisplay(nn::vi::Display*) | 2 |
| nn::vi::CreateLayer(nn::vi::Layer**, nn::vi::Display*) | 2 |
| nn::vi::DestroyLayer(nn::vi::Layer*) | 2 |
| nn::vi::Finalize() | 2 |
| nn::vi::GetNativeWindow(void**, nn::vi::Layer*) | 2 |
| nn::vi::Initialize() | 2 |
| nn::vi::OpenDefaultDisplay(nn::vi::Display**) | 2 |

### Namespace `nn::detail` (12 chamadas, 1 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::detail::UnexpectedDefaultImpl() | 12 |

### Namespace `nn::swkbd` (11 chamadas, 11 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::swkbd::GetRequiredStringBufferSize() | 1 |
| nn::swkbd::GetRequiredTextCheckWorkBufferSize() | 1 |
| nn::swkbd::GetRequiredWorkBufferSize(bool) | 1 |
| nn::swkbd::GetRequiredWorkBufferSize(int) | 1 |
| nn::swkbd::MakePreset(nn::swkbd::KeyboardConfig*, nn::swkbd::Preset) | 1 |
| nn::swkbd::SetHeaderText(nn::swkbd::KeyboardConfig*, char16_t const*) | 1 |
| nn::swkbd::SetInitialText(nn::swkbd::ShowKeyboardArg*, char16_t const*) | 1 |
| nn::swkbd::SetSubText(nn::swkbd::KeyboardConfig*, char16_t const*) | 1 |
| nn::swkbd::SetTextCheckCallback(nn::swkbd::ShowKeyboardArg*, nn::swkbd::TextCheckResult (*)(void*, unsigned long*, nn::swkbd::String*)) | 1 |
| nn::swkbd::SetUserWordList(nn::swkbd::ShowKeyboardArg*, nn::swkbd::UserWord const*, int) | 1 |
| nn::swkbd::ShowKeyboard(nn::swkbd::String*, nn::swkbd::ShowKeyboardArg const&) | 1 |

### Namespace `nn::prepo` (9 chamadas, 9 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::prepo::PlayReport::Add(char const*, char const*) | 1 |
| nn::prepo::PlayReport::Add(char const*, double) | 1 |
| nn::prepo::PlayReport::Add(char const*, long) | 1 |
| nn::prepo::PlayReport::Clear() | 1 |
| nn::prepo::PlayReport::GetCount() const | 1 |
| nn::prepo::PlayReport::PlayReport() | 1 |
| nn::prepo::PlayReport::Save(nn::account::Uid const&) | 1 |
| nn::prepo::PlayReport::SetBuffer(void*, unsigned long) | 1 |
| nn::prepo::PlayReport::SetEventId(char const*) | 1 |

### Namespace `nn::codec` (8 chamadas, 7 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::codec::GetHardwareOpusDecoderWorkBufferSize(int, int, int) | 2 |
| nn::codec::DecodeOpusInterleavedWithHardware(nn::codec::HardwareOpusDecoderType*, unsigned long*, int*, short*, unsigned long, void const*, unsigned long) | 1 |
| nn::codec::FinalizeHardwareOpusDecoder(nn::codec::HardwareOpusDecoderType*) | 1 |
| nn::codec::GetHardwareOpusDecoderChannelCount(nn::codec::HardwareOpusDecoderType const*) | 1 |
| nn::codec::GetHardwareOpusDecoderSampleRate(nn::codec::HardwareOpusDecoderType const*) | 1 |
| nn::codec::InitializeHardwareOpusDecoder(nn::codec::HardwareOpusDecoderType*, int, int, void*, unsigned long, int) | 1 |
| nn::codec::ResetHardwareOpusDecoder(nn::codec::HardwareOpusDecoderType*) | 1 |

### Namespace `nn::pia` (8 chamadas, 4 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::pia::ZSTD_trace_decompress_begin(nn::pia::ZSTD_DCtx_s const*) | 3 |
| nn::pia::ZSTD_trace_decompress_end(unsigned long long, nn::pia::ZSTD_Trace const*) | 3 |
| nn::pia::ZSTD_trace_compress_begin(nn::pia::ZSTD_CCtx_s const*) | 1 |
| nn::pia::ZSTD_trace_compress_end(unsigned long long, nn::pia::ZSTD_Trace const*) | 1 |

### Namespace `nn::web` (8 chamadas, 8 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::web::ShowWebPage(nn::web::WebPageReturnValue*, nn::web::ShowWebPageArg const&) | 1 |
| nn::web::ShowWebPageArg::SetCallbackUrl(char const*) | 1 |
| nn::web::ShowWebPageArg::SetDisplayUrlKind(nn::web::WebDisplayUrlKind) | 1 |
| nn::web::ShowWebPageArg::ShowWebPageArg(char const*) | 1 |
| nn::web::WebPageReturnValue::GetLastUrl() const | 1 |
| nn::web::WebPageReturnValue::GetLastUrlSize() const | 1 |
| nn::web::WebPageReturnValue::GetWebExitReason() const | 1 |
| nn::web::WebPageReturnValue::WebPageReturnValue() | 1 |

### Namespace `void std` (6 chamadas, 3 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| void std::__1::__sort<std::__1::__less<int, int>&, int*>(int*, int*, std::__1::__less<int, int>&) | 4 |
| void std::__1::__sort<std::__1::__less<long, long>&, long*>(long*, long*, std::__1::__less<long, long>&) | 1 |
| void std::__1::__sort<std::__1::__less<unsigned long, unsigned long>&, unsigned long*>(unsigned long*, unsigned long*, std::__1::__less<unsigned long, unsigned long>&) | 1 |

### Namespace `nn::pl` (5 chamadas, 4 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::pl::RequestSharedFontLoad(nn::pl::SharedFontType) | 2 |
| nn::pl::GetSharedFontAddress(nn::pl::SharedFontType) | 1 |
| nn::pl::GetSharedFontLoadState(nn::pl::SharedFontType) | 1 |
| nn::pl::GetSharedFontSize(nn::pl::SharedFontType) | 1 |

### Namespace `nn::aoc` (4 chamadas, 3 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::aoc::ListAddOnContent(int*, int, int) | 2 |
| nn::aoc::CountAddOnContent() | 1 |
| nn::aoc::GetAddOnContentListChangedEvent(nn::os::SystemEvent*) | 1 |

### Namespace `nn::album` (3 chamadas, 3 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::album::Finalize() | 1 |
| nn::album::Initialize() | 1 |
| nn::album::SaveCurrentScreenshot(nn::album::AlbumReportOption) | 1 |

### Namespace `nn::ngc` (3 chamadas, 3 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::ngc::ProfanityFilter::Finalize() | 1 |
| nn::ngc::ProfanityFilter::Initialize(void*, unsigned long) | 1 |
| nn::ngc::ProfanityFilter::ProfanityFilter() | 1 |

### Namespace `nv` (3 chamadas, 3 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nv::FinalizeGraphics() | 1 |
| nv::InitializeGraphics(void*, unsigned long) | 1 |
| nv::SetGraphicsAllocator(void* (*)(unsigned long, unsigned long, void*), void (*)(void*, void*), void* (*)(void*, unsigned long, void*), void*) | 1 |

### Namespace `nn::nsd` (2 chamadas, 2 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::nsd::GetEnvironmentIdentifier(nn::nsd::EnvironmentIdentifier*) | 2 |
| nn::nsd::ResolveEx(nn::nsd::Fqdn*, nn::nsd::Fqdn const&) | 0 |

### Namespace `nvn` (2 chamadas, 1 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nvnBootstrapLoader | 2 |

### Namespace `nn::Result nn` (1 chamadas, 1 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::Result nn::err::UpdateResultWithContext<nn::err::NplnContext>(nn::Result, nn::err::NplnContext const&) | 1 |

### Namespace `nn::ec` (1 chamadas, 1 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::ec::ShowShopAddOnContentList(nn::account::UserHandle const&) | 1 |

### Namespace `nn::result` (1 chamadas, 1 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::result::detail::OnUnhandledResult(nn::Result) | 1 |

### Namespace `nn::ro` (1 chamadas, 1 importações)

| Importação (demangled) | Chamadas |
| :--- | :--- |
| nn::ro::ProtectRelro(void const*, void const*, void const*, void const*, void const*) | 1 |

