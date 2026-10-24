#pragma once
// Exportacao de arquivos do TRPFS para o disco local (inspecao de modelos, texturas e shaders)

#include "vfs/trpfs.hpp"

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

namespace za::vfs {

// O TRPFS guarda apenas hashes; o dicionario devolve o caminho virtual quando ele e conhecido
class PathDictionary {
public:
    // Um caminho por linha; linhas vazias ou iniciadas com '#' sao ignoradas
    bool LoadFromFile(const std::string& path);
    // Grava os caminhos ordenados, um por linha, precedidos de header (linhas '#')
    bool SaveToFile(const std::string& path, std::string_view header = {}) const;
    void Add(std::string_view virtual_path);
    const std::string* Find(uint64_t hash) const;
    size_t Size() const { return paths_.size(); }
    const std::unordered_map<uint64_t, std::string>& Entries() const { return paths_; }

private:
    std::unordered_map<uint64_t, std::string> paths_;
};

// Extensao (com '.') deduzida do magic do conteudo; vazia se nao reconhecido
std::string_view DetectExtension(std::span<const uint8_t> data);

// Caminho relativo que nao escapa do diretorio de saida (sem "..", '\', ':' ou '/' inicial)
bool IsSafeRelativePath(std::string_view path);

// Resolve um caminho relativo (com "." e "..") a partir de um diretorio virtual:
// ("ik_pokemon/data/pm0150/pm0150_00_00", "../../../share/x.bntx") -> "ik_pokemon/share/x.bntx"
std::string ResolveRelativePath(std::string_view base_dir, std::string_view relative);

// Caminho relativo de saida: caminho do dicionario, senao "<pack>/<hash>.<ext>"
// (ext: magic do conteudo -> tipo do pack "xxx.bntx.trpak" -> ".bin")
std::string BuildExportName(const TrpfsArchive& archive, uint64_t hash,
                            std::span<const uint8_t> data, const PathDictionary* names = nullptr);

// Le o arquivo do pacote e grava em out_dir/<BuildExportName>. out_path recebe o caminho gravado.
bool ExportAsset(TrpfsArchive& archive, uint64_t hash, const std::string& out_dir,
                 const PathDictionary* names = nullptr, std::string* out_path = nullptr);

// Variante por caminho virtual: grava em out_dir/<NormalizePath(virtual_path)>
bool ExportAsset(TrpfsArchive& archive, std::string_view virtual_path, const std::string& out_dir,
                 std::string* out_path = nullptr);

} // namespace za::vfs
