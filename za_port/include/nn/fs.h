#pragma once

#include "nn/types.h"
#include <cstdint>
#include <cstddef>

namespace nn::fs {

enum DirectoryEntryType {
    DirectoryEntryType_Directory = 0,
    DirectoryEntryType_File      = 1
};

struct FileHandle {
    void* handle{nullptr};
};

using AllocatorFunction = void* (*)(size_t size);
using DeallocatorFunction = void (*)(void* ptr);

void SetAllocator(AllocatorFunction allocFunc, DeallocatorFunction deallocFunc);

Result QueryMountRomCacheSize(size_t* outCacheSize);
Result MountRom(const char* name, void* cacheBuffer, size_t cacheBufferSize);

Result MountSaveDataReadOnly(const char* name, ApplicationId appId, const account::Uid& uid);
bool IsSaveDataExisting(ApplicationId appId, const account::Uid& uid);

Result GetEntryType(DirectoryEntryType* outEntryType, const char* path);
void Unmount(const char* name);

// File I/O
Result OpenFile(FileHandle* outHandle, const char* path, uint32_t openMode);
Result ReadFile(size_t* outReadSize, FileHandle handle, int64_t offset, void* buffer, size_t size);
Result GetFileSize(int64_t* outSize, FileHandle handle);
void CloseFile(FileHandle handle);

// Port helper for setting local filesystem root for mounted archives (e.g., "data")
void SetMountDirectory(const char* mountName, const std::string& hostDirectoryPath);

} // namespace nn::fs
