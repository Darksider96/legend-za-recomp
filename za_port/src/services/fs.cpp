#include "nn/fs.hpp"
#include "vfs/trpfs.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <memory>
#include <cstring>
#include <vector>
#include <algorithm>

namespace nn::fs {

namespace fs_std = std::filesystem;

static std::string g_local_data_root = "data";
static za::vfs::TrpfsArchive g_trpfs_archive;

void SetLocalDataRoot(const std::string& root_dir) {
    g_local_data_root = root_dir;
    std::cout << "[nn::fs] Diretorio raiz de dados definido para: " << g_local_data_root << std::endl;
}

za::vfs::TrpfsArchive& GetTrpfsArchive() {
    return g_trpfs_archive;
}

// Converte caminhos do Switch ("data:/arc/data.trpfs") para o caminho local do PC ("./data/arc/data.trpfs")
static std::string ResolveGuestPath(const char* guest_path) {
    if (!guest_path) return "";
    std::string path(guest_path);

    // Remove prefixos conhecidos de montagem do Switch
    if (path.rfind("data:/", 0) == 0) {
        path = path.substr(6);
    } else if (path.rfind("data:", 0) == 0) {
        path = path.substr(5);
    } else if (path.rfind("rom:/", 0) == 0) {
        path = path.substr(5);
    }

    fs_std::path local_path = fs_std::path(g_local_data_root) / path;
    return local_path.string();
}

struct HostFileContext {
    std::string path;
    std::ifstream stream;
    std::vector<uint8_t> memory_data;
    bool is_memory{false};
    int64_t file_size{0};
};

static uint64_t g_next_handle = 1;
static std::unordered_map<void*, std::unique_ptr<HostFileContext>> g_open_files;

nn::Result QueryMountRomCacheSize(size_t* out_size) {
    if (out_size) {
        *out_size = 64 * 1024;
    }
    std::cout << "[nn::fs] QueryMountRomCacheSize() -> 64 KB" << std::endl;
    return nn::ResultSuccess;
}

nn::Result MountRom(const char* name, void* cache_buffer, size_t cache_size) {
    (void)cache_buffer;
    std::cout << "[nn::fs] MountRom(mount_point=\"" << (name ? name : "null") 
              << "\", cache_size=" << cache_size << " bytes)" << std::endl;
    std::cout << "[nn::fs] -> Redirecionando '" 
              << (name ? name : "data") << ":/' para pasta local: " << g_local_data_root << std::endl;

    // Se o TRPFS ainda não foi montado, tenta montar automaticamente
    if (!g_trpfs_archive.IsMounted()) {
        const std::vector<std::pair<std::string, std::string>> candidates = {
            { g_local_data_root + "/arc/data.trpfd", g_local_data_root + "/arc/data.trpfs" },
            { g_local_data_root + "/data.trpfd", g_local_data_root + "/data.trpfs" },
            { "romfs/arc/data.trpfd", "romfs/arc/data.trpfs" },
            { "../romfs/arc/data.trpfd", "../romfs/arc/data.trpfs" },
            { "C:/Users/johna/OneDrive/Documentos/legend za/romfs/arc/data.trpfd", 
              "C:/Users/johna/OneDrive/Documentos/legend za/romfs/arc/data.trpfs" }
        };

        for (const auto& [trpfd_cand, trpfs_cand] : candidates) {
            if (fs_std::exists(trpfd_cand) && fs_std::exists(trpfs_cand)) {
                std::cout << "[nn::fs] Localizado arquivo TRPFS RomFS em: " << trpfd_cand << std::endl;
                if (g_trpfs_archive.Mount(trpfd_cand, trpfs_cand)) {
                    std::cout << "[nn::fs] [SUCESSO] TRPFS montado automaticamente: "
                              << g_trpfs_archive.GetEntryCount() << " arquivos em " 
                              << g_trpfs_archive.GetPackCount() << " pacotes!" << std::endl;
                    break;
                }
            }
        }
    }

    return nn::ResultSuccess;
}

nn::Result MountRomSimple(const char* name) {
    return MountRom(name, nullptr, 0);
}

void Unmount(const char* name) {
    std::cout << "[nn::fs] Unmount(\"" << (name ? name : "null") << "\")" << std::endl;
    if (g_trpfs_archive.IsMounted()) {
        g_trpfs_archive.Unmount();
    }
}

nn::Result OpenFile(FileHandle* out_handle, const char* path, int mode) {
    if (!out_handle || !path) {
        return 1; // ErrorInvalidPath
    }

    std::string local_path = ResolveGuestPath(path);
    std::cout << "[nn::fs] OpenFile(\"" << path << "\", mode=" << mode << ")" << std::endl;

    auto ctx = std::make_unique<HostFileContext>();
    ctx->path = local_path;

    // 1. Tenta abrir arquivo real descompactado no disco
    if (fs_std::exists(local_path) && !fs_std::is_directory(local_path)) {
        ctx->stream.open(local_path, std::ios::binary);
        if (ctx->stream.is_open()) {
            ctx->file_size = static_cast<int64_t>(fs_std::file_size(local_path));
            ctx->is_memory = false;
            std::cout << "         [OK-DISCO] Arquivo real aberto (" << ctx->file_size << " bytes)" << std::endl;

            void* handle_ptr = reinterpret_cast<void*>(g_next_handle++);
            out_handle->handle = handle_ptr;
            g_open_files[handle_ptr] = std::move(ctx);
            return nn::ResultSuccess;
        }
    }

    // 2. Se não está no disco, busca no arquivo TRPFS montado
    if (g_trpfs_archive.IsMounted()) {
        std::string norm = za::vfs::NormalizePath(path);
        uint64_t found_hash = 0;
        std::string matched_vpath;

        if (g_trpfs_archive.HasFile(norm)) {
            found_hash = za::vfs::HashPath(norm);
            matched_vpath = norm;
        } else {
            // Tenta prefixos comuns da engine Trinity
            const char* prefixes[] = {
                "system_resource/",
                "world/ik_data/",
                "world/",
                "field/",
                "arc/",
                "system/"
            };
            for (const char* pfx : prefixes) {
                std::string cand = std::string(pfx) + norm;
                if (g_trpfs_archive.HasFile(cand)) {
                    found_hash = za::vfs::HashPath(cand);
                    matched_vpath = cand;
                    break;
                }
            }
        }

        if (found_hash != 0) {
            if (g_trpfs_archive.ReadFile(found_hash, ctx->memory_data)) {
                ctx->file_size = static_cast<int64_t>(ctx->memory_data.size());
                ctx->is_memory = true;
                ctx->path = matched_vpath;
                std::cout << "         [OK-TRPFS] Carregado da RomFS VFS: '" << matched_vpath 
                          << "' (Hash: 0x" << std::hex << found_hash << std::dec 
                          << ", " << ctx->file_size << " bytes)" << std::endl;

                void* handle_ptr = reinterpret_cast<void*>(g_next_handle++);
                out_handle->handle = handle_ptr;
                g_open_files[handle_ptr] = std::move(ctx);
                return nn::ResultSuccess;
            }
        }
    }

    // 3. Fallback: handle simulado com tamanho seguro para evitar falha catastrófica de boot
    std::cout << "         [STUB] Arquivo nao encontrado fisicamente nem no TRPFS. Criando handle simulado." << std::endl;
    ctx->file_size = 1024;
    ctx->is_memory = true;
    ctx->memory_data.resize(1024, 0);

    void* handle_ptr = reinterpret_cast<void*>(g_next_handle++);
    out_handle->handle = handle_ptr;
    g_open_files[handle_ptr] = std::move(ctx);

    return nn::ResultSuccess;
}

nn::Result GetFileSize(int64_t* out_size, FileHandle handle) {
    if (!out_size) return 1;

    auto it = g_open_files.find(handle.handle);
    if (it != g_open_files.end()) {
        *out_size = it->second->file_size;
        return nn::ResultSuccess;
    }

    *out_size = 0;
    return 1; // ErrorHandleNotFound
}

nn::Result ReadFile(FileHandle handle, int64_t offset, void* buffer, size_t size) {
    if (!buffer) return 1;

    auto it = g_open_files.find(handle.handle);
    if (it != g_open_files.end()) {
        auto& ctx = it->second;
        if (ctx->is_memory) {
            if (offset < 0 || static_cast<size_t>(offset) >= ctx->memory_data.size()) {
                std::memset(buffer, 0, size);
                return nn::ResultSuccess;
            }
            size_t available = ctx->memory_data.size() - static_cast<size_t>(offset);
            size_t copy_size = std::min(size, available);
            std::memcpy(buffer, ctx->memory_data.data() + offset, copy_size);
            if (copy_size < size) {
                std::memset(reinterpret_cast<uint8_t*>(buffer) + copy_size, 0, size - copy_size);
            }
            return nn::ResultSuccess;
        } else if (ctx->stream.is_open()) {
            ctx->stream.seekg(offset);
            ctx->stream.read(reinterpret_cast<char*>(buffer), size);
            return nn::ResultSuccess;
        } else {
            std::memset(buffer, 0, size);
            return nn::ResultSuccess;
        }
    }

    return 1; // ErrorHandleNotFound
}

nn::Result ReadFileWithSize(size_t* out_read_size, FileHandle handle, int64_t offset, void* buffer, size_t size) {
    nn::Result res = ReadFile(handle, offset, buffer, size);
    if (res == nn::ResultSuccess && out_read_size) {
        auto it = g_open_files.find(handle.handle);
        if (it != g_open_files.end()) {
            int64_t fsize = it->second->file_size;
            if (offset >= fsize) {
                *out_read_size = 0;
            } else {
                *out_read_size = std::min(size, static_cast<size_t>(fsize - offset));
            }
        } else {
            *out_read_size = size;
        }
    }
    return res;
}

void CloseFile(FileHandle handle) {
    auto it = g_open_files.find(handle.handle);
    if (it != g_open_files.end()) {
        if (it->second->stream.is_open()) {
            it->second->stream.close();
        }
        it->second->memory_data.clear();
        g_open_files.erase(it);
        std::cout << "[nn::fs] CloseFile(handle=" << handle.handle << ") [Fechado com sucesso]" << std::endl;
    }
}

nn::Result GetEntryType(EntryType* out_type, const char* path) {
    if (!out_type || !path) return 1;

    std::string local_path = ResolveGuestPath(path);
    if (fs_std::exists(local_path)) {
        if (fs_std::is_directory(local_path)) {
            *out_type = EntryType::Directory;
        } else {
            *out_type = EntryType::File;
        }
        return nn::ResultSuccess;
    }

    if (g_trpfs_archive.IsMounted()) {
        std::string norm = za::vfs::NormalizePath(path);
        if (g_trpfs_archive.HasFile(norm)) {
            *out_type = EntryType::File;
            return nn::ResultSuccess;
        }
        const char* prefixes[] = {
            "system_resource/",
            "world/ik_data/",
            "world/",
            "field/",
            "arc/",
            "system/"
        };
        for (const char* pfx : prefixes) {
            if (g_trpfs_archive.HasFile(std::string(pfx) + norm)) {
                *out_type = EntryType::File;
                return nn::ResultSuccess;
            }
        }
    }

    *out_type = EntryType::File; // Padrão seguro para o Trinity Engine
    return nn::ResultSuccess;
}

} // namespace nn::fs
