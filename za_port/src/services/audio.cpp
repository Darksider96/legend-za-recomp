#include "audio/wwise.hpp"
#include <iostream>
#include <filesystem>
#include <cstring>

namespace za::audio {

namespace fs_std = std::filesystem;

WwiseArchive& WwiseArchive::Instance() {
    static WwiseArchive s_instance;
    return s_instance;
}

WwiseArchive::~WwiseArchive() {
    Shutdown();
}

void WwiseArchive::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stream_pck_file_.is_open()) {
        stream_pck_file_.close();
    }
    media_map_.clear();
    loaded_banks_.clear();
    initialized_ = false;
}

bool WwiseArchive::MountPck(const std::string& pck_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!fs_std::exists(pck_path)) {
        std::cerr << "[za::audio] Erro: Pacote Wwise nao encontrado: " << pck_path << std::endl;
        return false;
    }

    std::ifstream file(pck_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[za::audio] Erro: Falha ao abrir pacote: " << pck_path << std::endl;
        return false;
    }

    char magic[4];
    file.read(magic, 4);
    if (std::memcmp(magic, "AKPK", 4) != 0) {
        std::cerr << "[za::audio] Erro: Arquivo nao possui assinatura AKPK: " << pck_path << std::endl;
        return false;
    }

    uint32_t header_size = 0;
    uint32_t version = 0;
    uint32_t lang_chunk_size = 0;
    uint32_t bank_chunk_size = 0;
    uint32_t stream_chunk_size = 0;
    uint32_t ext_chunk_size = 0;

    file.read(reinterpret_cast<char*>(&header_size), 4);
    file.read(reinterpret_cast<char*>(&version), 4);
    file.read(reinterpret_cast<char*>(&lang_chunk_size), 4);
    file.read(reinterpret_cast<char*>(&bank_chunk_size), 4);
    file.read(reinterpret_cast<char*>(&stream_chunk_size), 4);
    file.read(reinterpret_cast<char*>(&ext_chunk_size), 4);

    // Salto para a tabela de streams (prefixo de 28 bytes + chunk de idioma + chunk de soundbanks)
    uint64_t stream_table_offset = 28 + static_cast<uint64_t>(lang_chunk_size) + static_cast<uint64_t>(bank_chunk_size);
    file.seekg(stream_table_offset);

    uint32_t stream_count = 0;
    file.read(reinterpret_cast<char*>(&stream_count), 4);

    size_t added_streams = 0;
    for (uint32_t i = 0; i < stream_count; ++i) {
        uint32_t file_id = 0;
        uint32_t block_size = 0;
        uint32_t file_size = 0;
        uint32_t file_offset = 0;
        uint32_t lang_id = 0;

        file.read(reinterpret_cast<char*>(&file_id), 4);
        file.read(reinterpret_cast<char*>(&block_size), 4);
        file.read(reinterpret_cast<char*>(&file_size), 4);
        file.read(reinterpret_cast<char*>(&file_offset), 4);
        file.read(reinterpret_cast<char*>(&lang_id), 4);

        uint64_t abs_offset = static_cast<uint64_t>(file_offset) * (block_size ? block_size : 1);

        WwiseMediaEntry entry;
        entry.file_id = file_id;
        entry.file_offset = abs_offset;
        entry.file_size = file_size;
        entry.language_id = lang_id;
        entry.source_file = pck_path;

        media_map_[file_id] = entry;
        added_streams++;
    }

    stream_pck_path_ = pck_path;
    if (stream_pck_file_.is_open()) stream_pck_file_.close();
    stream_pck_file_.open(stream_pck_path_, std::ios::binary);

    std::cout << "[za::audio] [PCK] Pacote " << fs_std::path(pck_path).filename().string() 
              << " montado com sucesso (" << added_streams << " faixas de audio indexadas)!" << std::endl;

    return true;
}

bool WwiseArchive::LoadBank(const std::string& bnk_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!fs_std::exists(bnk_path)) {
        return false;
    }

    std::ifstream file(bnk_path, std::ios::binary);
    if (!file.is_open()) return false;

    struct DidxEntry {
        uint32_t id;
        uint32_t offset;
        uint32_t size;
    };
    std::vector<DidxEntry> didx_entries;

    while (file.good() && !file.eof()) {
        char tag[4];
        file.read(tag, 4);
        if (file.gcount() < 4) break;

        uint32_t chunk_length = 0;
        file.read(reinterpret_cast<char*>(&chunk_length), 4);
        if (file.gcount() < 4) break;

        uint64_t chunk_start = file.tellg();

        if (std::memcmp(tag, "BKHD", 4) == 0) {
            // Header do SoundBank
            uint32_t bank_version = 0;
            uint32_t soundbank_id = 0;
            file.read(reinterpret_cast<char*>(&bank_version), 4);
            file.read(reinterpret_cast<char*>(&soundbank_id), 4);
        } else if (std::memcmp(tag, "DIDX", 4) == 0) {
            // Data Index
            uint32_t count = chunk_length / 12;
            didx_entries.resize(count);
            for (uint32_t i = 0; i < count; ++i) {
                file.read(reinterpret_cast<char*>(&didx_entries[i].id), 4);
                file.read(reinterpret_cast<char*>(&didx_entries[i].offset), 4);
                file.read(reinterpret_cast<char*>(&didx_entries[i].size), 4);
            }
        } else if (std::memcmp(tag, "DATA", 4) == 0) {
            // Chunk DATA contendo áudios embutidos
            uint64_t data_base = chunk_start;
            for (const auto& didx : didx_entries) {
                WwiseMediaEntry entry;
                entry.file_id = didx.id;
                entry.file_offset = data_base + didx.offset;
                entry.file_size = didx.size;
                entry.language_id = 0;
                entry.source_file = bnk_path;

                media_map_[didx.id] = entry;
            }
        }

        // Pula para o final do chunk
        file.seekg(chunk_start + chunk_length);
    }

    loaded_banks_.push_back(bnk_path);
    return true;
}

bool WwiseArchive::Initialize(const std::string& audio_root_dir) {
    if (initialized_) return true;

    audio_root_ = audio_root_dir;
    if (!fs_std::exists(audio_root_)) {
        // Tenta caminhos alternativos conhecidos
        const char* cands[] = {
            "romfs/audio/wwise/bin/Switch",
            "../romfs/audio/wwise/bin/Switch",
            "C:/Users/johna/OneDrive/Documentos/legend za/romfs/audio/wwise/bin/Switch"
        };
        for (const char* c : cands) {
            if (fs_std::exists(c)) {
                audio_root_ = c;
                break;
            }
        }
    }

    if (!fs_std::exists(audio_root_)) {
        std::cerr << "[za::audio] Aviso: Diretorio raiz do Wwise nao encontrado: " << audio_root_dir << std::endl;
        return false;
    }

    std::cout << "[za::audio] Inicializando Wwise Audio Engine a partir de: " << audio_root_ << std::endl;

    // 1. Monta o pacote de stream principal (Stream.pck de 841 MB)
    std::string stream_pck = (fs_std::path(audio_root_) / "Stream.pck").string();
    if (fs_std::exists(stream_pck)) {
        MountPck(stream_pck);
    }

    // 2. Carrega todos os SoundBanks (.bnk)
    size_t bank_count = 0;
    for (const auto& entry : fs_std::directory_iterator(audio_root_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".bnk") {
            if (LoadBank(entry.path().string())) {
                bank_count++;
            }
        }
    }

    std::cout << "[za::audio] [SUCESSO] Wwise Engine Inicializado: " 
              << bank_count << " SoundBanks carregados, total de " 
              << media_map_.size() << " faixas de audio indexadas!" << std::endl;

    initialized_ = true;
    return true;
}

bool WwiseArchive::HasMedia(uint32_t file_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return media_map_.find(file_id) != media_map_.end();
}

bool WwiseArchive::GetMediaData(uint32_t file_id, std::vector<uint8_t>& out_buffer) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = media_map_.find(file_id);
    if (it == media_map_.end()) {
        return false;
    }

    const auto& entry = it->second;
    std::ifstream file(entry.source_file, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.seekg(entry.file_offset);
    out_buffer.resize(entry.file_size);
    file.read(reinterpret_cast<char*>(out_buffer.data()), entry.file_size);

    return static_cast<size_t>(file.gcount()) == entry.file_size;
}

} // namespace za::audio
