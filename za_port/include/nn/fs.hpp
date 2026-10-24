#pragma once
#include "types.hpp"
#include <string>
#include <cstdint>
#include <cstddef>

namespace za::vfs {
    class TrpfsArchive;
}

namespace nn::fs {

struct FileHandle {
    void* handle{nullptr};
};

enum OpenMode {
    OpenMode_Read = 1,
    OpenMode_Write = 2,
    OpenMode_AllowAppend = 4
};

enum class EntryType : int32_t {
    Directory = 0,
    File = 1
};

// Funções de Montagem
nn::Result QueryMountRomCacheSize(size_t* out_size);
nn::Result MountRom(const char* name, void* cache_buffer, size_t cache_size);
nn::Result MountRomSimple(const char* name);
void Unmount(const char* name);

// Operações de Arquivo (Mapeadas pela Fase 2 do Codex)
nn::Result OpenFile(FileHandle* out_handle, const char* path, int mode);
nn::Result GetFileSize(int64_t* out_size, FileHandle handle);
nn::Result ReadFile(FileHandle handle, int64_t offset, void* buffer, size_t size);
nn::Result ReadFileWithSize(size_t* out_read_size, FileHandle handle, int64_t offset, void* buffer, size_t size);
void CloseFile(FileHandle handle);
nn::Result GetEntryType(EntryType* out_type, const char* path);

// Utilitário do Host para configurar o diretório base dos assets
void SetLocalDataRoot(const std::string& root_dir);

// Acesso ao arquivo TRPFS montado internamente pelo VFS
za::vfs::TrpfsArchive& GetTrpfsArchive();

} // namespace nn::fs
