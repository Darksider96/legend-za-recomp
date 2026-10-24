#pragma once
// Trinity Package File System (data.trpfd + data.trpfs) — Pokémon Legends: Z-A
//
// data.trpfs: "ONEPACK\0" + u64 index_offset -> FlatBuffer { [u64] pack_hashes, [u64] pack_offsets }
// data.trpfd: FlatBuffer {
//     0: [u64]                                  file_hashes (ordenado)
//     1: [string]                               pack_names ("arc/xxx.trpak")
//     2: [{ u64 pack_index; u32 unk; }]         file_info (paralelo a file_hashes)
//     3: [{ u64 pack_size; u64 file_count; }]   pack_info (paralelo a pack_names)
// }
// .trpak (dentro do trpfs): FlatBuffer { 0: [u64] hashes, 1: [File] files }
//     File { 0: u8 unused; 1: i8 compression (-1 = nenhuma, 3 = Oodle); 2: u8 level;
//            3: u64 decompressed_size; 4: [u8] data }

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace za::vfs {

// FNV-1a 64 bits com a base de offset da Game Freak (a base padrao e 0xCBF29CE484222325)
inline constexpr uint64_t kFnvOffsetBasis = 0xCBF29CE484222645ULL;
inline constexpr uint64_t kFnvPrime       = 0x00000100000001B3ULL;

constexpr uint64_t Fnv1a64(std::string_view text) noexcept {
    uint64_t hash = kFnvOffsetBasis;
    for (char c : text) {
        hash ^= static_cast<uint8_t>(c);
        hash *= kFnvPrime;
    }
    return hash;
}

// Remove o prefixo de montagem ("data:/", "data:", "rom:/"), converte '\' em '/' e tira '/' inicial
std::string NormalizePath(std::string_view path);

// Normaliza o caminho virtual com NormalizePath() e calcula seu Fnv1a64
uint64_t HashPath(std::string_view path);

namespace oodle {

// Carrega oo2core_9_win64.dll (dir do executavel -> ZA_OODLE_DLL -> PATH). Idempotente e thread-safe.
bool Load();
bool IsAvailable();
// Retorna true apenas se exatamente dst_size bytes foram produzidos
bool Decompress(const uint8_t* src, size_t src_size, uint8_t* dst, size_t dst_size);

} // namespace oodle

class TrpfsArchive {
public:
    enum class Compression : int8_t {
        None  = -1,
        Oodle = 3,
    };

    struct Entry {
        uint64_t hash{0};
        uint32_t pack_index{0};
    };

    struct Pack {
        std::string name;
        uint64_t offset{0};      // offset absoluto no data.trpfs (0 = nao resolvido)
        uint64_t size{0};
        uint64_t file_count{0};
    };

    struct FileInfo {
        uint64_t hash{0};
        uint32_t pack_index{0};
        Compression compression{Compression::None};
        uint64_t stored_size{0};   // bytes dentro do pack
        uint64_t size{0};          // bytes finais (descomprimido)
    };

    TrpfsArchive() = default;
    TrpfsArchive(const TrpfsArchive&) = delete;
    TrpfsArchive& operator=(const TrpfsArchive&) = delete;

    bool Mount(const std::string& trpfd_path, const std::string& trpfs_path);
    void Unmount();
    bool IsMounted() const { return mounted_; }

    bool HasFile(uint64_t hash) const;
    bool HasFile(std::string_view path) const { return HasFile(HashPath(path)); }

    bool ReadFile(uint64_t hash, std::vector<uint8_t>& out_buffer);
    bool ReadFile(std::string_view path, std::vector<uint8_t>& out_buffer) {
        return ReadFile(HashPath(path), out_buffer);
    }

    // Metadados do arquivo sem descomprimir (le o pack, que fica no cache)
    bool GetFileInfo(uint64_t hash, FileInfo& out_info);

    size_t GetEntryCount() const { return entries_.size(); }
    size_t GetPackCount() const { return packs_.size(); }
    const std::vector<Entry>& GetEntries() const { return entries_; }
    const std::vector<Pack>& GetPacks() const { return packs_; }
    const Pack* FindPackForFile(uint64_t hash) const;
    bool FindPackIndex(uint64_t hash, uint32_t& out_pack_index) const;

    // Orcamento do cache LRU de packs brutos (padrao 64 MB; 0 desativa o cache)
    void SetPackCacheBudget(size_t bytes);
    size_t GetPackCacheBudget() const { return cache_budget_; }

private:
    using PackData = std::shared_ptr<const std::vector<uint8_t>>;

    struct CachedPack {
        uint32_t index;
        PackData data;
    };

    struct LocatedFile {
        PackData pack;
        size_t data_offset{0};
        FileInfo info;
    };

    const Entry* FindEntry(uint64_t hash) const;
    bool LocateFile(uint64_t hash, LocatedFile& out);
    PackData LoadPack(uint32_t pack_index);
    void TrimCache();

    std::vector<Entry> entries_;   // ordenado por hash -> busca binaria O(log N)
    std::vector<Pack> packs_;
    bool mounted_{false};

    std::mutex io_mutex_;          // protege trpfs_stream_ e o cache
    std::ifstream trpfs_stream_;
    uint64_t trpfs_size_{0};
    std::list<CachedPack> cache_;  // frente = mais recente
    size_t cache_bytes_{0};
    size_t cache_budget_{64u * 1024u * 1024u};
};

} // namespace za::vfs
