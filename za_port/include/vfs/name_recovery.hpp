#pragma once
// Reconstrucao do dicionario de caminhos do TRPFS (todo candidato e validado contra os hashes do data.trpfd)
//
// 1. strings de binarios (ex.: rodata.bin): caminhos completos, relativos (+ diretorios conhecidos) e raizes "dir/"
// 2. nomes de pack: "arc/" + caminho sem '/' + ".trpak" -> reinsere as barras com busca guiada pelo vocabulario
//    de diretorios conhecidos, permitindo ate N barras "livres" fora do vocabulario
// 3. irmaos: os demais arquivos do pack ficam no mesmo diretorio, com radical e extensao aparentados
// 4. variacoes numericas: series como ".../0744_01_00.bin" -> cada sequencia de 1 a 4 digitos do nome
// 5. conteudo (opcional): strings internas dos arquivos descomprimidos (malhas, materiais, texturas...),
//    testadas no diretorio do proprio pack e depois contra todos os diretorios conhecidos

#include "vfs/asset_export.hpp"

#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace za::vfs {

struct NameRecoveryOptions {
    int max_free_slashes{4};   // barras por nome de pack fora do vocabulario de diretorios
    unsigned threads{0};       // 0 = std::thread::hardware_concurrency()
    bool siblings{true};
    bool numeric{true};
    bool mine_content{false};  // descomprime todos os arquivos (le o data.trpfs inteiro)
};

struct NameRecoveryStats {
    size_t seed{0};
    size_t from_strings{0};
    size_t from_packs{0};
    size_t from_siblings{0};
    size_t from_numeric{0};
    size_t from_content{0};
    size_t packs_resolved{0};
    size_t total{0};
    double seconds{0};
};

class NameRecovery {
public:
    // Copia do dicionario semente apenas os caminhos que existem no pacote
    NameRecovery(TrpfsArchive& archive, const PathDictionary& seed);

    // Extrai strings com cara de caminho ("a/b.ext", raizes "dir/") de um binario
    void AddSeedBinary(std::span<const uint8_t> data);

    NameRecoveryStats Run(const NameRecoveryOptions& options);

    // Reinsere as barras no nome de um pack sem "arc/" e ".trpak"; nullopt se nenhum candidato existe no pacote
    std::optional<std::string> ResolvePackStem(std::string_view stem, int max_free_slashes) const;

    const PathDictionary& Result() const { return result_; }

private:
    bool Contains(uint64_t hash) const;
    bool Learn(std::string_view path, size_t& counter);
    void AddDirectory(std::string_view dir);
    void AddSeedString(std::string_view text);
    static bool HasPlausibleExtension(std::string_view text);

    size_t RecoverFromStrings(unsigned threads);
    size_t RecoverFromPackNames(int max_free_slashes, unsigned threads);
    size_t RecoverSiblings(unsigned threads);
    size_t RecoverNumericVariants(unsigned threads);
    size_t RecoverFromContent(unsigned threads);

    TrpfsArchive& archive_;
    PathDictionary result_;
    NameRecoveryStats stats_;

    std::vector<uint64_t> filter_;                      // bitset de pre-filtro dos hashes do indice
    std::unordered_set<uint64_t> vocabulary_;           // Fnv1a64 de componentes de diretorio
    std::vector<std::string> directories_;              // ordem de descoberta
    std::vector<uint64_t> directory_states_;            // Fnv1a64(diretorio), para concatenacao incremental
    std::unordered_set<std::string> directory_set_;
    std::set<std::string, std::less<>> extensions_;     // "trmdl", "bntx", ...

    std::vector<std::string> strings_;                  // sementes de binarios
    std::unordered_set<std::string> string_set_;
    size_t tested_strings_{0};
    size_t tested_directories_{0};

    std::vector<std::string> learned_order_;            // caminhos na ordem em que foram aprendidos
    size_t numeric_cursor_{0};
    std::vector<uint32_t> named_per_pack_;

    std::unordered_map<std::string, uint32_t> pack_by_stem_;
    std::vector<uint8_t> pack_resolved_;
    size_t vocabulary_at_last_pack_pass_{~size_t{0}};
    size_t names_at_last_sibling_pass_{~size_t{0}};
    bool content_mined_{false};
};

} // namespace za::vfs
