#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <span>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <fstream>
#include <memory>

namespace za::audio {

struct WwiseMediaEntry {
    uint32_t file_id{0};
    uint64_t file_offset{0};
    uint32_t file_size{0};
    uint32_t language_id{0};
    std::string source_file; // Caminho do .pck ou .bnk
};

struct WwiseBankHeader {
    uint32_t version{0};
    uint32_t soundbank_id{0};
    uint32_t alt_soundbank_id{0};
    uint32_t flags{0};
};

class WwiseArchive {
public:
    static WwiseArchive& Instance();

    WwiseArchive() = default;
    ~WwiseArchive();

    WwiseArchive(const WwiseArchive&) = delete;
    WwiseArchive& operator=(const WwiseArchive&) = delete;

    // Inicializa carregando a pasta raiz do Wwise e montando Stream.pck e soundbanks essenciais
    bool Initialize(const std::string& audio_root_dir);

    // Carrega um pacote de áudio (.pck) com tabela AKPK
    bool MountPck(const std::string& pck_path);

    // Carrega e indexa um SoundBank (.bnk) com chunks BKHD, DIDX, DATA
    bool LoadBank(const std::string& bnk_path);

    // Consulta e extração de áudio WEM / RIFF
    bool HasMedia(uint32_t file_id) const;
    bool GetMediaData(uint32_t file_id, std::vector<uint8_t>& out_buffer);

    [[nodiscard]] size_t GetTotalMediaCount() const noexcept { return media_map_.size(); }
    [[nodiscard]] size_t GetLoadedBankCount() const noexcept { return loaded_banks_.size(); }

    void Shutdown();

private:
    std::string audio_root_;
    std::string stream_pck_path_;
    std::ifstream stream_pck_file_;
    mutable std::mutex mutex_;

    // file_id -> entrada de mídia
    std::unordered_map<uint32_t, WwiseMediaEntry> media_map_;
    std::vector<std::string> loaded_banks_;
    bool initialized_{false};
};

// ---------------------------------------------------------------------------------------------------------------
// Leitores de baixo nivel (src/audio/wwise.cpp), sem estado global — usados pelo trpfs_tool e pelos testes
//
// .bnk: sequencia de chunks { char tag[4]; u32 tamanho; payload }
//   BKHD { u32 versao (0x91 = 145); u32 soundbank_id; u32 language_id; u32 flags; ... }
//   DIDX [{ u32 media_id; u32 offset (relativo ao payload de DATA); u32 tamanho }]
//   DATA .wem concatenados; HIRC { u32 contagem; objetos }; INIT/STMG/ENVS/PLAT so no Init.bnk
// .pck (AKPK v1): "AKPK" u32 header_size (bytes apos estes 8) u32 versao
//   u32 tam_idiomas u32 tam_banks u32 tam_streams [u32 tam_externos]
//   idiomas { u32 n; [{u32 offset_nome; u32 id}]; nomes (char no Switch, UTF-16 no PC) }
//   banks / streams { u32 n; [{u32 id; u32 block_size; u32 tamanho; u32 bloco_inicial; u32 idioma}] }
//   externos (opcional) { u32 n; [{u64 id; u32 block_size; u32 tamanho; u32 bloco_inicial; u32 idioma}] }
//   offset absoluto = bloco_inicial * block_size
// .wem: RIFF/WAVE; codigo do "fmt ": 0x0001 PCM, 0x0002 IMA ADPCM, 0xFFFF Vorbis, 0x3039 Opus NX,
//   0x3040/0x3041 Opus, 0x8311 PTADPCM (no Stream.pck do Z-A: 5344 PTADPCM, 752 Vorbis, 182 Opus NX)

struct WwiseMedia {
    uint64_t id{0};
    uint64_t offset{0};      // .bnk: relativo ao payload de DATA; .pck: absoluto no arquivo
    uint32_t size{0};
    uint32_t language_id{0};
};

struct WemInfo {
    uint16_t codec{0};
    uint16_t channels{0};
    uint32_t sample_rate{0};
    uint32_t data_size{0};   // bytes do chunk "data"
    uint16_t block_align{0};
    uint16_t bits_per_sample{0};
    uint32_t data_offset{0}; // inicio do payload de "data" no .wem
};

// Le o cabecalho RIFF/WAVE e o chunk "fmt " de um .wem
bool ParseWemInfo(std::span<const uint8_t> wem, WemInfo& out);
std::string_view WemCodecName(uint16_t codec);
// PCM linear (0x0001) ou WAVE_FORMAT_EXTENSIBLE (0xFFFE) ja e um .wav valido
inline bool IsPcmCodec(uint16_t codec) { return codec == 0x0001 || codec == 0xFFFE; }

// Objeto do HIRC: { u8 tipo; u32 tamanho (id + corpo); u32 id; corpo }
// Tipos (v145): 2 Sound, 3 Action, 4 Event, 5 RanSeqCntr, 6 SwitchCntr, 7 ActorMixer
enum class HircType : uint8_t { Sound = 2, Action = 3, Event = 4, RandomSequence = 5, Switch = 6, ActorMixer = 7 };

struct HircObject {
    uint8_t type{0};
    uint32_t id{0};
    size_t body_offset{0};   // corpo (apos o id) no arquivo do banco; bytes via WwiseBank::Body
    uint32_t body_size{0};
};

class WwiseBank {
public:
    struct Chunk {
        std::string tag;
        size_t offset{0};    // payload
        uint32_t size{0};
    };

    bool Load(const std::string& path, std::string& error);
    bool Parse(std::vector<uint8_t> data, std::string& error);

    const WwiseBankHeader& Header() const { return header_; }
    const std::vector<Chunk>& Chunks() const { return chunks_; }
    const std::vector<WwiseMedia>& Media() const { return media_; }   // ordem do DIDX
    uint32_t HircObjectCount() const { return hirc_count_; }
    const std::vector<HircObject>& HircObjects() const { return hirc_; }
    const HircObject* FindObject(uint32_t id) const;
    std::span<const uint8_t> Body(const HircObject& object) const {
        return std::span<const uint8_t>(data_).subspan(object.body_offset, object.body_size);
    }

    // Bytes do .wem dentro do chunk DATA; vazio se o id nao existe
    std::span<const uint8_t> MediaData(uint32_t media_id) const;
    std::span<const uint8_t> MediaData(const WwiseMedia& media) const;

private:
    std::vector<uint8_t> data_;
    WwiseBankHeader header_;
    std::vector<Chunk> chunks_;
    std::vector<WwiseMedia> media_;
    std::unordered_map<uint32_t, size_t> index_;
    std::vector<HircObject> hirc_;
    std::unordered_map<uint32_t, size_t> hirc_index_;
    size_t data_offset_{0};
    size_t data_size_{0};
    uint32_t hirc_count_{0};
};

// Indice do .pck em memoria; o conteudo e lido sob demanda. Read e const e serializado por mutex, entao varios
// leitores (ex.: streams de ambiencia) podem compartilhar o mesmo pacote; a classe nao e copiavel nem movel
class WwisePackage {
public:
    struct Language {
        uint32_t id{0};
        std::string name;
    };

    bool Open(const std::string& path, std::string& error);

    uint32_t Version() const { return version_; }
    uint32_t HeaderSize() const { return header_size_; }
    uint64_t FileSize() const { return file_size_; }
    const std::vector<Language>& Languages() const { return languages_; }
    const std::vector<WwiseMedia>& Banks() const { return banks_; }
    const std::vector<WwiseMedia>& Streams() const { return streams_; }
    const std::vector<WwiseMedia>& Externals() const { return externals_; }

    // Procura nos streams, depois nos banks e externos
    const WwiseMedia* Find(uint64_t id) const;
    // max_bytes limita a leitura ao inicio da midia (ex.: so o cabecalho RIFF)
    bool Read(const WwiseMedia& media, std::vector<uint8_t>& out, std::string& error, size_t max_bytes = SIZE_MAX) const;

private:
    mutable std::mutex read_mutex_;
    mutable std::ifstream file_;
    uint64_t file_size_{0};
    uint32_t header_size_{0};
    uint32_t version_{0};
    std::vector<Language> languages_;
    std::vector<WwiseMedia> banks_;
    std::vector<WwiseMedia> streams_;
    std::vector<WwiseMedia> externals_;
    std::unordered_map<uint64_t, const WwiseMedia*> index_;
};

// ---------------------------------------------------------------------------------------------------------------
// Eventos -> Sound. Ids de eventos/switches sao FNV-1 32 bits do nome em minusculas (WwiseHash).
// Event { var n; u32 acoes[n] } -> Action { u16 tipo (0x0403 = Play); u32 alvo } -> containers -> Sound
//   Sound { u32 plugin_id; u8 stream_type (0 = DATA do banco, 2 = Stream.pck); u32 source_media_id; ... }
//   SwitchCntr (depois dos NodeBaseParams): u8 tipo_grupo; u32 grupo; u32 switch_padrao; u8 continuo;
//     u32 n; u32 filhos[n]; u32 n_grupos; [{u32 switch; u32 n; u32 nos[n]}]; ...
// Os NodeBaseParams tem tamanho variavel: a lista de filhos e localizada como o primeiro "u32 n + n ids"
// cujos objetos apontam de volta para o container (DirectParentID). O caminho segue o switch padrao —
// o que o jogo toca sem estado definido — e o primeiro filho nos RanSeqCntr.
//
// Cries: evento "PLAY_PV_%04hu" (id interno da especie) no PM_VOICE.bnk. Os 235 eventos do Z-A resolvem
// para faixas PTADPCM mono no Stream.pck (Switch -> [Switch de forma] -> Sound).

struct WwiseSoundSource {
    uint32_t sound_id{0};
    uint32_t plugin_id{0};
    uint8_t stream_type{0};
    uint32_t media_id{0};

    bool InStreamPackage() const { return stream_type == 2; }
};

uint32_t WwiseHash(std::string_view name);
bool ResolveEventSound(const WwiseBank& bank, uint32_t event_id, WwiseSoundSource& out, std::string& error);
bool ResolvePokemonCry(const WwiseBank& voice_bank, uint16_t species_id, WwiseSoundSource& out, std::string& error);
bool ResolvePokemonCryMediaId(const WwiseBank& voice_bank, uint16_t species_id, uint32_t& out_media_id, std::string& error);

} // namespace za::audio
