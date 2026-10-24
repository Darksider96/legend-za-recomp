#include "vfs/asset_export.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace za::vfs {

namespace {

namespace fs_std = std::filesystem;

struct MagicRule {
    const char* magic;
    size_t length;
    std::string_view extension;
};

// Formatos com assinatura; os FlatBuffers da Trinity (trmdl, trmsh, tbl...) nao tem magic
constexpr MagicRule kMagicRules[] = {
    {"BNTX", 4, ".bntx"},
    {"BNSH", 4, ".bnsh"},
    {"FRES", 4, ".bfres"},
    {"SARC", 4, ".sarc"},
    {"Yaz0", 4, ".yaz0"},
    {"AHTB", 4, ".ahtb"},
    {"BKHD", 4, ".bnk"},
    {"RIFF", 4, ".wem"},
    {"\x89PNG", 4, ".png"},
};

std::string Hex16(uint64_t value) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string text(16, '0');
    for (int i = 15; i >= 0; --i, value >>= 4) text[static_cast<size_t>(i)] = kDigits[value & 0xF];
    return text;
}

// "arc/foo_bar.bntx.trpak" -> "foo_bar.bntx"
std::string_view PackStem(std::string_view pack_name) {
    if (const size_t slash = pack_name.rfind('/'); slash != std::string_view::npos) pack_name.remove_prefix(slash + 1);
    if (pack_name.ends_with(".trpak")) pack_name.remove_suffix(6);
    return pack_name;
}

bool WriteAsset(uint64_t hash, const std::vector<uint8_t>& data, const std::string& out_dir,
                const std::string& relative_name, std::string* out_path) {
    fs_std::path target = fs_std::path(out_dir) / fs_std::path(relative_name);
    target.make_preferred();
    std::error_code ec;
    fs_std::create_directories(target.parent_path(), ec);
    if (ec) {
        std::cout << "[TRPFS] Erro: nao foi possivel criar " << target.parent_path().string() << ": " << ec.message() << std::endl;
        return false;
    }

    std::ofstream stream(target, std::ios::binary | std::ios::trunc);
    if (!stream || !stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()))) {
        std::cout << "[TRPFS] Erro: falha ao gravar " << target.string() << std::endl;
        return false;
    }

    std::cout << "[TRPFS] Exportado 0x" << Hex16(hash) << " -> " << target.string() << " (" << data.size() << " bytes)" << std::endl;
    if (out_path) *out_path = target.string();
    return true;
}

} // namespace

bool IsSafeRelativePath(std::string_view path) {
    if (path.empty() || path.front() == '/' || path.find_first_of(":\\") != std::string_view::npos) return false;
    while (!path.empty()) {
        const size_t slash = path.find('/');
        const std::string_view part = path.substr(0, slash);
        if (part.empty() || part == "." || part == "..") return false;
        if (slash == std::string_view::npos) break;
        path.remove_prefix(slash + 1);
    }
    return true;
}

std::string ResolveRelativePath(std::string_view base_dir, std::string_view relative) {
    std::vector<std::string_view> parts;
    const auto append = [&parts](std::string_view path) {
        while (!path.empty()) {
            const size_t slash = path.find('/');
            const std::string_view part = path.substr(0, slash);
            if (part == "..") {
                if (!parts.empty()) parts.pop_back();
            } else if (!part.empty() && part != ".") {
                parts.push_back(part);
            }
            if (slash == std::string_view::npos) break;
            path.remove_prefix(slash + 1);
        }
    };
    append(base_dir);
    append(relative);

    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) result.push_back('/');
        result.append(parts[i]);
    }
    return result;
}

bool PathDictionary::LoadFromFile(const std::string& path) {
    std::ifstream stream(path);
    if (!stream) {
        std::cout << "[TRPFS] Erro: dicionario de caminhos nao encontrado: " << path << std::endl;
        return false;
    }

    const size_t before = paths_.size();
    std::string line;
    while (std::getline(stream, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line.front() == '#') continue;
        Add(line);
    }
    std::cout << "[TRPFS] Dicionario carregado: " << (paths_.size() - before) << " caminhos de " << path << std::endl;
    return true;
}

bool PathDictionary::SaveToFile(const std::string& path, std::string_view header) const {
    std::vector<std::string_view> sorted;
    sorted.reserve(paths_.size());
    for (const auto& [hash, virtual_path] : paths_) sorted.push_back(virtual_path);
    std::sort(sorted.begin(), sorted.end());

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        std::cout << "[TRPFS] Erro: nao foi possivel gravar o dicionario em " << path << std::endl;
        return false;
    }
    stream << header;
    for (std::string_view virtual_path : sorted) stream << virtual_path << '\n';
    if (!stream) return false;

    std::cout << "[TRPFS] Dicionario gravado: " << sorted.size() << " caminhos em " << path << std::endl;
    return true;
}

void PathDictionary::Add(std::string_view virtual_path) {
    std::string normalized = NormalizePath(virtual_path);
    const uint64_t hash = Fnv1a64(normalized);
    paths_.insert_or_assign(hash, std::move(normalized));
}

const std::string* PathDictionary::Find(uint64_t hash) const {
    const auto it = paths_.find(hash);
    return it != paths_.end() ? &it->second : nullptr;
}

std::string_view DetectExtension(std::span<const uint8_t> data) {
    for (const MagicRule& rule : kMagicRules) {
        if (data.size() >= rule.length && std::memcmp(data.data(), rule.magic, rule.length) == 0) return rule.extension;
    }
    return {};
}

std::string BuildExportName(const TrpfsArchive& archive, uint64_t hash,
                            std::span<const uint8_t> data, const PathDictionary* names) {
    if (names) {
        if (const std::string* known = names->Find(hash); known && IsSafeRelativePath(*known)) return *known;
    }

    const TrpfsArchive::Pack* pack = archive.FindPackForFile(hash);
    const std::string_view stem = pack ? PackStem(pack->name) : std::string_view("unknown_pack");

    std::string_view extension = DetectExtension(data);
    if (extension.empty()) {
        const size_t dot = stem.rfind('.');
        extension = dot != std::string_view::npos ? stem.substr(dot) : std::string_view(".bin");
        // O tipo do pack vale para o arquivo principal; sem o magic esperado o conteudo e outro
        for (const MagicRule& rule : kMagicRules) {
            if (rule.extension == extension) extension = ".bin";
        }
    }

    std::string name;
    name.reserve(stem.size() + 1 + 16 + extension.size());
    name.append(stem).append("/").append(Hex16(hash)).append(extension);
    return IsSafeRelativePath(name) ? name : Hex16(hash) + std::string(extension);
}

bool ExportAsset(TrpfsArchive& archive, uint64_t hash, const std::string& out_dir,
                 const PathDictionary* names, std::string* out_path) {
    std::vector<uint8_t> data;
    if (!archive.ReadFile(hash, data)) return false;
    return WriteAsset(hash, data, out_dir, BuildExportName(archive, hash, data, names), out_path);
}

bool ExportAsset(TrpfsArchive& archive, std::string_view virtual_path, const std::string& out_dir,
                 std::string* out_path) {
    const std::string normalized = NormalizePath(virtual_path);
    const uint64_t hash = Fnv1a64(normalized);

    std::vector<uint8_t> data;
    if (!archive.ReadFile(hash, data)) return false;

    const std::string name = IsSafeRelativePath(normalized) ? normalized : BuildExportName(archive, hash, data);
    return WriteAsset(hash, data, out_dir, name, out_path);
}

} // namespace za::vfs
