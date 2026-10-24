#pragma once
// Leitura de modulos NSO do Switch (NSO0) para o recompilador
//
// NSO0: cabecalho de 0x100 bytes com 3 segmentos (.text, .rodata, .data), cada um opcionalmente comprimido em
// LZ4 (bloco) e com SHA-256 do conteudo descomprimido; .bss segue .data. O modulo e mapeado numa imagem plana
// (offsets = RVA a partir da base do modulo). .text[4] aponta para o MOD0, que da a .dynamic, a faixa do .bss e o
// .eh_frame_hdr. A .dynamic e ELF64 padrao (RELA/JMPREL/SYMTAB/STRTAB, relocacoes R_AARCH64_*).

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace recomp {

struct Segment {
    uint32_t file_offset{0};
    uint32_t memory_offset{0};   // RVA
    uint32_t size{0};            // descomprimido
    uint32_t file_size{0};       // no arquivo
    bool compressed{false};
    bool hash_checked{false};
    std::array<uint8_t, 32> sha256{};
};

struct Symbol {
    std::string name;
    uint64_t value{0};           // RVA (0 para importacoes)
    uint64_t size{0};
    uint8_t type{0};             // STT_*
    uint8_t binding{0};          // STB_*
    uint16_t section{0};         // 0 = indefinido (importacao)

    bool IsImport() const { return section == 0 && !name.empty(); }
    bool IsFunction() const { return type == 2; }
};

struct Relocation {
    uint64_t offset{0};          // RVA corrigido
    uint32_t type{0};            // R_AARCH64_*
    uint32_t symbol{0};          // indice em Symbols()
    int64_t addend{0};           // em DT_RELR: valor ja gravado no local (addend implicito)
    bool plt{false};             // veio de DT_JMPREL
    bool relr{false};            // veio de DT_RELR (relativa compactada; type = kRelocRelative)
};

struct FunctionRange {
    uint64_t start{0};           // RVA
    uint64_t size{0};
};

enum : uint32_t {
    kRelocAbs64 = 257,
    kRelocGlobDat = 1025,
    kRelocJumpSlot = 1026,
    kRelocRelative = 1027,
};

class NsoModule {
public:
    bool Load(const std::string& path, std::string& error);

    const std::string& Path() const { return path_; }
    const Segment& Text() const { return segments_[0]; }
    const Segment& Rodata() const { return segments_[1]; }
    const Segment& Data() const { return segments_[2]; }
    uint32_t BssSize() const { return bss_size_; }
    // Imagem plana: .text, .rodata, .data e .bss zerado
    std::span<const uint8_t> Image() const { return image_; }

    uint64_t Mod0() const { return mod0_; }
    uint64_t Dynamic() const { return dynamic_; }
    uint64_t EhFrameHdr() const { return eh_frame_hdr_; }
    uint64_t EhFrameHdrEnd() const { return eh_frame_hdr_end_; }

    const std::vector<Symbol>& Symbols() const { return symbols_; }
    const std::vector<Relocation>& Relocations() const { return relocations_; }
    // Construtores estaticos (DT_INIT_ARRAY): RVAs das entradas, executadas pelo rtld antes de nnMain
    uint64_t InitArray() const { return init_array_; }
    uint64_t InitArrayCount() const { return init_array_count_; }
    // DT_INIT / DT_FINI: o crt do modulo (roda o INIT_ARRAY); o rtld chama na ordem reversa de carga (0 = ausente)
    uint64_t Init() const { return init_; }
    uint64_t Fini() const { return fini_; }
    // Inicios e tamanhos das funcoes descritas no .eh_frame (ordenados por inicio)
    const std::vector<FunctionRange>& Functions() const { return functions_; }
    const Symbol* FindExport(const std::string& name) const;

private:
    bool ParseDynamic(std::string& error);
    bool ParseEhFrame(std::string& error);

    std::string path_;
    std::array<Segment, 3> segments_{};
    uint32_t bss_size_{0};
    std::vector<uint8_t> image_;
    uint64_t mod0_{0}, dynamic_{0}, eh_frame_hdr_{0}, eh_frame_hdr_end_{0};
    uint64_t dynsym_count_{0};   // do cabecalho NSO (tamanho da dynsym / 24)
    uint64_t init_array_{0}, init_array_count_{0}, init_{0}, fini_{0};
    std::vector<Symbol> symbols_;
    std::vector<Relocation> relocations_;
    std::vector<FunctionRange> functions_;
};

// Descompressao LZ4 de bloco; true apenas se exatamente dst.size() bytes forem produzidos
bool Lz4DecompressBlock(std::span<const uint8_t> src, std::span<uint8_t> dst);
std::array<uint8_t, 32> Sha256(std::span<const uint8_t> data);

} // namespace recomp
