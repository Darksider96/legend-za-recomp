#pragma once
#include "types.hpp"
#define LEGEND_ZA_BOOT_TYPES_EXTERNAL_NN_RESULT
#include "boot_types.h"

namespace nn::os {

// Funções de Inicialização e Argumentos
int32_t GetHostArgc();
char** GetHostArgv();

// Eventos Leves
void InitializeLightEvent(LightEventType* event, bool initially_signaled, EventClearMode clear_mode);
void WaitLightEvent(LightEventType* event);
void SignalLightEvent(LightEventType* event);
void ClearLightEvent(LightEventType* event);

// Mutexes
void InitializeMutex(MutexType* mutex, bool is_recursive, int32_t lock_level);
void LockMutex(MutexType* mutex);
void UnlockMutex(MutexType* mutex);

// Reader-Writer Lock
void InitializeReaderWriterLock(ReaderWriterLockType* rwlock);
void AcquireReadLock(ReaderWriterLockType* rwlock);
void ReleaseReadLock(ReaderWriterLockType* rwlock);

} // namespace nn::os
