// Teste de integracao do TRPFS contra os dados reais do RomFS
// uso: trpfs_test <romfs_dir> <tests/data/trpfs_refs.txt>
//   trpfs_refs.txt: "<hash hex> <tamanho> <crc32 hex>" de arquivos sem compressao (gerado em Python)

#include "vfs/asset_export.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace za::vfs;

static_assert(Fnv1a64("") == kFnvOffsetBasis);

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    std::printf("%s %s\n", condition ? "[PASS]" : "[FAIL]", what);
    if (!condition) ++g_failures;
}

uint32_t Crc32(const std::vector<uint8_t>& data) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint8_t b : data) {
        crc ^= b;
        for (int i = 0; i < 8; ++i) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

std::vector<uint8_t> ReadDisk(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

// Arquivo comprimido (Oodle) conhecido: pack arc/ai_influenceba_battle_area_0003_01_00_00.imap.trpak
constexpr uint64_t kCompressedSample = 0x05AFD1BF8BA6BBF9ULL;

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::puts("uso: trpfs_test <romfs_dir> <trpfs_refs.txt>");
        return 2;
    }
    const std::filesystem::path arc = std::filesystem::path(argv[1]) / "arc";

    Check(NormalizePath("data:/system\\config/a.bin") == "system/config/a.bin", "NormalizePath remove prefixo e converte '\\'");
    Check(NormalizePath("rom:/x") == "x" && NormalizePath("//y") == "y", "NormalizePath rom:/ e barras iniciais");
    Check(HashPath("data:/arc/a") == Fnv1a64("arc/a"), "HashPath == Fnv1a64(NormalizePath)");

    const uint8_t bntx[] = {'B', 'N', 'T', 'X', 0, 0};
    const uint8_t flatbuffer[] = {0x0C, 0, 0, 0};
    Check(DetectExtension(bntx) == ".bntx" && DetectExtension(flatbuffer).empty(), "DetectExtension");

    TrpfsArchive archive;
    if (!archive.Mount((arc / "data.trpfd").string(), (arc / "data.trpfs").string())) return 1;
    Check(archive.GetEntryCount() == 178109 && archive.GetPackCount() == 9892, "Mount: 178109 arquivos / 9892 packs");

    // Caminhos reais do rodata.bin (hash de arquivo confirmado com a base Game Freak)
    for (const char* path : {
             "world/ik_data/battle/lockon_desc/player.bin",
             "system_resource/gfx2/shader/NX64/env_ikkaku/system/ao_calc.trsha",
             "system_resource/gfx2/shader/NX64/env_ikkaku/system/ao_blur_h.bnsh",
             "world/ik_data/audio/env_sound/env_sound/env_sound_array.bin",
         }) {
        Check(archive.HasFile(std::string_view(path)), path);
    }
    Check(!archive.HasFile(0x1ULL), "HasFile(hash inexistente) == false");

    std::ifstream refs(argv[2]);
    std::string hash_hex, crc_hex;
    size_t size = 0, verified = 0, mismatched = 0;
    std::vector<uint8_t> buffer;
    uint64_t export_hash = 0;
    while (refs >> hash_hex >> size >> crc_hex) {
        const uint64_t hash = std::strtoull(hash_hex.c_str(), nullptr, 16);
        const uint32_t crc = static_cast<uint32_t>(std::strtoul(crc_hex.c_str(), nullptr, 16));
        TrpfsArchive::FileInfo info;
        const bool ok = archive.GetFileInfo(hash, info) && info.compression == TrpfsArchive::Compression::None &&
                        info.size == size && archive.ReadFile(hash, buffer) && buffer.size() == size && Crc32(buffer) == crc;
        ++(ok ? verified : mismatched);
        if (ok && size > 0 && export_hash == 0) export_hash = hash;
    }
    Check(verified >= 300 && mismatched == 0, "300 arquivos sem compressao: GetFileInfo + ReadFile + CRC32");

    // ExportAsset: grava em diretorio temporario e compara com ReadFile
    const auto out_dir = std::filesystem::temp_directory_path() / "za_trpfs_test_export";
    std::filesystem::remove_all(out_dir);
    std::string written;
    const bool exported = export_hash != 0 && ExportAsset(archive, export_hash, out_dir.string(), nullptr, &written);
    Check(exported && archive.ReadFile(export_hash, buffer) && ReadDisk(written) == buffer, "ExportAsset grava bytes identicos");
    Check(exported && written.find(".trpak") == std::string::npos, "ExportAsset nomeia por <pack>/<hash>.<ext>");
    std::filesystem::remove_all(out_dir);

    TrpfsArchive::FileInfo compressed;
    Check(archive.GetFileInfo(kCompressedSample, compressed) && compressed.compression == TrpfsArchive::Compression::Oodle &&
              compressed.size > compressed.stored_size, "GetFileInfo de arquivo Oodle (sem descomprimir)");
    Check(archive.ReadFile(kCompressedSample, buffer) == oodle::IsAvailable(), "ReadFile Oodle == disponibilidade da DLL");
    if (oodle::IsAvailable()) Check(buffer.size() == compressed.size, "Oodle: tamanho descomprimido confere");

    std::printf("%s (%d falhas)\n", g_failures == 0 ? "OK" : "FALHOU", g_failures);
    return g_failures == 0 ? 0 : 1;
}
