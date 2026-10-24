#include "nn/fs.h"
#include "nn/diag.h"
#include <unordered_map>
#include <string>
#include <fstream>
#include <filesystem>
#include <mutex>

namespace nn::fs {

namespace {
    AllocatorFunction g_fs_alloc = nullptr;
    DeallocatorFunction g_fs_dealloc = nullptr;

    std::unordered_map<std::string, std::string> g_mount_points;
    std::mutex g_fs_mutex;

    std::string ResolveVirtualPath(const std::string& path) {
        auto colon_pos = path.find(":/");
        if (colon_pos == std::string::npos) {
            colon_pos = path.find(":");
        }
        if (colon_pos != std::string::npos) {
            std::string mount_name = path.substr(0, colon_pos);
            std::string sub_path = path.substr(colon_pos + 1);
            if (!sub_path.empty() && (sub_path[0] == '/' || sub_path[0] == '\\')) {
                sub_path = sub_path.substr(1);
            }
            std::lock_guard<std::mutex> lock(g_fs_mutex);
            auto it = g_mount_points.find(mount_name);
            if (it != g_mount_points.end()) {
                return (std::filesystem::path(it->second) / sub_path).string();
            }
        }
        return path;
    }
}

void SetAllocator(AllocatorFunction allocFunc, DeallocatorFunction deallocFunc) {
    g_fs_alloc = allocFunc;
    g_fs_dealloc = deallocFunc;
    NN_LOG("[nn::fs] SetAllocator registered alloc=%p, dealloc=%p\n", (void*)allocFunc, (void*)deallocFunc);
}

Result QueryMountRomCacheSize(size_t* outCacheSize) {
    if (!outCacheSize) return Result(1);
    // Return typical RomFS metadata cache size requested during boot (64 KB)
    *outCacheSize = 64 * 1024;
    NN_LOG("[nn::fs] QueryMountRomCacheSize -> %zu bytes\n", *outCacheSize);
    return ResultSuccess;
}

Result MountRom(const char* name, void* cacheBuffer, size_t cacheBufferSize) {
    if (!name) return Result(1);
    std::lock_guard<std::mutex> lock(g_fs_mutex);
    std::string mount_name(name);
    // If not already explicitly pointed to host dir, default to "./romfs" or "./data"
    if (g_mount_points.find(mount_name) == g_mount_points.end()) {
        g_mount_points[mount_name] = "./" + mount_name;
    }
    NN_LOG("[nn::fs] MountRom(\"%s\", buffer=%p, size=%zu) -> Path: %s\n",
           name, cacheBuffer, cacheBufferSize, g_mount_points[mount_name].c_str());
    return ResultSuccess;
}

Result MountSaveDataReadOnly(const char* name, ApplicationId appId, const account::Uid& uid) {
    if (!name) return Result(1);
    std::lock_guard<std::mutex> lock(g_fs_mutex);
    g_mount_points[name] = "./save";
    NN_LOG("[nn::fs] MountSaveDataReadOnly(\"%s\", appId=0x%llx) -> ./save\n", name, (unsigned long long)appId);
    return ResultSuccess;
}

bool IsSaveDataExisting(ApplicationId appId, const account::Uid& uid) {
    return std::filesystem::exists("./save");
}

Result GetEntryType(DirectoryEntryType* outEntryType, const char* path) {
    if (!outEntryType || !path) return Result(1);
    std::string resolved = ResolveVirtualPath(path);
    if (!std::filesystem::exists(resolved)) {
        return Result(2); // Path not found
    }
    if (std::filesystem::is_directory(resolved)) {
        *outEntryType = DirectoryEntryType_Directory;
    } else {
        *outEntryType = DirectoryEntryType_File;
    }
    return ResultSuccess;
}

void Unmount(const char* name) {
    if (!name) return;
    std::lock_guard<std::mutex> lock(g_fs_mutex);
    g_mount_points.erase(name);
    NN_LOG("[nn::fs] Unmount(\"%s\")\n", name);
}

void SetMountDirectory(const char* mountName, const std::string& hostDirectoryPath) {
    if (!mountName) return;
    std::lock_guard<std::mutex> lock(g_fs_mutex);
    g_mount_points[mountName] = hostDirectoryPath;
    NN_LOG("[nn::fs] SetMountDirectory: '%s:' mapped to host path '%s'\n", mountName, hostDirectoryPath.c_str());
}

// File Operations
Result OpenFile(FileHandle* outHandle, const char* path, uint32_t openMode) {
    if (!outHandle || !path) return Result(1);
    std::string resolved = ResolveVirtualPath(path);
    auto* stream = new std::ifstream(resolved, std::ios::binary);
    if (!stream->is_open()) {
        delete stream;
        outHandle->handle = nullptr;
        return Result(2); // File not found / open failed
    }
    outHandle->handle = stream;
    return ResultSuccess;
}

Result ReadFile(size_t* outReadSize, FileHandle handle, int64_t offset, void* buffer, size_t size) {
    if (!handle.handle || !buffer) return Result(1);
    auto* stream = static_cast<std::ifstream*>(handle.handle);
    stream->seekg(offset, std::ios::beg);
    stream->read(static_cast<char*>(buffer), size);
    std::streamsize bytes_read = stream->gcount();
    if (outReadSize) *outReadSize = static_cast<size_t>(bytes_read);
    return ResultSuccess;
}

Result GetFileSize(int64_t* outSize, FileHandle handle) {
    if (!handle.handle || !outSize) return Result(1);
    auto* stream = static_cast<std::ifstream*>(handle.handle);
    auto current_pos = stream->tellg();
    stream->seekg(0, std::ios::end);
    *outSize = stream->tellg();
    stream->seekg(current_pos, std::ios::beg);
    return ResultSuccess;
}

void CloseFile(FileHandle handle) {
    if (!handle.handle) return;
    auto* stream = static_cast<std::ifstream*>(handle.handle);
    stream->close();
    delete stream;
}

} // namespace nn::fs
