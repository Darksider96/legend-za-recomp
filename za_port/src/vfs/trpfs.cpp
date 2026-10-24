#include "vfs/trpfs.hpp"

#include "za/flat_reader.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <unordered_map>

namespace za::vfs {

namespace {

namespace fs_std = std::filesystem;

constexpr char kOnePackMagic[8] = {'O', 'N', 'E', 'P', 'A', 'C', 'K', '\0'};

std::string Hex(uint64_t value) {
    char buffer[19] = {'0', 'x'};
    auto result = std::to_chars(buffer + 2, buffer + sizeof(buffer), value, 16);
    return std::string(buffer, result.ptr);
}

bool ReadWholeFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    std::error_code ec;
    const auto size = fs_std::file_size(path, ec);
    if (ec) return false;
    out.resize(static_cast<size_t>(size));
    return static_cast<bool>(stream.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size)));
}

} // namespace

std::string NormalizePath(std::string_view path) {
    for (std::string_view prefix : {"data:/", "data:", "rom:/", "rom:"}) {
        if (path.starts_with(prefix)) {
            path.remove_prefix(prefix.size());
            break;
        }
    }

    std::string result(path);
    std::replace(result.begin(), result.end(), '\\', '/');
    const size_t first = result.find_first_not_of('/');
    result.erase(0, first == std::string::npos ? result.size() : first);
    return result;
}

uint64_t HashPath(std::string_view path) {
    return Fnv1a64(NormalizePath(path));
}

bool TrpfsArchive::Mount(const std::string& trpfd_path, const std::string& trpfs_path) {
    Unmount();
    const auto start = std::chrono::steady_clock::now();
    std::cout << "[TRPFS] Montando indice: " << trpfd_path << std::endl;

    // --- data.trpfd ---
    std::vector<uint8_t> trpfd;
    if (!ReadWholeFile(trpfd_path, trpfd)) {
        std::cout << "[TRPFS] Erro: nao foi possivel ler " << trpfd_path << std::endl;
        return false;
    }

    FlatReader fd(trpfd);
    FlatReader::Table root;
    FlatReader::Vector file_hashes, pack_names, file_infos, pack_infos;
    if (!fd.Root(root) ||
        !fd.VectorField(root, 0, 8, file_hashes) ||
        !fd.VectorField(root, 1, 4, pack_names) ||
        !fd.VectorField(root, 2, 4, file_infos) ||
        !fd.VectorField(root, 3, 4, pack_infos) ||
        file_hashes.count != file_infos.count ||
        pack_names.count != pack_infos.count) {
        std::cout << "[TRPFS] Erro: estrutura FlatBuffer invalida em " << trpfd_path << std::endl;
        return false;
    }

    std::vector<Pack> packs(pack_names.count);
    for (uint32_t i = 0; i < pack_names.count; ++i) {
        FlatReader::Table info;
        if (!fd.StringElement(pack_names, i, packs[i].name) || !fd.TableElement(pack_infos, i, info)) {
            std::cout << "[TRPFS] Erro: pack_info #" << i << " corrompido" << std::endl;
            return false;
        }
        packs[i].size = fd.Scalar<uint64_t>(info, 0, 0);
        packs[i].file_count = fd.Scalar<uint64_t>(info, 1, 0);
    }

    std::vector<Entry> entries(file_hashes.count);
    for (uint32_t i = 0; i < file_hashes.count; ++i) {
        FlatReader::Table info;
        if (!fd.TableElement(file_infos, i, info)) {
            std::cout << "[TRPFS] Erro: file_info #" << i << " corrompido" << std::endl;
            return false;
        }
        // FlatBuffers omite campos com valor padrao: pack_index ausente == 0
        const uint64_t pack_index = fd.Scalar<uint64_t>(info, 0, 0);
        if (pack_index >= packs.size()) {
            std::cout << "[TRPFS] Erro: file_info #" << i << " aponta para pack inexistente " << pack_index << std::endl;
            return false;
        }
        entries[i] = {fd.U64Element(file_hashes, i), static_cast<uint32_t>(pack_index)};
    }

    const auto by_hash = [](const Entry& a, const Entry& b) { return a.hash < b.hash; };
    if (!std::is_sorted(entries.begin(), entries.end(), by_hash)) {
        std::cout << "[TRPFS] Indice fora de ordem; ordenando " << entries.size() << " entradas" << std::endl;
        std::sort(entries.begin(), entries.end(), by_hash);
    }
    const auto duplicates = std::adjacent_find(entries.begin(), entries.end(),
                                               [](const Entry& a, const Entry& b) { return a.hash == b.hash; });
    if (duplicates != entries.end()) {
        std::cout << "[TRPFS] Aviso: hash duplicado no indice: " << Hex(duplicates->hash) << std::endl;
    }

    // --- data.trpfs ---
    std::ifstream stream(trpfs_path, std::ios::binary);
    std::error_code ec;
    const uint64_t trpfs_size = fs_std::file_size(trpfs_path, ec);
    char magic[8] = {};
    uint64_t index_offset = 0;
    if (!stream || ec ||
        !stream.read(magic, sizeof(magic)) ||
        !stream.read(reinterpret_cast<char*>(&index_offset), sizeof(index_offset)) ||
        std::memcmp(magic, kOnePackMagic, sizeof(magic)) != 0 ||
        index_offset < 16 || index_offset >= trpfs_size) {
        std::cout << "[TRPFS] Erro: cabecalho ONEPACK invalido em " << trpfs_path << std::endl;
        return false;
    }

    std::vector<uint8_t> index(static_cast<size_t>(trpfs_size - index_offset));
    stream.seekg(static_cast<std::streamoff>(index_offset));
    if (!stream.read(reinterpret_cast<char*>(index.data()), static_cast<std::streamsize>(index.size()))) {
        std::cout << "[TRPFS] Erro: falha ao ler indice de packs em " << Hex(index_offset) << std::endl;
        return false;
    }

    FlatReader fs(index);
    FlatReader::Table fs_root;
    FlatReader::Vector pack_hashes, pack_offsets;
    if (!fs.Root(fs_root) ||
        !fs.VectorField(fs_root, 0, 8, pack_hashes) ||
        !fs.VectorField(fs_root, 1, 8, pack_offsets) ||
        pack_hashes.count != pack_offsets.count) {
        std::cout << "[TRPFS] Erro: indice FlatBuffer do trpfs invalido" << std::endl;
        return false;
    }

    std::unordered_map<uint64_t, uint64_t> offset_by_hash;
    offset_by_hash.reserve(pack_hashes.count);
    for (uint32_t i = 0; i < pack_hashes.count; ++i) {
        offset_by_hash.emplace(fs.U64Element(pack_hashes, i), fs.U64Element(pack_offsets, i));
    }

    size_t unresolved = 0;
    for (Pack& pack : packs) {
        const auto it = offset_by_hash.find(Fnv1a64(pack.name));
        if (it == offset_by_hash.end() || it->second < 16 || pack.size > index_offset - std::min(it->second, index_offset)) {
            ++unresolved;
            continue;
        }
        pack.offset = it->second;
    }
    if (unresolved) {
        std::cout << "[TRPFS] Aviso: " << unresolved << " packs sem offset valido no trpfs" << std::endl;
    }

    {
        std::lock_guard lock(io_mutex_);
        trpfs_stream_ = std::move(stream);
        trpfs_size_ = trpfs_size;
    }
    entries_ = std::move(entries);
    packs_ = std::move(packs);
    mounted_ = true;

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    std::cout << "[TRPFS] Indice carregado: " << entries_.size() << " arquivos em " << packs_.size()
              << " packs (" << pack_hashes.count << " no trpfs, indice @ " << Hex(index_offset)
              << ") em " << elapsed.count() << " ms" << std::endl;

    oodle::Load();
    return true;
}

void TrpfsArchive::Unmount() {
    std::lock_guard lock(io_mutex_);
    if (trpfs_stream_.is_open()) trpfs_stream_.close();
    trpfs_stream_.clear();
    trpfs_size_ = 0;
    cache_.clear();
    cache_bytes_ = 0;
    entries_.clear();
    packs_.clear();
    mounted_ = false;
}

const TrpfsArchive::Entry* TrpfsArchive::FindEntry(uint64_t hash) const {
    const auto it = std::lower_bound(entries_.begin(), entries_.end(), hash,
                                     [](const Entry& e, uint64_t h) { return e.hash < h; });
    return (it != entries_.end() && it->hash == hash) ? &*it : nullptr;
}

bool TrpfsArchive::HasFile(uint64_t hash) const {
    return FindEntry(hash) != nullptr;
}

const TrpfsArchive::Pack* TrpfsArchive::FindPackForFile(uint64_t hash) const {
    const Entry* entry = FindEntry(hash);
    return entry ? &packs_[entry->pack_index] : nullptr;
}

bool TrpfsArchive::FindPackIndex(uint64_t hash, uint32_t& out_pack_index) const {
    const Entry* entry = FindEntry(hash);
    if (entry) out_pack_index = entry->pack_index;
    return entry != nullptr;
}

void TrpfsArchive::SetPackCacheBudget(size_t bytes) {
    std::lock_guard lock(io_mutex_);
    cache_budget_ = bytes;
    TrimCache();
}

void TrpfsArchive::TrimCache() {
    while (cache_bytes_ > cache_budget_ && !cache_.empty()) {
        cache_bytes_ -= cache_.back().data->size();
        cache_.pop_back();
    }
}

TrpfsArchive::PackData TrpfsArchive::LoadPack(uint32_t pack_index) {
    const Pack& pack = packs_[pack_index];
    std::lock_guard lock(io_mutex_);

    const auto cached = std::find_if(cache_.begin(), cache_.end(),
                                     [pack_index](const CachedPack& c) { return c.index == pack_index; });
    if (cached != cache_.end()) {
        cache_.splice(cache_.begin(), cache_, cached);
        return cache_.front().data;
    }

    if (pack.offset == 0 || !trpfs_stream_.is_open()) {
        std::cout << "[TRPFS] Erro: pack '" << pack.name << "' sem offset resolvido" << std::endl;
        return nullptr;
    }

    auto data = std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(pack.size));
    trpfs_stream_.clear();
    trpfs_stream_.seekg(static_cast<std::streamoff>(pack.offset));
    if (!trpfs_stream_.read(reinterpret_cast<char*>(data->data()), static_cast<std::streamsize>(data->size()))) {
        std::cout << "[TRPFS] Erro: falha de leitura do pack '" << pack.name << "' @ " << Hex(pack.offset) << std::endl;
        trpfs_stream_.clear();
        return nullptr;
    }

    if (data->size() <= cache_budget_) {
        cache_.push_front({pack_index, data});
        cache_bytes_ += data->size();
        TrimCache();
    }
    return data;
}

bool TrpfsArchive::LocateFile(uint64_t hash, LocatedFile& out) {
    const Entry* entry = FindEntry(hash);
    if (!entry) {
        std::cout << "[TRPFS] " << Hex(hash) << ": hash nao encontrado no indice" << std::endl;
        return false;
    }

    PackData pack_data = LoadPack(entry->pack_index);
    if (!pack_data) return false;
    const std::string& pack_name = packs_[entry->pack_index].name;

    FlatReader pak(*pack_data);
    FlatReader::Table root;
    FlatReader::Vector hashes, files;
    if (!pak.Root(root) || !pak.VectorField(root, 0, 8, hashes) || !pak.VectorField(root, 1, 4, files) ||
        hashes.count != files.count) {
        std::cout << "[TRPFS] Erro: pack '" << pack_name << "' com FlatBuffer invalido" << std::endl;
        return false;
    }

    uint32_t slot = hashes.count;
    for (uint32_t i = 0; i < hashes.count; ++i) {
        if (pak.U64Element(hashes, i) == hash) {
            slot = i;
            break;
        }
    }

    FlatReader::Table file;
    FlatReader::Vector data;
    if (slot == hashes.count || !pak.TableElement(files, slot, file) || !pak.VectorField(file, 4, 1, data)) {
        std::cout << "[TRPFS] Erro: " << Hex(hash) << " ausente ou corrompido no pack '" << pack_name << "'" << std::endl;
        return false;
    }

    const auto compression = static_cast<Compression>(pak.Scalar<int8_t>(file, 1, -1));
    out.info = {hash, entry->pack_index, compression, data.count,
                compression == Compression::None ? data.count : pak.Scalar<uint64_t>(file, 3, 0)};
    out.data_offset = data.data;
    out.pack = std::move(pack_data);
    return true;
}

bool TrpfsArchive::GetFileInfo(uint64_t hash, FileInfo& out_info) {
    LocatedFile located;
    if (!LocateFile(hash, located)) return false;
    out_info = located.info;
    return true;
}

bool TrpfsArchive::ReadFile(uint64_t hash, std::vector<uint8_t>& out_buffer) {
    out_buffer.clear();

    LocatedFile located;
    if (!LocateFile(hash, located)) return false;
    const FileInfo& info = located.info;
    const uint8_t* src = located.pack->data() + located.data_offset;

    switch (info.compression) {
    case Compression::None:
        out_buffer.assign(src, src + info.stored_size);
        return true;

    case Compression::Oodle:
        if (!oodle::IsAvailable()) {
            std::cout << "[TRPFS] ReadFile(" << Hex(hash) << "): arquivo comprimido com Oodle, descompressor indisponivel" << std::endl;
            return false;
        }
        if (info.size == 0) {
            std::cout << "[TRPFS] Erro: " << Hex(hash) << " sem tamanho descomprimido" << std::endl;
            return false;
        }
        out_buffer.resize(static_cast<size_t>(info.size));
        if (!oodle::Decompress(src, static_cast<size_t>(info.stored_size), out_buffer.data(), out_buffer.size())) {
            std::cout << "[TRPFS] Erro: falha Oodle em " << Hex(hash) << " (" << info.stored_size << " -> "
                      << info.size << " bytes)" << std::endl;
            out_buffer.clear();
            return false;
        }
        return true;
    }

    std::cout << "[TRPFS] Erro: compressao desconhecida " << static_cast<int>(info.compression) << " em " << Hex(hash) << std::endl;
    return false;
}

} // namespace za::vfs
