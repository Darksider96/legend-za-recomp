#include "loader/nso_loader.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <windows.h>

namespace za::loader {

NsoLoader::NsoLoader() = default;

NsoLoader::~NsoLoader() {
    if (allocated_memory_) {
        VirtualFree(allocated_memory_, 0, MEM_RELEASE);
        allocated_memory_ = nullptr;
    }
}

// Descompressor LZ4 Bloco autônomo (padrão NSO da Nintendo)
int NsoLoader::DecompressLZ4(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_len) {
    const uint8_t* src_ptr = src;
    const uint8_t* src_end = src + src_len;
    uint8_t* dst_ptr = dst;
    uint8_t* dst_end = dst + dst_len;

    while (src_ptr < src_end) {
        uint8_t token = *src_ptr++;
        size_t literal_length = token >> 4;

        if (literal_length == 15) {
            uint8_t s;
            do {
                if (src_ptr >= src_end) return -1;
                s = *src_ptr++;
                literal_length += s;
            } while (s == 255);
        }

        if (dst_ptr + literal_length > dst_end || src_ptr + literal_length > src_end) {
            return -1;
        }

        std::memcpy(dst_ptr, src_ptr, literal_length);
        dst_ptr += literal_length;
        src_ptr += literal_length;

        if (src_ptr >= src_end) break;

        uint16_t offset = *reinterpret_cast<const uint16_t*>(src_ptr);
        src_ptr += 2;
        if (offset == 0) return -1;

        size_t match_length = (token & 0x0F) + 4;
        if (match_length == 19) {
            uint8_t s;
            do {
                if (src_ptr >= src_end) return -1;
                s = *src_ptr++;
                match_length += s;
            } while (s == 255);
        }

        if (dst_ptr + match_length > dst_end) return -1;

        const uint8_t* match_ptr = dst_ptr - offset;
        if (match_ptr < dst) return -1;

        for (size_t i = 0; i < match_length; ++i) {
            *dst_ptr++ = *match_ptr++;
        }
    }

    return static_cast<int>(dst_ptr - dst);
}

bool NsoLoader::Load(const std::string& filepath, uintptr_t preferred_base) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[NsoLoader] Erro: Nao foi possivel abrir " << filepath << std::endl;
        return false;
    }

    NsoHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (header.magic != 0x304F534E) { // "NSO0"
        std::cerr << "[NsoLoader] Erro: Arquivo nao possui magic NSO0 valido" << std::endl;
        return false;
    }

    std::cout << "[NsoLoader] NSO0 valido detectado. Lendo secoes..." << std::endl;
    std::cout << "  - .text:   file=0x" << std::hex << header.text.file_offset
              << " mem=0x" << header.text.memory_offset 
              << " size=" << std::dec << header.text.decompressed_size / (1024*1024) << " MB" << std::endl;
    std::cout << "  - .rodata: file=0x" << std::hex << header.rodata.file_offset
              << " mem=0x" << header.rodata.memory_offset 
              << " size=" << std::dec << header.rodata.decompressed_size / (1024*1024) << " MB" << std::endl;
    std::cout << "  - .data:   file=0x" << std::hex << header.data.file_offset
              << " mem=0x" << header.data.memory_offset 
              << " size=" << std::dec << header.data.decompressed_size / (1024*1024) << " MB" << std::endl;

    // Calcula a extensão total da imagem de memória (incluindo BSS)
    size_t memory_extent = header.data.memory_offset + header.data.decompressed_size;
    // Alinha para páginas de 64KB
    total_size_ = (memory_extent + 0xFFFF) & ~0xFFFF;

    // Aloca no endereço idêntico ao do Ghidra (0x7100000000)
    allocated_memory_ = VirtualAlloc(reinterpret_cast<void*>(preferred_base), total_size_, 
                                     MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

    if (!allocated_memory_) {
        std::cout << "[NsoLoader] Aviso: Nao foi possivel alocar no endereco preferencial 0x" 
                  << std::hex << preferred_base << ". Alocando dinamicamente..." << std::dec << std::endl;
        allocated_memory_ = VirtualAlloc(nullptr, total_size_, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (!allocated_memory_) {
            std::cerr << "[NsoLoader] Falha fatal de alocacao de memoria virtual" << std::endl;
            return false;
        }
    }

    base_address_ = reinterpret_cast<uintptr_t>(allocated_memory_);
    std::cout << "[NsoLoader] Memoria Virtual Mapeada com Sucesso em: 0x" 
              << std::hex << base_address_ << " (Tamanho: " << std::dec 
              << total_size_ / (1024*1024) << " MB)" << std::endl;

    auto load_section = [&](const NsoSectionHeader& sec, uint32_t comp_size, bool is_compressed, const char* name) -> bool {
        std::cout << "[NsoLoader] Carregando secao " << name << "..." << std::endl;
        uint8_t* dest = reinterpret_cast<uint8_t*>(base_address_) + sec.memory_offset;
        file.seekg(sec.file_offset);

        if (is_compressed) {
            std::vector<uint8_t> comp_buf(comp_size);
            file.read(reinterpret_cast<char*>(comp_buf.data()), comp_size);
            int res = DecompressLZ4(comp_buf.data(), comp_size, dest, sec.decompressed_size);
            if (res < 0) {
                std::cerr << "[NsoLoader] Erro de descompressao LZ4 na secao " << name << std::endl;
                return false;
            }
        } else {
            file.read(reinterpret_cast<char*>(dest), sec.decompressed_size);
        }
        return true;
    };

    bool text_comp = (header.flags & 1) != 0;
    bool rodata_comp = (header.flags & 2) != 0;
    bool data_comp = (header.flags & 4) != 0;

    if (!load_section(header.text, header.text_compressed_size, text_comp, ".text")) return false;
    if (!load_section(header.rodata, header.rodata_compressed_size, rodata_comp, ".rodata")) return false;
    if (!load_section(header.data, header.data_compressed_size, data_comp, ".data")) return false;

    std::cout << "[NsoLoader] Todas as secoes carregadas e descomprimidas na memoria!" << std::endl;
    return true;
}

} // namespace za::loader
