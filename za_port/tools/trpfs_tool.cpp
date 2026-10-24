// trpfs_tool — inspecao e extracao de arquivos do TRPFS (data.trpfd + data.trpfs)

#include "audio/wem_decoder.hpp"
#include "audio/wwise.hpp"
#include "data/message_text.hpp"
#include "data/personal_table.hpp"
#include "gfx/bntx.hpp"
#include "gfx/ik_character.hpp"
#include "gfx/texture_codec.hpp"
#include "gfx/tranm.hpp"
#include "gfx/scene_assembly.hpp"
#include "gfx/trcol.hpp"
#include "gfx/trmdl.hpp"
#include "gfx/trscn.hpp"
#include "vfs/asset_export.hpp"
#include "vfs/name_recovery.hpp"
#include "za/parallel_for.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

using namespace za::vfs;

namespace {

namespace fs_std = std::filesystem;

struct Options {
    std::string romfs_dir;
    std::string names_path;
    std::string out_dir{"trpfs_export"};
    std::vector<std::string> string_sources;
    int depth{4};
    unsigned lod{0};
    unsigned threads{0};
    std::optional<bool> mine;
    bool dry_run{false};
    bool dds{false};
    bool bake{false};
    bool png{false};
};

// Arquivo carregado do disco ou do pacote
struct Asset {
    std::vector<uint8_t> data;
    std::string label;       // texto para logs
    std::string output_dir;  // subdiretorio de saida (relativo a -o)
    uint64_t hash{0};        // 0 = arquivo do disco
};

void PrintUsage() {
    std::cout <<
        "uso: trpfs_tool [opcoes] <comando> [args...]\n"
        "\n"
        "opcoes:\n"
        "  --romfs <dir>      raiz do RomFS (padrao: ./romfs ou ../romfs)\n"
        "  --names <arq>      dicionario de caminhos (padrao: analysis/recovered_dictionary.txt, se existir)\n"
        "  -o <dir>           diretorio de saida (padrao: ./trpfs_export)\n"
        "  --dds              extract*: converte cada BNTX exportado em DDS/ASTC ao lado do arquivo\n"
        "  --lod <n>          extract-model/extract-anim: nivel de detalhe (padrao: 0, o mais detalhado)\n"
        "  --bake             extract-model/extract-anim: compoe o shader IkCharacter (BaseColorMap x LayerMaskMap x\n"
        "                     BaseColorLayer1..4) em PNG e converte normal/AO para PNG (glTF sem extensoes)\n"
        "  --png              extract-textures: grava PNG decodificado (BC1/BC3/BC4/BC5/BC7/RGBA) alem do DDS\n"
        "  --dry-run          extract-textures/extract-model: converte e valida sem gravar\n"
        "  --strings <arq>    recover-names: binario com strings de caminhos (repetivel)\n"
        "  --depth <n>        recover-names: barras livres por nome de pack (padrao: 4)\n"
        "  --no-mine          recover-names: nao minerar o conteudo descomprimido\n"
        "  --threads <n>      threads de trabalho (padrao: todos os nucleos)\n"
        "\n"
        "comandos:\n"
        "  info                          resumo do indice\n"
        "  hash <caminho>...             HashPath() de cada caminho e se existe no pacote\n"
        "  stat <hash|caminho>...        pack, compressao e tamanhos\n"
        "  extract <hash|caminho>...     exporta arquivos para o diretorio de saida\n"
        "  extract-pack <nome|indice>    exporta todos os arquivos de um pack\n"
        "  extract-prefix <prefixo>      exporta os caminhos do dicionario que comecam com <prefixo>\n"
        "  list-packs [filtro]           lista packs (indice, arquivos, bytes, nome)\n"
        "  recover-names [saida.txt]     reconstroi o dicionario a partir de analysis/trpfs_names.txt e\n"
        "                                analysis/boot/rodata.bin (padrao: analysis/recovered_dictionary.txt)\n"
        "  bntx-info <alvo>...           metadados de BNTX (arquivo no disco, hash ou caminho)\n"
        "  extract-textures [alvo|filtro]...\n"
        "                                converte texturas BNTX em DDS (BC/RGBA) ou .astc. Sem argumentos\n"
        "                                varre o pacote inteiro; texto que nao e arquivo, hash nem caminho\n"
        "                                filtra por caminho do dicionario ou nome do pack\n"
        "  extract-model [alvo]...       .trmdl -> glTF 2.0 + OBJ com texturas DDS e esqueleto. Sem\n"
        "                                argumentos processa todos os .trmdl do dicionario\n"
        "  extract-anim <.tranm>... [.trmdl]\n"
        "                                glTF do modelo com as animacoes; sem .trmdl usa <dir>/<nome do dir>.trmdl\n"
        "  anim-info <.tranm|.tracn>...  quadros, fps, duracao, trilhas, tipos de canal e chaves\n"
        "  scene-info <.trscn|.trins>... hierarquia de entidades, transformacoes (T/R/S), sub-cenas e assets\n"
        "                                referenciados (.trmdl, .trcol, .trins...) com checagem no pacote\n"
        "  scene-load <.trscn>... [x z]...\n"
        "                                monta a hierarquia (sub-cenas, objetos, templates), carrega as colisoes\n"
        "                                em coordenadas de mundo no CollisionWorld e consulta o chao em (x, z)\n"
        "  col-info <.trcol>... [x z]... malha de colisao hknp (vertices, triangulos, AABB, materiais) e\n"
        "                                RaycastGround em cada (x, z) — padrao: centro da AABB\n"
        "  audio-info <.bnk|.pck>...     versao, chunks, contagem de midias, codecs e primeiros IDs (disco; nome\n"
        "                                solto e procurado em romfs/audio/wwise/bin/Switch)\n"
        "  extract-audio <.bnk|.pck> [media_id]...\n"
        "                                grava <-o>/<arquivo>/<media_id>.wem (.wav se PCM); sem ids extrai todos\n"
        "  poke-info <especie|nome>...   ficha da Pokedex (personal_array.bin): tipos, atributos, EVs, altura, peso\n"
        "                                e media_id do cry; especie = id interno (= n. nacional ate 916)\n"
        "  extract-cry <especie|nome>... resolve PLAY_PV_%04u no PM_VOICE.bnk, decodifica o PTADPCM do Stream.pck\n"
        "                                e grava <-o>/<especie>_cry.wav (PCM 16 bits, 48 kHz estereo)\n"
        "\n"
        "hash: 0x<hex> ou 16 digitos hex; qualquer outro texto e tratado como caminho virtual.\n";
}

std::optional<uint64_t> ParseHash(std::string_view token) {
    const bool prefixed = token.starts_with("0x") || token.starts_with("0X");
    if (prefixed) token.remove_prefix(2);
    if (token.empty() || (!prefixed && token.size() != 16)) return std::nullopt;

    uint64_t value = 0;
    const auto [end, ec] = std::from_chars(token.data(), token.data() + token.size(), value, 16);
    if (ec != std::errc() || end != token.data() + token.size()) return std::nullopt;
    return value;
}

std::optional<unsigned> ParseUnsigned(std::string_view token) {
    unsigned value = 0;
    const auto [end, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (ec != std::errc() || end != token.data() + token.size()) return std::nullopt;
    return value;
}

std::string Hex(uint64_t value) {
    char buffer[19];
    std::snprintf(buffer, sizeof(buffer), "0x%016llx", static_cast<unsigned long long>(value));
    return buffer;
}

std::string HexShort(uint64_t value) {
    char buffer[19];
    std::snprintf(buffer, sizeof(buffer), "0x%llx", static_cast<unsigned long long>(value));
    return buffer;
}

const char* CompressionName(TrpfsArchive::Compression compression) {
    switch (compression) {
    case TrpfsArchive::Compression::None:  return "none";
    case TrpfsArchive::Compression::Oodle: return "oodle";
    }
    return "desconhecida";
}

std::string_view DirOf(std::string_view path) {
    const size_t slash = path.rfind('/');
    return slash == std::string_view::npos ? std::string_view() : path.substr(0, slash);
}

std::string_view StemOf(std::string_view path) {
    const size_t slash = path.rfind('/');
    const std::string_view base = slash == std::string_view::npos ? path : path.substr(slash + 1);
    return base.substr(0, base.find('.'));
}

// "arc/xxx.bntx.trpak" -> "xxx.bntx"
std::string PackStem(std::string_view pack_name) {
    if (pack_name.starts_with("arc/")) pack_name.remove_prefix(4);
    if (pack_name.ends_with(".trpak")) pack_name.remove_suffix(6);
    return std::string(pack_name);
}

std::string SanitizeFileName(std::string_view name) {
    std::string result;
    for (char c : name) {
        const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                          c == '_' || c == '-' || c == '.';
        result.push_back(safe ? c : '_');
    }
    if (result.empty() || result == "." || result == "..") result = "texture";
    return result;
}

std::string FindRomfs(const std::string& explicit_dir) {
    if (!explicit_dir.empty()) return explicit_dir;
    for (const char* candidate : {"romfs", "../romfs", "../../romfs"}) {
        std::error_code ec;
        if (fs_std::exists(fs_std::path(candidate) / "arc" / "data.trpfd", ec)) return candidate;
    }
    return "romfs";
}

// Pasta analysis/ ao lado do romfs/
fs_std::path AnalysisDir(const std::string& romfs_dir) {
    fs_std::path parent = fs_std::path(romfs_dir).lexically_normal().parent_path();
    return (parent.empty() ? fs_std::path(".") : parent) / "analysis";
}

bool IsDiskFile(const std::string& token) {
    std::error_code ec;
    return fs_std::is_regular_file(fs_std::path(token), ec);
}

bool ReadDiskFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    out.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    return true;
}

bool WriteDiskFile(const fs_std::path& path, std::span<const uint8_t> data) {
    std::error_code ec;
    fs_std::create_directories(path.parent_path(), ec);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    return !ec && stream && stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

// Le um arquivo do pacote pelo caminho virtual, sem logar ausencia
bool ReadVirtual(TrpfsArchive& archive, const std::string& path, std::vector<uint8_t>& out) {
    const uint64_t hash = HashPath(path);
    return archive.HasFile(hash) && archive.ReadFile(hash, out);
}

// Subdiretorio de saida das texturas: diretorio do caminho conhecido, senao o nome do pack
std::string OutputDirFor(const TrpfsArchive& archive, const PathDictionary& names, uint64_t hash,
                         std::string_view virtual_path = {}) {
    std::string known = virtual_path.empty() ? std::string() : NormalizePath(virtual_path);
    if (known.empty()) {
        if (const std::string* entry = names.Find(hash)) known = *entry;
    }
    if (const size_t slash = known.rfind('/'); slash != std::string::npos && IsSafeRelativePath(known)) {
        return known.substr(0, slash);
    }
    const TrpfsArchive::Pack* pack = archive.FindPackForFile(hash);
    const std::string stem = pack ? PackStem(pack->name) : std::string("unknown_pack");
    return IsSafeRelativePath(stem) ? stem : "unknown_pack";
}

std::string LabelFor(const PathDictionary& names, uint64_t hash, std::string_view virtual_path = {}) {
    const std::string* known = names.Find(hash);
    return Hex(hash) + "  " + (known ? *known : virtual_path.empty() ? std::string("(caminho desconhecido)")
                                                                     : NormalizePath(virtual_path));
}

bool LoadAsset(TrpfsArchive& archive, const PathDictionary& names, const std::string& token, Asset& out) {
    if (IsDiskFile(token)) {
        out.label = token;
        out.output_dir.clear();
        out.hash = 0;
        if (ReadDiskFile(token, out.data)) return true;
        std::cout << "[TRPFS] Erro: nao foi possivel ler " << token << std::endl;
        return false;
    }

    const std::optional<uint64_t> parsed = ParseHash(token);
    const std::string_view virtual_path = parsed ? std::string_view() : std::string_view(token);
    out.hash = parsed.value_or(HashPath(token));
    if (!archive.ReadFile(out.hash, out.data)) return false;
    out.label = LabelFor(names, out.hash, virtual_path);
    out.output_dir = OutputDirFor(archive, names, out.hash, virtual_path);
    return true;
}

// Converte as texturas de um BNTX em DDS/ASTC dentro de dir; retorna quantas foram gravadas
size_t WriteBntxTextures(std::span<const uint8_t> data, const fs_std::path& dir, const std::string& label) {
    za::gfx::BntxFile bntx;
    std::string error;
    if (!za::gfx::ParseBntx(data, bntx, error)) {
        std::cout << "[TRPFS] " << label << ": " << error << std::endl;
        return 0;
    }
    size_t written = 0;
    for (const za::gfx::BntxTexture& texture : bntx.textures) {
        std::vector<uint8_t> converted;
        std::string extension;
        if (!za::gfx::ConvertTexture(data, texture, converted, extension, error)) {
            std::cout << "[TRPFS] " << label << " [" << texture.name << "]: " << error << std::endl;
            continue;
        }
        const fs_std::path target = dir / (SanitizeFileName(texture.name) + extension);
        if (WriteDiskFile(target, converted)) {
            std::cout << "[TRPFS] Textura " << texture.format.name << " " << texture.width << "x" << texture.height
                      << " -> " << target.string() << std::endl;
            ++written;
        }
    }
    return written;
}

// Exporta e, com --dds, converte o BNTX exportado
bool ExportWithOptions(TrpfsArchive& archive, uint64_t hash, std::string_view virtual_path, const Options& options,
                       const PathDictionary& names) {
    std::string written;
    const bool ok = virtual_path.empty() ? ExportAsset(archive, hash, options.out_dir, &names, &written)
                                         : ExportAsset(archive, virtual_path, options.out_dir, &written);
    if (ok && options.dds) {
        std::vector<uint8_t> data;
        if (archive.ReadFile(hash, data) && za::gfx::IsBntx(data)) {
            WriteBntxTextures(data, fs_std::path(written).parent_path(), LabelFor(names, hash, virtual_path));
        }
    }
    return ok;
}

std::string ChannelString(uint32_t swizzle) {
    static constexpr char kSources[] = "01RGBA";
    std::string text;
    for (int channel = 0; channel < 4; ++channel) {
        const uint32_t source = (swizzle >> (8 * channel)) & 0xFF;
        text.push_back(source < 6 ? kSources[source] : '?');
    }
    return text;
}

void PrintBntx(const Asset& asset, const za::gfx::BntxFile& bntx) {
    std::cout << asset.label << "\n"
              << "  BNTX v" << (bntx.version >> 16) << "." << ((bntx.version >> 8) & 0xFF) << "." << (bntx.version & 0xFF)
              << "  \"" << bntx.name << "\"  " << bntx.textures.size() << " textura(s), " << asset.data.size() << " bytes\n";

    for (size_t i = 0; i < bntx.textures.size(); ++i) {
        const za::gfx::BntxTexture& texture = bntx.textures[i];
        const za::gfx::BntxFormat& format = texture.format;
        const za::gfx::SurfaceLayout layout = za::gfx::ComputeLayout(texture);

        std::cout << "  [" << i << "] " << texture.name << "\n"
                  << "      formato:    " << format.name << " (" << HexShort(format.raw) << ")";
        if (format.Known()) {
            std::cout << ", bloco " << int{format.block_width} << "x" << int{format.block_height} << ", "
                      << int{format.bytes_per_block} << " bytes";
        }
        std::cout << "\n      dimensoes:  " << texture.width << " x " << texture.height;
        if (texture.Is3D()) std::cout << " x " << texture.depth;
        std::cout << ", " << za::gfx::SurfaceDimName(texture.surface_dim);
        if (texture.array_length > 1) std::cout << ", " << texture.array_length << " camadas";
        std::cout << ", " << texture.mip_count << " mip(s)\n"
                  << "      layout:     " << (texture.tile_mode == 0 ? "block linear" : "pitch linear");
        if (texture.tile_mode == 0) {
            std::cout << ", bloco de " << (1u << texture.block_height_log2) << " GOB(s) em Y";
            if (texture.block_depth_log2) std::cout << " x " << (1u << texture.block_depth_log2) << " em Z";
        }
        std::cout << "\n      dados:      " << texture.image_size << " bytes @ " << HexShort(texture.mip_offsets.front())
                  << ", layout calculado ";
        if (!format.Known()) {
            std::cout << "indisponivel (formato desconhecido)";
        } else if (layout.total_size == texture.image_size) {
            std::cout << "confere";
        } else {
            std::cout << "DIFERE (" << layout.total_size << " bytes)";
        }
        std::cout << "\n      canais:     " << ChannelString(texture.channel_swizzle)
                  << "\n      saida:      ";
        if (format.astc) {
            std::cout << ".astc (mip 0)";
        } else if (format.dxgi) {
            std::cout << "DDS DX10, DXGI_FORMAT " << format.dxgi;
        } else {
            std::cout << "sem conversao";
        }
        std::cout << "\n";
    }
}

int CommandBntxInfo(TrpfsArchive& archive, const PathDictionary& names, const std::vector<std::string>& args) {
    size_t ok = 0, failed = 0;
    for (const std::string& token : args) {
        Asset asset;
        if (!LoadAsset(archive, names, token, asset)) {
            ++failed;
            continue;
        }
        za::gfx::BntxFile bntx;
        std::string error;
        if (!za::gfx::ParseBntx(asset.data, bntx, error)) {
            std::cout << asset.label << ": " << error << "\n";
            ++failed;
            continue;
        }
        PrintBntx(asset, bntx);
        ++ok;
    }
    std::cout << "[TRPFS] Concluido: " << ok << " ok, " << failed << " falhas" << std::endl;
    return failed == 0 ? 0 : 1;
}

int CommandExtractTextures(TrpfsArchive& archive, const PathDictionary& names, const Options& options,
                           const std::vector<std::string>& args) {
    // Selecao: arquivos do disco, hashes/caminhos do pacote e filtros por texto
    std::vector<std::string> disk_files;
    std::vector<std::pair<uint64_t, std::string>> selected;  // hash, caminho virtual informado
    std::vector<std::string> filters;
    for (const std::string& token : args) {
        if (IsDiskFile(token)) {
            disk_files.push_back(token);
        } else if (const auto hash = ParseHash(token)) {
            selected.emplace_back(*hash, std::string());
        } else if (archive.HasFile(HashPath(token))) {
            selected.emplace_back(HashPath(token), token);
        } else {
            filters.push_back(token);
        }
    }
    const auto& packs = archive.GetPacks();
    if (args.empty() || !filters.empty()) {
        for (const auto& entry : archive.GetEntries()) {
            bool match = args.empty();
            for (size_t f = 0; !match && f < filters.size(); ++f) {
                const std::string* known = names.Find(entry.hash);
                match = (known && known->find(filters[f]) != std::string::npos) ||
                        packs[entry.pack_index].name.find(filters[f]) != std::string::npos;
            }
            if (match) selected.emplace_back(entry.hash, std::string());
        }
    }

    // Agrupa por pack para ler cada pack uma unica vez
    std::map<uint32_t, std::vector<std::pair<uint64_t, std::string>>> by_pack;
    for (auto& item : selected) {
        uint32_t index = 0;
        if (archive.FindPackIndex(item.first, index)) by_pack[index].push_back(std::move(item));
    }
    std::vector<const std::vector<std::pair<uint64_t, std::string>>*> groups;
    for (const auto& [index, items] : by_pack) groups.push_back(&items);

    std::cout << "[TRPFS] extract-textures: " << (disk_files.size() + selected.size()) << " arquivos em "
              << groups.size() << " packs" << (options.dry_run ? " (dry-run)" : "") << std::endl;

    std::mutex mutex;
    std::map<std::string, size_t> formats;
    std::set<std::string> used_paths;
    std::vector<std::string> failures;
    size_t bntx_files = 0, textures = 0, written = 0, failed = 0, pngs = 0;
    uint64_t bytes_written = 0;

    const auto process = [&](Asset& asset) {
        if (!za::gfx::IsBntx(asset.data)) return;
        za::gfx::BntxFile bntx;
        std::string error;
        if (!za::gfx::ParseBntx(asset.data, bntx, error)) {
            std::lock_guard lock(mutex);
            ++failed;
            failures.push_back(asset.label + ": " + error);
            return;
        }
        {
            std::lock_guard lock(mutex);
            ++bntx_files;
        }

        for (const za::gfx::BntxTexture& texture : bntx.textures) {
            std::vector<uint8_t> converted;
            std::string extension;
            const bool ok = za::gfx::ConvertTexture(asset.data, texture, converted, extension, error);

            fs_std::path target;
            {
                std::lock_guard lock(mutex);
                ++textures;
                ++formats[texture.format.name];
                if (!ok) {
                    ++failed;
                    failures.push_back(asset.label + " [" + texture.name + "]: " + error);
                    continue;
                }
                if (options.dry_run) continue;

                const std::string base = SanitizeFileName(texture.name);
                target = fs_std::path(options.out_dir) / fs_std::path(asset.output_dir) / (base + extension);
                if (!used_paths.insert(target.generic_string()).second) {
                    target.replace_filename(base + "_" + Hex(asset.hash).substr(2) + extension);
                    used_paths.insert(target.generic_string());
                }
            }

            const bool saved = WriteDiskFile(target, converted);
            // --png: copia decodificada para visualizadores comuns (BC6H/ASTC ficam so no DDS/ASTC)
            std::vector<uint8_t> rgba, png;
            const bool png_saved = options.png && za::gfx::DecodeBntxTexture(asset.data, texture, rgba, error) &&
                                   za::gfx::EncodePng(texture.width, texture.height, 4, rgba, png, error) &&
                                   WriteDiskFile(fs_std::path(target).replace_extension(".png"), png);
            std::lock_guard lock(mutex);
            if (saved) {
                ++written;
                bytes_written += converted.size();
                pngs += png_saved ? 1 : 0;
            } else {
                ++failed;
                failures.push_back(target.string() + ": falha ao gravar");
            }
        }
    };

    const size_t cache_budget = archive.GetPackCacheBudget();
    archive.SetPackCacheBudget(std::max<size_t>(cache_budget, size_t{512} << 20));
    const unsigned threads = options.threads ? options.threads : za::DefaultThreadCount();
    za::ParallelFor(disk_files.size() + groups.size(), threads, [&](size_t unit, unsigned) {
        if (unit < disk_files.size()) {
            Asset asset;
            if (LoadAsset(archive, names, disk_files[unit], asset)) process(asset);
            return;
        }
        for (const auto& [hash, virtual_path] : *groups[unit - disk_files.size()]) {
            Asset asset;
            asset.hash = hash;
            if (!archive.ReadFile(hash, asset.data)) continue;
            asset.label = LabelFor(names, hash, virtual_path);
            asset.output_dir = OutputDirFor(archive, names, hash, virtual_path);
            process(asset);
        }
    });
    archive.SetPackCacheBudget(cache_budget);

    std::cout << "[TRPFS] BNTX: " << bntx_files << " arquivos, " << textures << " texturas, " << written
              << " gravadas (" << (bytes_written >> 20) << " MB), " << failed << " falhas";
    if (options.png) std::cout << ", " << pngs << " PNG";
    std::cout << std::endl;
    for (const auto& [name, count] : formats) std::cout << "         " << name << ": " << count << std::endl;
    for (size_t i = 0; i < failures.size() && i < 20; ++i) std::cout << "  falha: " << failures[i] << std::endl;
    if (failures.size() > 20) std::cout << "  ... mais " << (failures.size() - 20) << " falhas" << std::endl;
    return failed == 0 ? 0 : 1;
}

struct ModelReport {
    size_t meshes{0};
    size_t vertices{0};
    size_t triangles{0};
    size_t materials{0};
    size_t textures{0};
    size_t bones{0};
    size_t baked{0};
    size_t unmatched_tracks{0};
    bool empty{false};
    std::vector<std::string> warnings;
    std::string output;
};

std::string_view BaseOf(std::string_view path) {
    const size_t slash = path.rfind('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

// --bake: difusa IkCharacter assada por material + normal/AO em PNG (glTF sem extensoes)
void BakeModelTextures(TrpfsArchive& archive, const std::string& dir, const std::string& stem, const za::gfx::Model& model,
                       const fs_std::path& model_dir, const Options& options, za::gfx::TextureUris& uris,
                       za::gfx::ModelExportOptions& export_options, ModelReport& report) {
    struct Decoded {
        za::gfx::BntxTexture texture;
        std::vector<uint8_t> rgba;
    };
    std::map<std::string, std::optional<Decoded>> cache;
    const auto decode = [&](const std::string& file) -> const Decoded* {
        auto [entry, inserted] = cache.try_emplace(file);
        if (inserted) {
            std::vector<uint8_t> data;
            za::gfx::BntxFile bntx;
            Decoded decoded;
            std::string error;
            if (ReadVirtual(archive, ResolveRelativePath(dir, file), data) && za::gfx::ParseBntx(data, bntx, error) &&
                !bntx.textures.empty() && za::gfx::DecodeBntxTexture(data, bntx.textures.front(), decoded.rgba, error)) {
                decoded.texture = bntx.textures.front();
                entry->second = std::move(decoded);
            } else {
                report.warnings.push_back("bake: " + file + " nao decodificada " + error);
            }
        }
        return entry->second ? &*entry->second : nullptr;
    };
    const auto write_png = [&](const std::string& uri, uint32_t width, uint32_t height, uint32_t channels,
                               std::span<const uint8_t> pixels) {
        std::vector<uint8_t> png;
        std::string error;
        if (!za::gfx::EncodePng(width, height, channels, pixels, png, error) ||
            (!options.dry_run && !WriteDiskFile(model_dir / uri, png))) {
            report.warnings.push_back("bake: falha ao gravar " + uri + " " + error);
            return false;
        }
        return true;
    };

    for (const za::gfx::Material& material : model.materials) {
        const za::gfx::MaterialTexture* base = material.Find("BaseColorMap");
        const za::gfx::MaterialTexture* mask = material.Find("LayerMaskMap");
        const Decoded* base_pixels = base ? decode(base->file) : nullptr;
        if (!base_pixels) continue;
        const Decoded* mask_pixels = mask ? decode(mask->file) : nullptr;

        std::vector<uint8_t> rgba = base_pixels->rgba;  // sem mascara: difusa original (sRGB)
        uint32_t width = base_pixels->texture.width, height = base_pixels->texture.height;
        if (mask_pixels) {
            std::vector<std::array<float, 4>> layers;
            const za::gfx::IkCharacterBakeSettings settings = za::gfx::BakeSettingsFromMaterial(material, layers);
            std::string error;
            if (!za::gfx::BakeIkCharacterTexture(base_pixels->texture, base_pixels->rgba, mask_pixels->texture,
                                                 mask_pixels->rgba, layers, rgba, error, settings)) {
                report.warnings.push_back("bake: material " + material.name + ": " + error);
                continue;
            }
            const std::array<uint32_t, 2> size = za::gfx::BakeOutputSize(base_pixels->texture, mask_pixels->texture, settings);
            width = size[0];
            height = size[1];
        }
        const std::string uri = "textures/" + SanitizeFileName(stem + "_" + material.name) + "_baked.png";
        if (write_png(uri, width, height, 4, rgba)) {
            export_options.base_color_uris[material.name] = uri;
            ++report.baked;
        }
    }

    std::set<std::string> normal_files, occlusion_files;
    for (const za::gfx::Material& material : model.materials) {
        if (const auto* normal = material.Find("NormalMap")) normal_files.insert(normal->file);
        if (const auto* occlusion = material.Find("OcclusionMap")) occlusion_files.insert(occlusion->file);
    }
    for (const std::string& file : normal_files) {
        const Decoded* normal = decode(file);
        if (!normal) continue;
        // Normal em dois canais (BC5): reconstroi Z para o normalTexture RGB do glTF
        const size_t pixels = size_t{normal->texture.width} * normal->texture.height;
        std::vector<uint8_t> rgb(pixels * 3);
        for (size_t p = 0; p < pixels; ++p) {
            const float x = static_cast<float>(normal->rgba[4 * p]) / 127.5f - 1.0f;
            const float y = static_cast<float>(normal->rgba[4 * p + 1]) / 127.5f - 1.0f;
            const float z = std::sqrt(std::max(0.0f, 1.0f - x * x - y * y));
            rgb[3 * p] = normal->rgba[4 * p];
            rgb[3 * p + 1] = normal->rgba[4 * p + 1];
            rgb[3 * p + 2] = static_cast<uint8_t>(std::lround((z * 0.5f + 0.5f) * 255.0f));
        }
        const std::string uri = "textures/" + SanitizeFileName(StemOf(file)) + ".png";
        if (write_png(uri, normal->texture.width, normal->texture.height, 3, rgb)) uris[file] = uri;
    }
    for (const std::string& file : occlusion_files) {
        const Decoded* occlusion = decode(file);
        if (!occlusion) continue;
        std::vector<uint8_t> gray(size_t{occlusion->texture.width} * occlusion->texture.height);
        for (size_t p = 0; p < gray.size(); ++p) gray[p] = occlusion->rgba[4 * p];
        const std::string uri = "textures/" + SanitizeFileName(StemOf(file)) + ".png";
        if (write_png(uri, occlusion->texture.width, occlusion->texture.height, 1, gray)) uris[file] = uri;
    }
}

// .trmdl -> .trmsh (LOD) -> .trmbf, .trmtr, .trskl e texturas, todos relativos ao diretorio do modelo
bool ExtractModel(TrpfsArchive& archive, const std::string& trmdl_path, const Options& options,
                  ModelReport& report, std::string& error,
                  const std::vector<za::gfx::AnimationClip>* clips = nullptr, const std::string& output_stem = {}) {
    const std::string dir(DirOf(trmdl_path));
    const std::string stem(StemOf(trmdl_path));
    std::vector<uint8_t> data;
    za::gfx::TrmdlFile trmdl;
    if (!ReadVirtual(archive, trmdl_path, data)) {
        error = "arquivo nao encontrado no pacote";
        return false;
    }
    if (!za::gfx::ParseTrmdl(data, trmdl, error)) return false;

    za::gfx::Model model;
    model.name = stem;
    if (!trmdl.mesh_files.empty()) {
        const size_t lod = std::min<size_t>(options.lod, trmdl.mesh_files.size() - 1);
        const std::string trmsh_path = ResolveRelativePath(dir, trmdl.mesh_files[lod]);
        std::string buffer_file;
        if (!ReadVirtual(archive, trmsh_path, data)) {
            error = "malha ausente: " + trmsh_path;
            return false;
        }
        if (!za::gfx::ParseTrmsh(data, model.meshes, buffer_file, error)) return false;
        const std::string trmbf_path = ResolveRelativePath(dir, buffer_file);
        if (!ReadVirtual(archive, trmbf_path, data)) {
            error = "buffer ausente: " + trmbf_path;
            return false;
        }
        if (!za::gfx::LoadTrmbf(data, model.meshes, error)) return false;
    } else {
        report.warnings.push_back("modelo-base sem malhas: exportando apenas o esqueleto");
    }

    for (const std::string& file : trmdl.material_files) {
        std::vector<za::gfx::Material> materials;
        std::string material_error;
        if (!ReadVirtual(archive, ResolveRelativePath(dir, file), data) || !za::gfx::ParseTrmtr(data, materials, material_error)) {
            report.warnings.push_back("material " + file + " ignorado " + material_error);
            continue;
        }
        model.materials.insert(model.materials.end(), materials.begin(), materials.end());
    }
    if (!trmdl.skeleton_file.empty()) {
        std::string skeleton_error;
        if (!ReadVirtual(archive, ResolveRelativePath(dir, trmdl.skeleton_file), data) ||
            !za::gfx::ParseTrskl(data, model.skeleton, skeleton_error)) {
            report.warnings.push_back("esqueleto " + trmdl.skeleton_file + " ignorado " + skeleton_error);
            model.skeleton = {};
        }
    }
    if (model.meshes.empty() && model.skeleton.nodes.empty()) {
        report.empty = true;  // placeholders (ex.: *_hicol com 0 malhas) ou esqueleto ausente
        return true;
    }

    // Texturas referenciadas pelos materiais -> textures/<nome>.dds
    const fs_std::path model_dir =
        (fs_std::path(options.out_dir) / (IsSafeRelativePath(dir) ? fs_std::path(dir) : fs_std::path(stem))).make_preferred();
    za::gfx::TextureUris uris;
    std::set<std::string> files;
    for (const za::gfx::Material& material : model.materials) {
        for (const za::gfx::MaterialTexture& texture : material.textures) files.insert(texture.file);
    }
    for (const std::string& file : files) {
        za::gfx::BntxFile bntx;
        std::vector<uint8_t> converted;
        std::string extension, texture_error;
        if (!ReadVirtual(archive, ResolveRelativePath(dir, file), data) || !za::gfx::ParseBntx(data, bntx, texture_error) ||
            bntx.textures.empty() || !za::gfx::ConvertTexture(data, bntx.textures.front(), converted, extension, texture_error)) {
            report.warnings.push_back("textura " + file + " ignorada " + texture_error);
            continue;
        }
        const std::string uri = "textures/" + SanitizeFileName(StemOf(file)) + extension;
        if (!options.dry_run && !WriteDiskFile(model_dir / uri, converted)) {
            report.warnings.push_back("falha ao gravar " + uri);
            continue;
        }
        uris[file] = uri;
        ++report.textures;
    }

    za::gfx::ModelExportOptions export_options;
    if (options.bake) BakeModelTextures(archive, dir, stem, model, model_dir, options, uris, export_options, report);
    if (clips) {
        std::set<std::string_view> bones;
        for (const za::gfx::SkeletonNode& node : model.skeleton.nodes) bones.insert(node.name);
        for (const za::gfx::AnimationClip& clip : *clips) {
            export_options.animations.push_back(&clip);
            for (const za::gfx::AnimationTrack& track : clip.tracks) report.unmatched_tracks += bones.count(track.bone) ? 0 : 1;
        }
    }

    report.meshes = model.meshes.size();
    report.materials = model.materials.size();
    report.bones = model.skeleton.inverse_bind.size();
    for (const za::gfx::MeshShape& mesh : model.meshes) {
        report.vertices += mesh.vertex_count;
        report.triangles += mesh.indices.size() / 3;
    }
    if (options.dry_run) return true;

    const std::string name = output_stem.empty() ? stem : output_stem;
    const fs_std::path gltf = model_dir / (name + ".gltf");
    report.output = gltf.string();
    if (!za::gfx::ExportToGltf(model, gltf.string(), uris, error, export_options)) return false;
    // OBJ e estatico: so acompanha a exportacao sem animacao
    return model.meshes.empty() || clips ||
           za::gfx::ExportToObj(model, (model_dir / (name + ".obj")).string(), uris, error, export_options);
}

int CommandExtractModel(TrpfsArchive& archive, const PathDictionary& names, const Options& options,
                        const std::vector<std::string>& args) {
    std::vector<std::string> models;
    for (const std::string& token : args) {
        if (const auto hash = ParseHash(token)) {
            const std::string* known = names.Find(*hash);
            if (!known) {
                std::cout << "[TRPFS] Erro: " << token << " sem caminho no dicionario (os arquivos do modelo sao "
                          << "relativos ao .trmdl); informe o caminho ou use --names" << std::endl;
                return 1;
            }
            models.push_back(*known);
        } else {
            models.push_back(NormalizePath(token));
        }
    }
    if (args.empty()) {
        for (const auto& [hash, path] : names.Entries()) {
            if (path.ends_with(".trmdl") && archive.HasFile(hash)) models.push_back(path);
        }
        std::sort(models.begin(), models.end());
        if (models.empty()) {
            std::cout << "[TRPFS] Nenhum .trmdl no dicionario; rode recover-names ou informe --names" << std::endl;
            return 1;
        }
    }
    std::cout << "[TRPFS] extract-model: " << models.size() << " modelo(s), LOD " << options.lod
              << (options.dry_run ? " (dry-run)" : "") << std::endl;

    std::mutex mutex;
    size_t ok = 0, failed = 0, empty = 0, vertices = 0, triangles = 0, textures = 0;
    std::vector<std::string> failures;
    const size_t cache_budget = archive.GetPackCacheBudget();
    archive.SetPackCacheBudget(std::max<size_t>(cache_budget, size_t{512} << 20));
    const unsigned threads = options.threads ? options.threads : za::DefaultThreadCount();
    za::ParallelFor(models.size(), threads, [&](size_t i, unsigned) {
        ModelReport report;
        std::string error;
        const bool success = ExtractModel(archive, models[i], options, report, error);
        std::lock_guard lock(mutex);
        if (!success) {
            ++failed;
            failures.push_back(models[i] + ": " + error);
            return;
        }
        if (report.empty) {
            ++empty;
            if (models.size() <= 20) std::cout << "[TRPFS] Modelo " << models[i] << ": vazio (sem malhas nem esqueleto), ignorado" << std::endl;
            return;
        }
        ++ok;
        vertices += report.vertices;
        triangles += report.triangles;
        textures += report.textures;
        if (models.size() <= 20) {
            std::cout << "[TRPFS] Modelo " << models[i] << ": " << report.meshes << " malhas, " << report.vertices
                      << " vertices, " << report.triangles << " triangulos, " << report.materials << " materiais, "
                      << report.textures << " texturas, " << report.bones << " ossos";
            if (options.bake) std::cout << ", " << report.baked << " difusas assadas";
            if (!report.output.empty()) std::cout << " -> " << report.output;
            std::cout << std::endl;
            for (const std::string& warning : report.warnings) std::cout << "         aviso: " << warning << std::endl;
        }
    });
    archive.SetPackCacheBudget(cache_budget);

    std::cout << "[TRPFS] Modelos: " << ok << " ok, " << empty << " vazios, " << failed << " falhas; " << vertices
              << " vertices, " << triangles << " triangulos, " << textures << " texturas" << std::endl;
    std::sort(failures.begin(), failures.end());
    for (size_t i = 0; i < failures.size() && i < 20; ++i) std::cout << "  falha: " << failures[i] << std::endl;
    if (failures.size() > 20) std::cout << "  ... mais " << (failures.size() - 20) << " falhas" << std::endl;
    return failed == 0 ? 0 : 1;
}

// Token (hash ou caminho) -> caminho virtual; hashes precisam do dicionario
std::optional<std::string> ResolveToken(const PathDictionary& names, const std::string& token) {
    if (const auto hash = ParseHash(token)) {
        if (const std::string* known = names.Find(*hash)) return *known;
        std::cout << "[TRPFS] Erro: " << token << " sem caminho no dicionario; informe o caminho ou use --names" << std::endl;
        return std::nullopt;
    }
    return NormalizePath(token);
}

int CommandExtractAnim(TrpfsArchive& archive, const PathDictionary& names, const Options& options,
                       const std::vector<std::string>& args) {
    std::string model_path;
    std::vector<std::string> anim_paths;
    for (const std::string& token : args) {
        const std::optional<std::string> path = ResolveToken(names, token);
        if (!path) return 1;
        if (path->ends_with(".trmdl")) {
            model_path = *path;
        } else {
            anim_paths.push_back(*path);
        }
    }
    if (anim_paths.empty()) {
        PrintUsage();
        return 2;
    }
    if (model_path.empty()) {  // ik_pokemon/data/pm0150/pm0150_00_00/<clipe>.tranm -> .../pm0150_00_00.trmdl
        const std::string dir(DirOf(anim_paths.front()));
        model_path = dir + "/" + std::string(BaseOf(dir)) + ".trmdl";
    }

    std::vector<za::gfx::AnimationClip> clips;
    for (const std::string& path : anim_paths) {
        std::vector<uint8_t> data;
        za::gfx::AnimationClip clip;
        std::string error;
        if (!ReadVirtual(archive, path, data) || !za::gfx::ParseTranm(data, clip, error)) {
            std::cout << "[TRPFS] Erro: " << path << ": " << (error.empty() ? "arquivo nao encontrado" : error) << std::endl;
            return 1;
        }
        clip.name = std::string(StemOf(path));
        std::cout << "[TRPFS] Animacao " << clip.name << ": " << clip.frame_count << " quadros @ " << clip.fps << " fps ("
                  << clip.DurationSeconds() << " s), " << clip.tracks.size() << " trilhas" << std::endl;
        clips.push_back(std::move(clip));
    }

    ModelReport report;
    std::string error;
    const std::string output_stem = clips.size() == 1 ? clips.front().name : std::string(StemOf(model_path)) + "_animations";
    if (!ExtractModel(archive, model_path, options, report, error, &clips, output_stem) || report.empty) {
        std::cout << "[TRPFS] Erro: modelo " << model_path << ": " << (error.empty() ? "vazio" : error) << std::endl;
        return 1;
    }
    std::cout << "[TRPFS] Modelo " << model_path << ": " << report.meshes << " malhas, " << report.bones << " ossos, "
              << clips.size() << " animacao(oes)";
    if (report.unmatched_tracks) std::cout << ", " << report.unmatched_tracks << " trilhas sem osso correspondente";
    std::cout << (report.output.empty() ? "" : " -> " + report.output) << std::endl;
    for (const std::string& warning : report.warnings) std::cout << "         aviso: " << warning << std::endl;
    return 0;
}

int CommandAnimInfo(TrpfsArchive& archive, const PathDictionary& names, const std::vector<std::string>& args) {
    static constexpr const char* kKinds[] = {"-", "Fixed", "Dynamic", "Framed16", "Framed8"};
    size_t ok = 0, failed = 0;
    for (const std::string& token : args) {
        Asset asset;
        if (!LoadAsset(archive, names, token, asset)) {
            ++failed;
            continue;
        }
        const std::string* known = asset.hash ? names.Find(asset.hash) : nullptr;
        const std::string path = known ? *known : token;
        std::string error;
        std::cout << asset.label << "\n";

        if (path.ends_with(".tracn")) {
            std::vector<za::gfx::AnimationSetEntry> entries;
            if (!za::gfx::ParseTracn(asset.data, entries, error)) {
                std::cout << "  " << error << "\n";
                ++failed;
                continue;
            }
            std::cout << "  tracn: descritor de conjunto com " << entries.size() << " entradas (sem keyframes)\n";
            for (const auto& entry : entries) std::cout << "    " << entry.key << " -> " << entry.file << "\n";
            ++ok;
            continue;
        }

        za::gfx::AnimationClip clip;
        if (!za::gfx::ParseTranm(asset.data, clip, error)) {
            std::cout << "  " << error << "\n";
            ++failed;
            continue;
        }
        size_t kinds[3][5] = {}, keys[3] = {}, animated = 0;
        std::vector<std::string> moving;
        for (const za::gfx::AnimationTrack& track : clip.tracks) {
            const za::gfx::AnimationChannelKind channel_kinds[3] = {track.scale.kind, track.rotation.kind, track.translation.kind};
            const size_t counts[3] = {track.scale.keys.size(), track.rotation.keys.size(), track.translation.keys.size()};
            for (size_t c = 0; c < 3; ++c) {
                const auto kind = static_cast<size_t>(channel_kinds[c]);
                ++kinds[c][kind < 5 ? kind : 0];
                keys[c] += counts[c];
            }
            if (track.scale.Animated() || track.rotation.Animated() || track.translation.Animated()) {
                ++animated;
                if (moving.size() < 10) {
                    moving.push_back(track.bone + " (S" + std::to_string(counts[0]) + " R" + std::to_string(counts[1]) +
                                     " T" + std::to_string(counts[2]) + ")");
                }
            }
        }
        std::printf("  tranm: %u quadros @ %u fps = %.3f s%s\n", clip.frame_count, clip.fps, static_cast<double>(clip.DurationSeconds()),
                    clip.loop ? ", loop" : "");
        std::printf("  trilhas: %zu (%zu com curvas animadas)\n", clip.tracks.size(), animated);
        std::printf("  canal        %8s %8s %9s %8s %8s\n", kKinds[1], kKinds[2], kKinds[3], kKinds[4], "chaves");
        const char* channel_names[3] = {"escala", "rotacao", "translacao"};
        for (size_t c = 0; c < 3; ++c) {
            std::printf("  %-12s %8zu %8zu %9zu %8zu %8zu\n", channel_names[c], kinds[c][1], kinds[c][2], kinds[c][3], kinds[c][4], keys[c]);
        }
        std::cout << "  interpolacao: linear entre chaves (rotacao em slerp; quaternion de 48 bits)\n";
        if (!moving.empty()) {
            std::cout << "  ossos animados:";
            for (const std::string& bone : moving) std::cout << " " << bone;
            std::cout << (animated > moving.size() ? " ..." : "") << "\n";
        }
        ++ok;
    }
    std::cout << "[TRPFS] Concluido: " << ok << " ok, " << failed << " falhas" << std::endl;
    return failed == 0 ? 0 : 1;
}

// --- Audio Wwise (arquivos soltos em romfs/audio/wwise/bin/Switch, fora do TRPFS) ---

// Caminho no disco; nome solto (ex.: "PM.bnk") e procurado na pasta Wwise do RomFS
std::string ResolveAudioPath(const std::string& romfs, const std::string& token) {
    if (IsDiskFile(token)) return token;
    const fs_std::path candidate = fs_std::path(romfs) / "audio" / "wwise" / "bin" / "Switch" / token;
    return IsDiskFile(candidate.string()) ? candidate.string() : token;
}

std::optional<uint64_t> ParseMediaId(std::string_view token) {
    const bool hex = token.starts_with("0x") || token.starts_with("0X");
    if (hex) token.remove_prefix(2);
    uint64_t value = 0;
    const auto [end, ec] = std::from_chars(token.data(), token.data() + token.size(), value, hex ? 16 : 10);
    if (token.empty() || ec != std::errc() || end != token.data() + token.size()) return std::nullopt;
    return value;
}

std::string DescribeWem(std::span<const uint8_t> header) {
    za::audio::WemInfo info;
    if (!za::audio::ParseWemInfo(header, info)) return "(sem RIFF/WAVE)";
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "%s 0x%04x, %u ch, %u Hz", std::string(za::audio::WemCodecName(info.codec)).c_str(),
                  info.codec, info.channels, info.sample_rate);
    return buffer;
}

// Media de um .bnk ou de um .pck com acesso uniforme aos bytes
struct AudioSource {
    bool is_package{false};
    za::audio::WwiseBank bank;
    za::audio::WwisePackage package;

    bool Open(const std::string& path, std::string& error) {
        is_package = fs_std::path(path).extension() == ".pck";
        return is_package ? package.Open(path, error) : bank.Load(path, error);
    }

    std::vector<za::audio::WwiseMedia> AllMedia() const {
        if (!is_package) return bank.Media();
        std::vector<za::audio::WwiseMedia> all = package.Streams();
        all.insert(all.end(), package.Banks().begin(), package.Banks().end());
        all.insert(all.end(), package.Externals().begin(), package.Externals().end());
        return all;
    }

    bool Read(const za::audio::WwiseMedia& media, std::vector<uint8_t>& out, std::string& error, size_t max_bytes = SIZE_MAX) {
        if (is_package) return package.Read(media, out, error, max_bytes);
        const std::span<const uint8_t> data = bank.MediaData(media);
        out.assign(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(std::min(data.size(), max_bytes)));
        return true;
    }
};

int CommandAudioInfo(const std::string& romfs, const std::vector<std::string>& args) {
    size_t ok = 0, failed = 0;
    for (const std::string& token : args) {
        const std::string path = ResolveAudioPath(romfs, token);
        AudioSource source;
        std::string error;
        if (!source.Open(path, error)) {
            std::cout << path << ": " << error << "\n";
            ++failed;
            continue;
        }
        std::cout << path << "\n";
        if (source.is_package) {
            const za::audio::WwisePackage& pck = source.package;
            std::cout << "  AKPK versao " << pck.Version() << ", cabecalho de " << pck.HeaderSize() << " bytes, arquivo de "
                      << pck.FileSize() << " bytes\n  idiomas:";
            for (const auto& language : pck.Languages()) std::cout << " " << language.id << "=" << language.name;
            std::cout << "\n  soundbanks: " << pck.Banks().size() << "  streams: " << pck.Streams().size()
                      << "  externos: " << pck.Externals().size() << "\n";
        } else {
            const za::audio::WwiseBank& bnk = source.bank;
            std::cout << "  BKHD versao " << bnk.Header().version << " (0x" << std::hex << bnk.Header().version << std::dec
                      << "), soundbank_id " << bnk.Header().soundbank_id << ", idioma " << bnk.Header().alt_soundbank_id
                      << "\n  chunks:";
            for (const auto& chunk : bnk.Chunks()) std::cout << " " << chunk.tag << "(" << chunk.size << ")";
            std::cout << "\n  midias (DIDX): " << bnk.Media().size() << "  objetos HIRC: " << bnk.HircObjectCount() << "\n";
        }

        // Codecs a partir dos primeiros bytes de cada .wem (o .pck nao e lido por inteiro)
        const std::vector<za::audio::WwiseMedia> media = source.AllMedia();
        std::map<std::string, size_t> codecs;
        uint64_t total = 0;
        std::vector<uint8_t> header;
        for (const auto& entry : media) {
            total += entry.size;
            if (!source.Read(entry, header, error, 256)) continue;
            za::audio::WemInfo info;
            ++codecs[za::audio::ParseWemInfo(header, info) ? std::string(za::audio::WemCodecName(info.codec)) : "?"];
        }
        std::cout << "  total: " << total << " bytes; codecs:";
        for (const auto& [name, count] : codecs) std::cout << " " << name << "=" << count;
        std::cout << "\n";
        for (size_t i = 0; i < media.size() && i < 10; ++i) {
            source.Read(media[i], header, error, 256);
            std::printf("    %10llu  %9u bytes  offset %-10llu  %s\n", static_cast<unsigned long long>(media[i].id), media[i].size,
                        static_cast<unsigned long long>(media[i].offset), DescribeWem(header).c_str());
        }
        if (media.size() > 10) std::cout << "    ... (" << media.size() - 10 << " a mais)\n";
        ++ok;
    }
    return failed == 0 && ok > 0 ? 0 : 1;
}

int CommandExtractAudio(const std::string& romfs, const Options& options, const std::vector<std::string>& args) {
    const std::string path = ResolveAudioPath(romfs, args.front());
    AudioSource source;
    std::string error;
    if (!source.Open(path, error)) {
        std::cout << path << ": " << error << std::endl;
        return 1;
    }
    std::vector<za::audio::WwiseMedia> selected;
    const std::vector<za::audio::WwiseMedia> media = source.AllMedia();
    if (args.size() == 1) {
        selected = media;
    } else {
        for (size_t i = 1; i < args.size(); ++i) {
            const std::optional<uint64_t> id = ParseMediaId(args[i]);
            const auto it = id ? std::find_if(media.begin(), media.end(), [&](const auto& m) { return m.id == *id; }) : media.end();
            if (it == media.end()) {
                std::cout << "[WWISE] media_id nao encontrado: " << args[i] << std::endl;
                return 1;
            }
            selected.push_back(*it);
        }
    }

    const fs_std::path dir = fs_std::path(options.out_dir) / fs_std::path(path).stem();
    size_t ok = 0, failed = 0, wav = 0;
    std::vector<uint8_t> data;
    for (const auto& entry : selected) {
        za::audio::WemInfo info;
        if (!source.Read(entry, data, error) || !za::audio::ParseWemInfo(data, info)) {
            std::cout << "[WWISE] " << entry.id << ": " << (error.empty() ? "sem cabecalho RIFF/WAVE" : error) << std::endl;
            ++failed;
            continue;
        }
        const bool pcm = za::audio::IsPcmCodec(info.codec);
        const fs_std::path out = dir / (std::to_string(entry.id) + (pcm ? ".wav" : ".wem"));
        if (!options.dry_run && !WriteDiskFile(out, data)) {
            std::cout << "[WWISE] Erro ao gravar " << out.string() << std::endl;
            ++failed;
            continue;
        }
        if (selected.size() <= 20) std::cout << "[WWISE] " << out.string() << "  " << DescribeWem(data) << std::endl;
        wav += pcm ? 1 : 0;
        ++ok;
    }
    std::cout << "[WWISE] Concluido: " << ok << " ok (" << wav << " .wav PCM), " << failed << " falhas -> " << dir.string()
              << (options.dry_run ? " (dry-run)" : "") << std::endl;
    return failed == 0 ? 0 : 1;
}

// --- Cenas (.trscn / .trins) e colisao (.trcol) ---

std::string FormatVec3(const std::array<float, 3>& v) {
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "(%.3f, %.3f, %.3f)", v[0], v[1], v[2]);
    return buffer;
}

int CommandSceneInfo(TrpfsArchive& archive, const PathDictionary& names, const std::vector<std::string>& args) {
    size_t ok = 0, failed = 0;
    for (const std::string& token : args) {
        Asset asset;
        if (!LoadAsset(archive, names, token, asset)) {
            ++failed;
            continue;
        }
        const std::string* known = asset.hash ? names.Find(asset.hash) : nullptr;
        const std::string path = known ? *known : NormalizePath(token);
        const std::string dir = path.substr(0, path.rfind('/') == std::string::npos ? 0 : path.rfind('/'));
        std::string error;
        std::cout << asset.label << "\n";

        if (path.ends_with(".trins")) {
            za::gfx::InstanceBuffer instances;
            if (!za::gfx::ParseTrins(asset.data, instances, error)) {
                std::cout << "  " << error << "\n";
                ++failed;
                continue;
            }
            std::cout << "  " << instances.type << ": " << instances.matrices.size() << " instancias (mat4 column-major)\n";
            for (size_t i = 0; i < instances.matrices.size() && i < 8; ++i) {
                const auto& m = instances.matrices[i];
                std::cout << "    [" << i << "] translacao " << FormatVec3({m[12], m[13], m[14]}) << "\n";
            }
            ++ok;
            continue;
        }

        za::gfx::Scene scene;
        if (!za::gfx::ParseTrscn(asset.data, scene, error)) {
            std::cout << "  " << error << "\n";
            ++failed;
            continue;
        }
        // Caminhos relativos ("../x/y.trscn") sao resolvidos a partir do diretorio da cena; uma sub-cena
        // ausente e trocada pelo seu bloco 0 ("y_0.trscn"), que e o arquivo de fato guardado no pacote
        const auto resolve = [&](const std::string& reference) {
            const std::string full = reference.starts_with(".") && !dir.empty() ? ResolveRelativePath(dir, reference)
                                                                                 : NormalizePath(reference);
            const std::string chunk = za::gfx::SceneChunkPath(full);
            return !archive.HasFile(full) && archive.HasFile(chunk) ? chunk : full;
        };
        std::map<std::string, size_t> types;
        std::set<std::string> referenced;
        size_t entity_count = 0;
        scene.Visit([&](const za::gfx::SceneEntity& entity, int) {
            ++entity_count;
            ++types[entity.type];
            for (const std::string& reference : entity.assets) referenced.insert(resolve(reference));
        });
        std::cout << "  cena: " << scene.name << "  camada: " << scene.layer << "  entidades: " << entity_count << "\n";
        for (const std::string& sub : scene.sub_scenes) {
            const std::string full = resolve(sub);
            std::cout << "  sub-cena: " << full << (archive.HasFile(full) ? "" : "  (AUSENTE)") << "\n";
        }

        size_t printed = 0;
        constexpr size_t kMaxLines = 200;
        scene.Visit([&](const za::gfx::SceneEntity& entity, int depth) {
            if (++printed > kMaxLines) return;
            std::cout << "  " << std::string(static_cast<size_t>(depth) * 2, ' ') << entity.type;
            if (!entity.name.empty()) std::cout << " \"" << entity.name << "\"";
            if (entity.has_transform) {
                std::cout << "  T" << FormatVec3(entity.transform.translation);
                if (entity.transform.rotation != std::array<float, 3>{0, 0, 0}) std::cout << " R" << FormatVec3(entity.transform.rotation);
                if (entity.transform.scale != std::array<float, 3>{1, 1, 1}) std::cout << " S" << FormatVec3(entity.transform.scale);
            }
            for (const std::string& reference : entity.assets) std::cout << "  -> " << reference;
            std::cout << "\n";
        });
        if (printed > kMaxLines) std::cout << "  ... (" << printed - kMaxLines << " entidades omitidas)\n";

        std::cout << "  tipos:";
        for (const auto& [type, count] : types) std::cout << " " << type << "=" << count;
        size_t present = 0;
        for (const std::string& reference : referenced) present += archive.HasFile(reference) ? 1 : 0;
        std::cout << "\n  assets referenciados: " << referenced.size() << " (" << present << " no pacote)\n";
        for (const std::string& reference : referenced) {
            std::cout << "    " << (archive.HasFile(reference) ? "OK      " : "AUSENTE ") << reference << "\n";
        }
        ++ok;
    }
    std::cout << "[TRPFS] Concluido: " << ok << " ok, " << failed << " falhas" << std::endl;
    return failed == 0 ? 0 : 1;
}

int CommandColInfo(TrpfsArchive& archive, const PathDictionary& names, const std::vector<std::string>& args) {
    // Pares numericos finais (x z) viram consultas de RaycastGround
    std::vector<std::string> targets = args;
    std::vector<std::pair<float, float>> probes;
    const auto is_number = [](const std::string& s) {
        char* end = nullptr;
        std::strtof(s.c_str(), &end);
        return !s.empty() && end == s.c_str() + s.size();
    };
    while (targets.size() >= 3 && is_number(targets[targets.size() - 1]) && is_number(targets[targets.size() - 2])) {
        probes.insert(probes.begin(), {std::strtof(targets[targets.size() - 2].c_str(), nullptr),
                                       std::strtof(targets.back().c_str(), nullptr)});
        targets.resize(targets.size() - 2);
    }

    size_t ok = 0, failed = 0;
    for (const std::string& token : targets) {
        Asset asset;
        za::gfx::CollisionMesh mesh;
        std::string error;
        if (!LoadAsset(archive, names, token, asset) || !za::gfx::ParseTrcol(asset.data, mesh, error)) {
            if (!error.empty()) std::cout << token << ": " << error << "\n";
            ++failed;
            continue;
        }
        std::cout << asset.label << "\n"
                  << "  hknpMeshShape: " << mesh.section_count << " secoes, " << mesh.vertices.size() << " vertices, "
                  << mesh.triangles.size() << " triangulos (" << mesh.quad_count << " quads)\n"
                  << "  AABB: " << FormatVec3(mesh.aabb_min) << " .. " << FormatVec3(mesh.aabb_max) << "\n"
                  << "  grade de raycast: " << mesh.grid_x << " x " << mesh.grid_z << " celulas\n  materiais:";
        for (const std::string& material : mesh.materials) std::cout << " " << material;
        std::cout << "\n";
        const auto probe_list = probes.empty() ? std::vector<std::pair<float, float>>{{(mesh.aabb_min[0] + mesh.aabb_max[0]) / 2,
                                                                                       (mesh.aabb_min[2] + mesh.aabb_max[2]) / 2}}
                                               : probes;
        for (const auto& [x, z] : probe_list) {
            float y = 0;
            std::array<float, 3> normal{};
            if (za::gfx::RaycastGround(mesh, x, z, y, normal)) {
                std::printf("  raycast (%.3f, %.3f): y = %.4f  normal %s\n", x, z, y, FormatVec3(normal).c_str());
            } else {
                std::printf("  raycast (%.3f, %.3f): sem chao\n", x, z);
            }
        }
        ++ok;
    }
    std::cout << "[TRPFS] Concluido: " << ok << " ok, " << failed << " falhas" << std::endl;
    return failed == 0 ? 0 : 1;
}

// Monta as cenas (hierarquia completa), carrega as colisoes no CollisionWorld e consulta o chao em (x, z)
int CommandSceneLoad(TrpfsArchive& archive, const std::vector<std::string>& args) {
    std::vector<std::string> scenes = args;
    std::vector<std::pair<float, float>> probes;
    const auto is_number = [](const std::string& s) {
        char* end = nullptr;
        std::strtof(s.c_str(), &end);
        return !s.empty() && end == s.c_str() + s.size();
    };
    while (scenes.size() >= 3 && is_number(scenes.back()) && is_number(scenes[scenes.size() - 2])) {
        probes.insert(probes.begin(), {std::strtof(scenes[scenes.size() - 2].c_str(), nullptr), std::strtof(scenes.back().c_str(), nullptr)});
        scenes.resize(scenes.size() - 2);
    }
    za::gfx::SceneAssembler assembler([&archive](const std::string& path, std::vector<uint8_t>& out) {
        const uint64_t hash = HashPath(path);
        return archive.HasFile(hash) && archive.ReadFile(hash, out);
    });
    std::string error;
    for (const std::string& scene : scenes) {
        if (!assembler.AddScene(scene, error)) std::cout << "[SCENE] " << error << "\n";
    }
    za::gfx::CollisionWorld world;
    std::string col_error;
    const size_t meshes = assembler.BuildCollisionWorld(world, col_error);
    std::cout << "[SCENE] " << assembler.Scenes().size() << " cenas, " << assembler.EntityCount() << " entidades, "
              << assembler.Models().size() << " modelos, " << assembler.Collisions().size() << " colisoes ("
              << assembler.BoxCollisionCount() << " caixas ignoradas), " << assembler.MissingScenes().size() << " cenas ausentes\n"
              << "[SCENE] CollisionWorld: " << meshes << " malhas, AABB " << FormatVec3(world.BoundsMin()) << " .. "
              << FormatVec3(world.BoundsMax()) << "\n";
    if (!col_error.empty()) std::cout << "[SCENE] falhas de colisao: " << col_error << "\n";
    for (size_t i = 0; i < world.MeshCount() && i < 40; ++i) {
        const za::gfx::CollisionMesh& mesh = world.Mesh(i);
        std::cout << "  " << assembler.Collisions()[i].path << "  " << mesh.triangles.size() << " tri  " << FormatVec3(mesh.aabb_min)
                  << " .. " << FormatVec3(mesh.aabb_max) << "\n";
    }
    for (const std::string& missing : assembler.MissingScenes()) std::cout << "  ausente: " << missing << "\n";
    for (const auto& [x, z] : probes) {
        float y = 0;
        std::array<float, 3> normal{};
        if (world.GetGroundHeight(x, z, y, normal)) {
            std::printf("  chao (%.3f, %.3f): y = %.4f  normal %s\n", x, z, y, FormatVec3(normal).c_str());
        } else {
            std::printf("  chao (%.3f, %.3f): nenhum\n", x, z);
        }
    }
    return assembler.Scenes().empty() ? 1 : 0;
}

// --- Pokedex (personal_array.bin + monsname.dat) e cries (PM_VOICE.bnk + Stream.pck) ---

struct PokedexContext {
    za::data::PersonalDatabase personal;
    std::vector<std::string> names;   // indice = id interno da especie
    za::audio::WwiseBank voice;
    za::audio::WwisePackage stream;
    bool has_voice{false};
    bool has_stream{false};

    bool Load(TrpfsArchive& archive, const std::string& romfs, std::string& error) {
        std::vector<uint8_t> data;
        if (!ReadVirtual(archive, "avalon/data/personal_array.bin", data) || !personal.Load(data, error)) {
            if (error.empty()) error = "avalon/data/personal_array.bin ausente";
            return false;
        }
        std::string ignored;
        if (ReadVirtual(archive, "ik_message/dat/English/common/monsname.dat", data)) za::data::ParseMessageText(data, names, ignored);
        const fs_std::path audio = fs_std::path(romfs) / "audio" / "wwise" / "bin" / "Switch";
        has_voice = voice.Load((audio / "PM_VOICE.bnk").string(), ignored);
        has_stream = stream.Open((audio / "Stream.pck").string(), ignored);
        return true;
    }

    std::string Name(uint16_t species) const { return species < names.size() ? names[species] : std::string("?"); }

    // Numero = id interno da especie; texto = nome em ingles (sem diferenciar maiusculas)
    std::optional<uint16_t> Resolve(const std::string& token) const {
        if (const std::optional<unsigned> number = ParseUnsigned(token); number && *number < 10000) {
            return static_cast<uint16_t>(*number);
        }
        const auto lower = [](std::string text) {
            for (char& c : text) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
            return text;
        };
        const std::string wanted = lower(token);
        for (size_t i = 1; i < names.size(); ++i) {
            if (lower(names[i]) == wanted) return static_cast<uint16_t>(i);
        }
        return std::nullopt;
    }

    // Bytes do .wem do cry (Stream.pck ou DATA do PM_VOICE.bnk)
    bool ReadCry(const za::audio::WwiseSoundSource& source, std::vector<uint8_t>& out, std::string& error, size_t max_bytes = SIZE_MAX) {
        if (source.InStreamPackage()) {
            const za::audio::WwiseMedia* media = has_stream ? stream.Find(source.media_id) : nullptr;
            if (!media) {
                error = "midia " + std::to_string(source.media_id) + " ausente do Stream.pck";
                return false;
            }
            return stream.Read(*media, out, error, max_bytes);
        }
        const std::span<const uint8_t> bytes = voice.MediaData(source.media_id);
        if (bytes.empty()) {
            error = "midia " + std::to_string(source.media_id) + " ausente do PM_VOICE.bnk";
            return false;
        }
        out.assign(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(std::min(bytes.size(), max_bytes)));
        return true;
    }
};

int CommandPokeInfo(TrpfsArchive& archive, const std::string& romfs, const std::vector<std::string>& args) {
    PokedexContext dex;
    std::string error;
    if (!dex.Load(archive, romfs, error)) {
        std::cout << "[POKEDEX] " << error << std::endl;
        return 1;
    }
    size_t ok = 0, failed = 0;
    for (const std::string& token : args) {
        const std::optional<uint16_t> species = dex.Resolve(token);
        const auto forms = species ? dex.personal.Forms(*species) : std::vector<const za::data::PokemonPersonalData*>{};
        if (forms.empty()) {
            std::cout << "[POKEDEX] especie nao encontrada: " << token << "\n";
            ++failed;
            continue;
        }
        for (const za::data::PokemonPersonalData* p : forms) {
            const za::data::PokemonStats& s = p->base_stats;
            const za::data::PokemonStats& ev = p->ev_yield;
            std::printf("#%04u %s  forma %u  (n. nacional %u)\n", p->species_id, dex.Name(p->species_id).c_str(), p->form_id,
                        p->national_dex);
            std::printf("  tipos:      %s", std::string(dex.personal.GetTypeName(p->type_1)).c_str());
            if (!p->SingleType()) std::printf(" / %s", std::string(dex.personal.GetTypeName(p->type_2)).c_str());
            std::printf("\n  atributos:  HP %u  Ataque %u  Defesa %u  Atq. Esp %u  Def. Esp %u  Velocidade %u  (total %u)\n", s.hp,
                        s.attack, s.defense, s.sp_attack, s.sp_defense, s.speed, s.Total());
            std::printf("  EVs:        %u/%u/%u/%u/%u/%u\n", ev.hp, ev.attack, ev.defense, ev.sp_attack, ev.sp_defense, ev.speed);
            std::printf("  altura:     %.2f m   peso: %.1f kg\n", p->HeightMeters(), p->WeightKg());
            std::printf("  captura:    %u   ciclos de ovo: %u   amizade: %u   grupos de ovo: %u/%u\n", p->catch_rate,
                        p->hatch_cycles, p->base_friendship, p->egg_group_1, p->egg_group_2);
        }
        za::audio::WwiseSoundSource cry;
        std::vector<uint8_t> header;
        if (dex.has_voice && za::audio::ResolvePokemonCry(dex.voice, *species, cry, error)) {
            std::printf("  cry:        PLAY_PV_%04u -> media_id %u (%s)", *species, cry.media_id,
                        cry.InStreamPackage() ? "Stream.pck" : "PM_VOICE.bnk");
            if (dex.ReadCry(cry, header, error, 256)) std::printf("  %s", DescribeWem(header).c_str());
            std::printf("\n");
        } else {
            std::printf("  cry:        %s\n", dex.has_voice ? error.c_str() : "PM_VOICE.bnk nao encontrado");
        }
        ++ok;
    }
    return failed == 0 && ok > 0 ? 0 : 1;
}

int CommandExtractCry(TrpfsArchive& archive, const std::string& romfs, const Options& options, const std::vector<std::string>& args) {
    PokedexContext dex;
    std::string error;
    if (!dex.Load(archive, romfs, error) || !dex.has_voice) {
        std::cout << "[CRY] " << (error.empty() ? "PM_VOICE.bnk nao encontrado" : error) << std::endl;
        return 1;
    }
    size_t ok = 0, failed = 0;
    for (const std::string& token : args) {
        const std::optional<uint16_t> species = dex.Resolve(token);
        za::audio::WwiseSoundSource cry;
        std::vector<uint8_t> wem;
        za::audio::PcmBuffer pcm;
        if (!species) {
            error = "especie nao encontrada: " + token;
        } else if (za::audio::ResolvePokemonCry(dex.voice, *species, cry, error) && dex.ReadCry(cry, wem, error) &&
                   za::audio::DecodeWem(wem, pcm, error)) {
            const za::audio::PcmBuffer out = za::audio::ResampleLinear(pcm, 48000, 2);
            const fs_std::path path = fs_std::path(options.out_dir) / (std::to_string(*species) + "_cry.wav");
            if (options.dry_run || WriteDiskFile(path, za::audio::BuildWav(out))) {
                std::printf("[CRY] #%04u %s: media_id %u, %s -> %s (%.2f s, %u Hz %u ch -> 48000 Hz estereo)\n", *species,
                            dex.Name(*species).c_str(), cry.media_id, DescribeWem(wem).c_str(), path.string().c_str(),
                            out.DurationSeconds(), pcm.sample_rate, pcm.channels);
                ++ok;
                continue;
            }
            error = "nao foi possivel gravar " + path.string();
        }
        std::cout << "[CRY] " << token << ": " << error << "\n";
        ++failed;
    }
    std::cout << "[CRY] Concluido: " << ok << " ok, " << failed << " falhas" << std::endl;
    return failed == 0 ? 0 : 1;
}

int CommandRecoverNames(TrpfsArchive& archive, const Options& options, const fs_std::path& analysis,
                        const std::vector<std::string>& args) {
    if (args.size() > 1) {
        PrintUsage();
        return 2;
    }
    const std::string output = args.empty() ? (analysis / "recovered_dictionary.txt").string() : args.front();

    std::error_code ec;
    PathDictionary seed;
    const std::string seed_path = !options.names_path.empty() ? options.names_path : (analysis / "trpfs_names.txt").string();
    if (fs_std::exists(seed_path, ec) && !seed.LoadFromFile(seed_path)) return 1;

    std::vector<std::string> sources = options.string_sources;
    if (sources.empty() && fs_std::exists(analysis / "boot" / "rodata.bin", ec)) sources.push_back((analysis / "boot" / "rodata.bin").string());

    NameRecovery recovery(archive, seed);
    for (const std::string& source : sources) {
        std::vector<uint8_t> data;
        if (!ReadDiskFile(source, data)) {
            std::cout << "[TRPFS] Erro: nao foi possivel ler " << source << std::endl;
            return 1;
        }
        recovery.AddSeedBinary(data);
    }

    NameRecoveryOptions recovery_options;
    recovery_options.max_free_slashes = options.depth;
    recovery_options.threads = options.threads;
    recovery_options.mine_content = options.mine.value_or(true);
    const NameRecoveryStats stats = recovery.Run(recovery_options);

    const std::string header =
        "# Caminhos virtuais do TRPFS verificados contra data.trpfd (FNV-1a 64, base 0xCBF29CE484222645)\n"
        "# Gerado por trpfs_tool recover-names: " + std::to_string(stats.total) + " caminhos de " +
        std::to_string(archive.GetEntryCount()) + " arquivos; " + std::to_string(stats.packs_resolved) + "/" +
        std::to_string(archive.GetPackCount()) + " packs nomeados\n"
        "# Origem: semente " + std::to_string(stats.seed) + ", strings " + std::to_string(stats.from_strings) +
        ", nomes de pack " + std::to_string(stats.from_packs) + ", irmaos " + std::to_string(stats.from_siblings) +
        ", numericos " + std::to_string(stats.from_numeric) + ", conteudo " + std::to_string(stats.from_content) + "\n";
    return recovery.Result().SaveToFile(output, header) ? 0 : 1;
}

int Run(int argc, char** argv) {
    Options options;
    std::vector<std::string> args;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (arg == "--romfs" && has_value) {
            options.romfs_dir = argv[++i];
        } else if (arg == "--names" && has_value) {
            options.names_path = argv[++i];
        } else if (arg == "-o" && has_value) {
            options.out_dir = argv[++i];
        } else if (arg == "--strings" && has_value) {
            options.string_sources.emplace_back(argv[++i]);
        } else if ((arg == "--depth" || arg == "--threads" || arg == "--lod") && has_value) {
            const std::optional<unsigned> value = ParseUnsigned(argv[++i]);
            if (!value || (arg == "--depth" && *value > 8)) {
                PrintUsage();
                return 2;
            }
            if (arg == "--depth") options.depth = static_cast<int>(*value);
            else if (arg == "--lod") options.lod = *value;
            else options.threads = *value;
        } else if (arg == "--mine" || arg == "--no-mine") {
            options.mine = arg == "--mine";
        } else if (arg == "--dry-run") {
            options.dry_run = true;
        } else if (arg == "--dds") {
            options.dds = true;
        } else if (arg == "--bake") {
            options.bake = true;
        } else if (arg == "--png") {
            options.png = true;
        } else if (arg == "-h" || arg == "--help") {
            PrintUsage();
            return 0;
        } else {
            args.emplace_back(arg);
        }
    }
    if (args.empty()) {
        PrintUsage();
        return 2;
    }

    const std::string command = args.front();
    args.erase(args.begin());
    const std::string romfs = FindRomfs(options.romfs_dir);
    const fs_std::path analysis = AnalysisDir(romfs);

    // Audio Wwise fica fora do TRPFS
    if (command == "audio-info" || command == "extract-audio") {
        if (args.empty()) {
            PrintUsage();
            return 2;
        }
        return command == "audio-info" ? CommandAudioInfo(romfs, args) : CommandExtractAudio(romfs, options, args);
    }

    // bntx-info de arquivos soltos no disco dispensa o pacote
    const bool disk_only = command == "bntx-info" && !args.empty() && std::all_of(args.begin(), args.end(), IsDiskFile);
    TrpfsArchive archive;
    if (!disk_only) {
        const fs_std::path arc = fs_std::path(romfs) / "arc";
        if (!archive.Mount((arc / "data.trpfd").string(), (arc / "data.trpfs").string())) return 1;
    }
    if (command == "recover-names") return CommandRecoverNames(archive, options, analysis, args);

    // Dicionario: --names, senao o reconstruido por recover-names (se existir)
    PathDictionary names;
    std::error_code ec;
    const std::string names_path = !options.names_path.empty() ? options.names_path
                                                                : (analysis / "recovered_dictionary.txt").string();
    if ((!options.names_path.empty() || fs_std::exists(names_path, ec)) && !names.LoadFromFile(names_path)) return 1;

    if (command == "bntx-info") {
        if (args.empty()) {
            PrintUsage();
            return 2;
        }
        return CommandBntxInfo(archive, names, args);
    }
    if (command == "extract-textures") return CommandExtractTextures(archive, names, options, args);
    if (command == "extract-model") return CommandExtractModel(archive, names, options, args);
    if (command == "extract-anim") return CommandExtractAnim(archive, names, options, args);
    if (command == "anim-info") {
        if (args.empty()) {
            PrintUsage();
            return 2;
        }
        return CommandAnimInfo(archive, names, args);
    }
    if (command == "scene-info" || command == "col-info") {
        if (args.empty()) {
            PrintUsage();
            return 2;
        }
        return command == "scene-info" ? CommandSceneInfo(archive, names, args) : CommandColInfo(archive, names, args);
    }
    if (command == "scene-load") {
        if (args.empty()) {
            PrintUsage();
            return 2;
        }
        return CommandSceneLoad(archive, args);
    }
    if (command == "poke-info" || command == "extract-cry") {
        if (args.empty()) {
            PrintUsage();
            return 2;
        }
        return command == "poke-info" ? CommandPokeInfo(archive, romfs, args) : CommandExtractCry(archive, romfs, options, args);
    }

    size_t ok = 0, failed = 0;

    if (command == "info") {
        size_t named = 0;
        for (const auto& entry : archive.GetEntries()) named += names.Find(entry.hash) ? 1 : 0;
        std::cout << "arquivos: " << archive.GetEntryCount() << "\n"
                  << "packs:    " << archive.GetPackCount() << "\n"
                  << "nomeados: " << named << " (dicionario com " << names.Size() << " caminhos)\n"
                  << "oodle:    " << (oodle::IsAvailable() ? "disponivel" : "indisponivel") << "\n";
        return 0;
    }

    if (command == "hash") {
        for (const std::string& path : args) {
            const uint64_t hash = HashPath(path);
            const bool found = archive.HasFile(hash);
            std::cout << Hex(hash) << "  " << (found ? "OK     " : "AUSENTE") << "  " << NormalizePath(path) << "\n";
            ++(found ? ok : failed);
        }
    } else if (command == "stat") {
        for (const std::string& token : args) {
            const uint64_t hash = ParseHash(token).value_or(HashPath(token));
            TrpfsArchive::FileInfo info;
            if (!archive.GetFileInfo(hash, info)) {
                ++failed;
                continue;
            }
            const std::string* known = names.Find(hash);
            std::cout << Hex(hash) << "\n"
                      << "  caminho:     " << (known ? *known : ParseHash(token) ? "(desconhecido)" : NormalizePath(token)) << "\n"
                      << "  pack:        #" << info.pack_index << " " << archive.GetPacks()[info.pack_index].name << "\n"
                      << "  compressao:  " << CompressionName(info.compression) << "\n"
                      << "  armazenado:  " << info.stored_size << " bytes\n"
                      << "  tamanho:     " << info.size << " bytes\n";
            ++ok;
        }
    } else if (command == "extract") {
        for (const std::string& token : args) {
            const std::optional<uint64_t> hash = ParseHash(token);
            const bool exported = hash ? ExportWithOptions(archive, *hash, {}, options, names)
                                       : ExportWithOptions(archive, HashPath(token), token, options, names);
            ++(exported ? ok : failed);
        }
    } else if (command == "extract-pack") {
        if (args.size() != 1) {
            PrintUsage();
            return 2;
        }
        const auto& packs = archive.GetPacks();
        std::optional<uint32_t> pack_index;
        const std::string& key = args.front();
        if (const std::optional<unsigned> parsed = ParseUnsigned(key); parsed && *parsed < packs.size()) {
            pack_index = *parsed;
        } else {
            for (uint32_t i = 0; i < packs.size(); ++i) {
                if (packs[i].name == key || packs[i].name == "arc/" + key) {
                    pack_index = i;
                    break;
                }
            }
        }
        if (!pack_index) {
            std::cout << "[TRPFS] Erro: pack nao encontrado: " << key << std::endl;
            return 1;
        }
        std::cout << "[TRPFS] Extraindo pack #" << *pack_index << " " << packs[*pack_index].name << std::endl;
        for (const auto& entry : archive.GetEntries()) {
            if (entry.pack_index == *pack_index) ++(ExportWithOptions(archive, entry.hash, {}, options, names) ? ok : failed);
        }
    } else if (command == "extract-prefix") {
        if (args.size() != 1 || names.Size() == 0) {
            std::cout << "extract-prefix exige um prefixo e um dicionario (--names)\n";
            return 2;
        }
        const std::string prefix = NormalizePath(args.front());
        for (const auto& [hash, path] : names.Entries()) {
            if (path.starts_with(prefix) && archive.HasFile(hash)) ++(ExportWithOptions(archive, hash, path, options, names) ? ok : failed);
        }
    } else if (command == "list-packs") {
        const std::string filter = args.empty() ? std::string() : args.front();
        const auto& packs = archive.GetPacks();
        for (uint32_t i = 0; i < packs.size(); ++i) {
            if (!filter.empty() && packs[i].name.find(filter) == std::string::npos) continue;
            std::cout << i << "\t" << packs[i].file_count << "\t" << packs[i].size << "\t" << packs[i].name << "\n";
        }
        return 0;
    } else {
        PrintUsage();
        return 2;
    }

    std::cout << "[TRPFS] Concluido: " << ok << " ok, " << failed << " falhas" << std::endl;
    return failed == 0 ? 0 : 1;
}

} // namespace

#ifdef _WIN32
extern "C" __declspec(dllimport) int __stdcall SetConsoleOutputCP(unsigned int code_page);
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);  // nomes e tipos com acento (UTF-8) no console do Windows
#endif
    return Run(argc, argv);
}
