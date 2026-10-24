#include "audio/wwise.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace za::audio {

namespace {

template <typename T>
bool ReadAt(std::span<const uint8_t> data, size_t offset, T& out) {
    if (offset > data.size() || sizeof(T) > data.size() - offset) return false;
    std::memcpy(&out, data.data() + offset, sizeof(T));
    return true;
}

bool ReadFileBytes(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    out.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    return true;
}

// Nome de idioma do AKPK: UTF-16LE no PC, char no Switch (detectado pelo segundo byte nulo)
std::string ReadLanguageName(std::span<const uint8_t> table, size_t offset) {
    std::string name;
    const bool wide = offset + 1 < table.size() && table[offset] != 0 && table[offset + 1] == 0;
    for (size_t p = offset; p < table.size(); p += wide ? 2 : 1) {
        if (table[p] == 0) break;
        name.push_back(static_cast<char>(table[p]));
    }
    return name;
}

} // namespace

bool ParseWemInfo(std::span<const uint8_t> wem, WemInfo& out) {
    if (wem.size() < 12 || std::memcmp(wem.data(), "RIFF", 4) != 0 || std::memcmp(wem.data() + 8, "WAVE", 4) != 0) return false;
    out = {};
    bool has_fmt = false;
    for (size_t pos = 12; pos + 8 <= wem.size();) {
        uint32_t size = 0;
        ReadAt(wem, pos + 4, size);
        const size_t payload = pos + 8;
        if (std::memcmp(wem.data() + pos, "fmt ", 4) == 0 && size >= 8) {
            ReadAt(wem, payload, out.codec);
            ReadAt(wem, payload + 2, out.channels);
            ReadAt(wem, payload + 4, out.sample_rate);
            if (size >= 16) {
                ReadAt(wem, payload + 12, out.block_align);
                ReadAt(wem, payload + 14, out.bits_per_sample);
            }
            has_fmt = true;
        } else if (std::memcmp(wem.data() + pos, "data", 4) == 0) {
            out.data_size = size;
            out.data_offset = static_cast<uint32_t>(payload);
            break;
        }
        pos = payload + size + (size & 1);
    }
    return has_fmt;
}

std::string_view WemCodecName(uint16_t codec) {
    switch (codec) {
    case 0x0001: return "PCM";
    case 0x0002: return "IMA ADPCM";
    case 0xFFFE: return "PCM (extensible)";
    case 0xFFFF: return "Vorbis";
    case 0x3039: return "Opus NX";
    case 0x3040:
    case 0x3041: return "Opus";
    case 0x8311: return "PTADPCM";
    default: return "desconhecido";
    }
}

bool WwiseBank::Load(const std::string& path, std::string& error) {
    std::vector<uint8_t> data;
    if (!ReadFileBytes(path, data)) {
        error = "nao foi possivel ler " + path;
        return false;
    }
    return Parse(std::move(data), error);
}

bool WwiseBank::Parse(std::vector<uint8_t> data, std::string& error) {
    data_ = std::move(data);
    header_ = {};
    chunks_.clear();
    media_.clear();
    index_.clear();
    data_offset_ = data_size_ = 0;
    hirc_count_ = 0;
    hirc_.clear();
    hirc_index_.clear();

    const std::span<const uint8_t> bytes(data_);
    for (size_t pos = 0; pos + 8 <= bytes.size();) {
        uint32_t size = 0;
        ReadAt(bytes, pos + 4, size);
        const size_t payload = pos + 8;
        if (size > bytes.size() - payload) {
            error = "chunk truncado no offset " + std::to_string(pos);
            return false;
        }
        const std::string tag(reinterpret_cast<const char*>(bytes.data() + pos), 4);
        chunks_.push_back({tag, payload, size});

        if (tag == "BKHD" && size >= 16) {
            ReadAt(bytes, payload, header_.version);
            ReadAt(bytes, payload + 4, header_.soundbank_id);
            ReadAt(bytes, payload + 8, header_.alt_soundbank_id);
            ReadAt(bytes, payload + 12, header_.flags);
        } else if (tag == "DIDX") {
            for (size_t e = payload; e + 12 <= payload + size; e += 12) {
                uint32_t id = 0, offset = 0, length = 0;
                ReadAt(bytes, e, id);
                ReadAt(bytes, e + 4, offset);
                ReadAt(bytes, e + 8, length);
                index_.emplace(id, media_.size());
                media_.push_back({id, offset, length, 0});
            }
        } else if (tag == "DATA") {
            data_offset_ = payload;
            data_size_ = size;
        } else if (tag == "HIRC" && size >= 4) {
            ReadAt(bytes, payload, hirc_count_);
            const size_t end = payload + size;
            size_t object = payload + 4;
            for (uint32_t i = 0; i < hirc_count_; ++i) {
                uint32_t object_size = 0, id = 0;
                if (object + 9 > end || !ReadAt(bytes, object + 1, object_size) || object_size < 4 ||
                    object_size > end - object - 5) {
                    error = "objeto HIRC " + std::to_string(i) + " truncado";
                    return false;
                }
                ReadAt(bytes, object + 5, id);
                hirc_index_[id] = hirc_.size();
                hirc_.push_back({bytes[object], id, object + 9, object_size - 4});
                object += 5 + size_t{object_size};
            }
        }
        pos = payload + size;
    }

    if (chunks_.empty() || chunks_.front().tag != "BKHD") {
        error = "SoundBank sem chunk BKHD";
        return false;
    }
    if (!media_.empty() && data_offset_ == 0) {
        error = "DIDX sem chunk DATA";
        return false;
    }
    for (const WwiseMedia& media : media_) {
        if (media.offset > data_size_ || media.size > data_size_ - media.offset) {
            error = "midia " + std::to_string(media.id) + " fora do chunk DATA";
            return false;
        }
    }
    return true;
}

std::span<const uint8_t> WwiseBank::MediaData(uint32_t media_id) const {
    const auto it = index_.find(media_id);
    return it == index_.end() ? std::span<const uint8_t>() : MediaData(media_[it->second]);
}

std::span<const uint8_t> WwiseBank::MediaData(const WwiseMedia& media) const {
    if (media.offset > data_size_ || media.size > data_size_ - media.offset) return {};
    return std::span<const uint8_t>(data_).subspan(data_offset_ + media.offset, media.size);
}

const HircObject* WwiseBank::FindObject(uint32_t id) const {
    const auto it = hirc_index_.find(id);
    return it == hirc_index_.end() ? nullptr : &hirc_[it->second];
}

bool WwisePackage::Open(const std::string& path, std::string& error) {
    std::lock_guard<std::mutex> lock(read_mutex_);
    file_.close();
    file_.clear();
    languages_.clear();
    banks_.clear();
    streams_.clear();
    externals_.clear();
    index_.clear();

    file_.open(path, std::ios::binary | std::ios::ate);
    if (!file_) {
        error = "nao foi possivel abrir " + path;
        return false;
    }
    file_size_ = static_cast<uint64_t>(file_.tellg());
    file_.seekg(0);

    char magic[4] = {};
    file_.read(magic, 4);
    file_.read(reinterpret_cast<char*>(&header_size_), 4);
    if (!file_ || std::memcmp(magic, "AKPK", 4) != 0) {
        error = "assinatura AKPK ausente";
        return false;
    }
    if (header_size_ < 16 || 8ull + header_size_ > file_size_) {
        error = "header_size invalido: " + std::to_string(header_size_);
        return false;
    }
    std::vector<uint8_t> header(header_size_);
    file_.read(reinterpret_cast<char*>(header.data()), header_size_);
    if (!file_) {
        error = "cabecalho AKPK truncado";
        return false;
    }
    const std::span<const uint8_t> bytes(header);

    uint32_t lang_size = 0, bank_size = 0, stream_size = 0, ext_size = 0;
    ReadAt(bytes, 0, version_);
    ReadAt(bytes, 4, lang_size);
    ReadAt(bytes, 8, bank_size);
    ReadAt(bytes, 12, stream_size);
    const uint64_t tables = uint64_t{lang_size} + bank_size + stream_size;
    size_t pos = 16;
    // A tabela de externos (ids de 64 bits) so existe quando o header tem o quinto tamanho
    if (ReadAt(bytes, 16, ext_size) && 20 + tables + ext_size == header_size_) {
        pos = 20;
    } else {
        ext_size = 0;
    }
    if (pos + tables + ext_size > header_size_) {
        error = "tamanhos das tabelas AKPK excedem o cabecalho";
        return false;
    }

    // Idiomas: offsets dos nomes sao relativos ao inicio da propria secao
    const std::span<const uint8_t> lang = bytes.subspan(pos, lang_size);
    uint32_t lang_count = 0;
    ReadAt(lang, 0, lang_count);
    for (uint32_t i = 0; i < lang_count; ++i) {
        uint32_t name_offset = 0, id = 0;
        if (!ReadAt(lang, 4 + 8 * size_t{i}, name_offset) || !ReadAt(lang, 8 + 8 * size_t{i}, id)) break;
        languages_.push_back({id, ReadLanguageName(lang, name_offset)});
    }
    pos += lang_size;

    const auto read_table = [&](size_t offset, uint32_t size, bool wide_id, std::vector<WwiseMedia>& out) {
        const std::span<const uint8_t> table = bytes.subspan(offset, size);
        const size_t entry_size = wide_id ? 24 : 20;
        uint32_t count = 0;
        if (size == 0) return true;
        if (!ReadAt(table, 0, count) || 4 + uint64_t{count} * entry_size > size) return false;
        out.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            const size_t e = 4 + i * entry_size;
            WwiseMedia media;
            uint32_t id32 = 0, block_size = 0, start_block = 0;
            if (wide_id) {
                ReadAt(table, e, media.id);
            } else {
                ReadAt(table, e, id32);
                media.id = id32;
            }
            const size_t f = e + (wide_id ? 8 : 4);
            ReadAt(table, f, block_size);
            ReadAt(table, f + 4, media.size);
            ReadAt(table, f + 8, start_block);
            ReadAt(table, f + 12, media.language_id);
            media.offset = uint64_t{start_block} * (block_size ? block_size : 1);
            if (media.offset > file_size_ || media.size > file_size_ - media.offset) return false;
            out.push_back(media);
        }
        return true;
    };
    if (!read_table(pos, bank_size, false, banks_) || !read_table(pos + bank_size, stream_size, false, streams_) ||
        !read_table(pos + bank_size + stream_size, ext_size, true, externals_)) {
        error = "tabela AKPK truncada ou com entrada fora do arquivo";
        return false;
    }

    for (const auto* table : {&externals_, &banks_, &streams_}) {
        for (const WwiseMedia& media : *table) index_[media.id] = &media;
    }
    return true;
}

const WwiseMedia* WwisePackage::Find(uint64_t id) const {
    const auto it = index_.find(id);
    return it == index_.end() ? nullptr : it->second;
}

bool WwisePackage::Read(const WwiseMedia& media, std::vector<uint8_t>& out, std::string& error, size_t max_bytes) const {
    std::lock_guard<std::mutex> lock(read_mutex_);
    if (!file_.is_open() || media.offset > file_size_ || media.size > file_size_ - media.offset) {
        error = "midia fora do pacote";
        return false;
    }
    out.resize(std::min<size_t>(media.size, max_bytes));
    file_.clear();
    file_.seekg(static_cast<std::streamoff>(media.offset));
    file_.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    if (!file_) {
        error = "leitura truncada da midia " + std::to_string(media.id);
        return false;
    }
    return true;
}

namespace {

constexpr int kMaxHierarchyDepth = 32;

bool IsPlayable(uint8_t type) {
    return type == static_cast<uint8_t>(HircType::Sound) || type == static_cast<uint8_t>(HircType::RandomSequence) ||
           type == static_cast<uint8_t>(HircType::Switch);
}

// DirectParentID fica no inicio dos NodeBaseParams: offset 9 em containers, ~23 em Sounds
bool ReferencesParent(std::span<const uint8_t> body, uint32_t parent_id) {
    const size_t limit = std::min<size_t>(body.size(), 64);
    for (size_t i = 0; i + 4 <= limit; ++i) {
        uint32_t value = 0;
        ReadAt(body, i, value);
        if (value == parent_id) return true;
    }
    return false;
}

struct ChildList {
    size_t count_offset{0};
    std::vector<uint32_t> ids;
};

// Primeiro "u32 n + n ids" apos o DirectParentID do proprio container cujos objetos apontam de volta para ele
bool FindChildren(const WwiseBank& bank, const HircObject& container, ChildList& out) {
    const std::span<const uint8_t> body = bank.Body(container);
    for (size_t pos = 13; pos + 8 <= body.size(); ++pos) {
        uint32_t count = 0;
        ReadAt(body, pos, count);
        if (count == 0 || count > 4096 || 4ull * count > body.size() - pos - 4) continue;
        std::vector<uint32_t> ids(count);
        bool ok = true;
        for (uint32_t k = 0; ok && k < count; ++k) {
            ReadAt(body, pos + 4 + 4 * size_t{k}, ids[k]);
            const HircObject* child = bank.FindObject(ids[k]);
            ok = child && child->id != container.id && IsPlayable(child->type) && ReferencesParent(bank.Body(*child), container.id);
        }
        if (ok) {
            out = {pos, std::move(ids)};
            return true;
        }
    }
    return false;
}

// Switch: no apontado pelo switch padrao (u32 cinco bytes antes da contagem de filhos); senao o primeiro filho
uint32_t PickChild(const WwiseBank& bank, const HircObject& container, const ChildList& children) {
    if (container.type == static_cast<uint8_t>(HircType::Switch)) {
        const std::span<const uint8_t> body = bank.Body(container);
        uint32_t default_switch = 0, groups = 0;
        size_t pos = children.count_offset + 4 + 4 * children.ids.size();
        if (ReadAt(body, children.count_offset - 5, default_switch) && ReadAt(body, pos, groups)) {
            pos += 4;
            for (uint32_t g = 0; g < groups && g < 4096; ++g) {
                uint32_t switch_id = 0, items = 0, first = 0;
                if (!ReadAt(body, pos, switch_id) || !ReadAt(body, pos + 4, items) || items > 4096) break;
                if (switch_id == default_switch && items > 0 && ReadAt(body, pos + 8, first) &&
                    std::find(children.ids.begin(), children.ids.end(), first) != children.ids.end()) {
                    return first;
                }
                pos += 8 + 4 * size_t{items};
            }
        }
    }
    return children.ids.front();
}

} // namespace

uint32_t WwiseHash(std::string_view name) {
    uint32_t hash = 2166136261u;
    for (const char c : name) {
        hash *= 16777619u;
        hash ^= static_cast<uint8_t>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    }
    return hash;
}

bool ResolveEventSound(const WwiseBank& bank, uint32_t event_id, WwiseSoundSource& out, std::string& error) {
    out = {};
    const HircObject* event = bank.FindObject(event_id);
    if (!event || event->type != static_cast<uint8_t>(HircType::Event)) {
        error = "evento " + std::to_string(event_id) + " ausente do HIRC";
        return false;
    }
    // Contagem de acoes em inteiro variavel (7 bits por byte)
    const std::span<const uint8_t> body = bank.Body(*event);
    uint32_t action_count = 0;
    size_t pos = 0;
    for (int shift = 0; pos < body.size() && shift < 28; shift += 7) {
        const uint8_t b = body[pos++];
        action_count |= uint32_t{b & 0x7Fu} << shift;
        if (!(b & 0x80)) break;
    }

    uint32_t node = 0;
    for (uint32_t a = 0; a < action_count && node == 0; ++a) {
        uint32_t action_id = 0;
        if (!ReadAt(body, pos + 4 * size_t{a}, action_id)) break;
        const HircObject* action = bank.FindObject(action_id);
        uint16_t action_type = 0;
        uint32_t target = 0;
        if (action && action->type == static_cast<uint8_t>(HircType::Action) && ReadAt(bank.Body(*action), 0, action_type) &&
            (action_type >> 8) == 0x04 && ReadAt(bank.Body(*action), 2, target)) {
            node = target;  // Play
        }
    }
    if (node == 0) {
        error = "evento " + std::to_string(event_id) + " sem acao Play";
        return false;
    }

    for (int depth = 0; depth < kMaxHierarchyDepth; ++depth) {
        const HircObject* object = bank.FindObject(node);
        if (!object) {
            error = "objeto " + std::to_string(node) + " ausente do HIRC";
            return false;
        }
        if (object->type == static_cast<uint8_t>(HircType::Sound)) {
            const std::span<const uint8_t> sound = bank.Body(*object);
            out.sound_id = object->id;
            if (!ReadAt(sound, 0, out.plugin_id) || !ReadAt(sound, 4, out.stream_type) || !ReadAt(sound, 5, out.media_id)) {
                error = "Sound " + std::to_string(node) + " truncado";
                return false;
            }
            return true;
        }
        ChildList children;
        if (!IsPlayable(object->type) || !FindChildren(bank, *object, children)) {
            error = "objeto " + std::to_string(node) + " (tipo " + std::to_string(object->type) + ") sem filhos reproduziveis";
            return false;
        }
        node = PickChild(bank, *object, children);
    }
    error = "hierarquia do evento " + std::to_string(event_id) + " profunda demais";
    return false;
}

bool ResolvePokemonCry(const WwiseBank& voice_bank, uint16_t species_id, WwiseSoundSource& out, std::string& error) {
    char name[24];
    std::snprintf(name, sizeof(name), "PLAY_PV_%04hu", species_id);
    if (!ResolveEventSound(voice_bank, WwiseHash(name), out, error)) {
        error = std::string(name) + ": " + error;
        return false;
    }
    return true;
}

bool ResolvePokemonCryMediaId(const WwiseBank& voice_bank, uint16_t species_id, uint32_t& out_media_id, std::string& error) {
    WwiseSoundSource source;
    if (!ResolvePokemonCry(voice_bank, species_id, source, error)) return false;
    out_media_id = source.media_id;
    return true;
}

} // namespace za::audio
