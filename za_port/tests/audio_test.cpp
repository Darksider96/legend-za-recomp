// Testes do leitor Wwise: SoundBanks (.bnk), pacote de streaming (.pck), cabecalho .wem, cries e decodificador PTADPCM
// uso: audio_test <romfs_dir>   (os testes de integracao leem romfs/audio/wwise/bin/Switch)

#include "audio/ambient_stream.hpp"
#include "audio/wem_decoder.hpp"
#include "audio/wwise.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

using namespace za;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    std::printf("%s %s\n", condition ? "[PASS]" : "[FAIL]", what);
    if (!condition) ++g_failures;
}

void Put32(std::vector<uint8_t>& out, uint32_t value) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(value >> (8 * i)));
}

void PutTag(std::vector<uint8_t>& out, const char* tag) { out.insert(out.end(), tag, tag + 4); }

// .wem minimo: RIFF/WAVE com fmt (codec, 2 canais, 48 kHz) e data de 4 bytes
std::vector<uint8_t> MakeWem(uint16_t codec) {
    std::vector<uint8_t> wem;
    PutTag(wem, "RIFF");
    Put32(wem, 4 + 24 + 12);
    PutTag(wem, "WAVE");
    PutTag(wem, "fmt ");
    Put32(wem, 16);
    wem.push_back(static_cast<uint8_t>(codec));
    wem.push_back(static_cast<uint8_t>(codec >> 8));
    wem.push_back(2), wem.push_back(0);
    Put32(wem, 48000);
    Put32(wem, 192000);
    Put32(wem, 0x00100004);
    PutTag(wem, "data");
    Put32(wem, 4);
    Put32(wem, 0x11223344);
    return wem;
}

void TestSynthetic() {
    audio::WemInfo info;
    const std::vector<uint8_t> pcm = MakeWem(0x0001);
    Check(audio::ParseWemInfo(pcm, info) && info.codec == 1 && info.channels == 2 && info.sample_rate == 48000 &&
              info.data_size == 4 && audio::IsPcmCodec(info.codec), "ParseWemInfo: PCM 2 ch 48 kHz, data de 4 bytes");
    Check(audio::WemCodecName(0x8311) == "PTADPCM" && audio::WemCodecName(0xFFFF) == "Vorbis" && !audio::IsPcmCodec(0xFFFF),
          "nomes de codec Wwise (PTADPCM, Vorbis)");
    std::vector<uint8_t> broken = pcm;
    broken.at(8) = 'X';  // "WAVE" -> "XAVE"
    Check(!audio::ParseWemInfo(broken, info), "ParseWemInfo rejeita RIFF que nao e WAVE");

    // BKHD + DIDX (2 midias) + DATA
    const std::vector<uint8_t> a = MakeWem(0x0001), b = MakeWem(0xFFFF);
    std::vector<uint8_t> bank;
    PutTag(bank, "BKHD");
    Put32(bank, 16);
    Put32(bank, 145), Put32(bank, 1234), Put32(bank, 0), Put32(bank, 16);
    PutTag(bank, "DIDX");
    Put32(bank, 24);
    Put32(bank, 7), Put32(bank, 0), Put32(bank, static_cast<uint32_t>(a.size()));
    Put32(bank, 9), Put32(bank, static_cast<uint32_t>(a.size())), Put32(bank, static_cast<uint32_t>(b.size()));
    PutTag(bank, "DATA");
    Put32(bank, static_cast<uint32_t>(a.size() + b.size()));
    bank.insert(bank.end(), a.begin(), a.end());
    bank.insert(bank.end(), b.begin(), b.end());

    audio::WwiseBank parsed;
    std::string error;
    const bool ok = parsed.Parse(bank, error);
    const auto second = parsed.MediaData(9);
    Check(ok && parsed.Header().version == 145 && parsed.Header().soundbank_id == 1234 && parsed.Media().size() == 2 &&
              second.size() == b.size() && std::equal(second.begin(), second.end(), b.begin()) && parsed.MediaData(8).empty(),
          "WwiseBank sintetico: BKHD, DIDX e media_id -> span do DATA");

    std::vector<uint8_t> truncated = bank;
    truncated.resize(truncated.size() - 10);
    Check(!parsed.Parse(truncated, error), "WwiseBank rejeita chunk DATA truncado");

    // AKPK com 1 idioma ("sfx") e 1 stream de bloco 16 apos o cabecalho
    std::vector<uint8_t> pck;
    PutTag(pck, "AKPK");
    Put32(pck, 0);  // header_size, preenchido abaixo
    Put32(pck, 1);
    Put32(pck, 16), Put32(pck, 4), Put32(pck, 24), Put32(pck, 4);
    Put32(pck, 1), Put32(pck, 12), Put32(pck, 0);
    PutTag(pck, "sfx\0");
    Put32(pck, 0);                                   // banks
    Put32(pck, 1);                                   // streams
    Put32(pck, 4242), Put32(pck, 16), Put32(pck, static_cast<uint32_t>(a.size())), Put32(pck, 5), Put32(pck, 0);
    Put32(pck, 0);                                   // externos
    const uint32_t header_size = static_cast<uint32_t>(pck.size() - 8);
    std::memcpy(pck.data() + 4, &header_size, 4);
    pck.resize(80, 0);
    pck.insert(pck.end(), a.begin(), a.end());

    const auto path = std::filesystem::temp_directory_path() / "za_audio_test.pck";
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(pck.data()), static_cast<std::streamsize>(pck.size()));
    audio::WwisePackage package;
    std::vector<uint8_t> read;
    const bool opened = package.Open(path.string(), error);
    const audio::WwiseMedia* media = opened ? package.Find(4242) : nullptr;
    Check(opened && package.Languages().size() == 1 && package.Languages().front().name == "sfx" && media &&
              media->offset == 80 && package.Read(*media, read, error) && read == a,
          "WwisePackage sintetico: idioma, offset = bloco x block_size e leitura sob demanda");
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

void TestRomfs(const std::filesystem::path& romfs) {
    const std::filesystem::path dir = romfs / "audio" / "wwise" / "bin" / "Switch";
    std::string error;

    audio::WwiseBank init;
    const bool init_ok = init.Load((dir / "Init.bnk").string(), error);
    Check(init_ok && init.Header().version == 145 && init.Media().empty() && init.Chunks().size() == 6 &&
              init.Chunks()[1].tag == "INIT", "Init.bnk: BKHD versao 145 (0x91), chunks INIT/STMG/HIRC sem midia");

    audio::WwiseBank pm;
    const bool pm_ok = pm.Load((dir / "PM.bnk").string(), error);
    Check(pm_ok && pm.Header().version == 145 && pm.Media().size() == 1749, "PM.bnk: exatamente 1749 midias no DIDX");
    bool all_riff = pm_ok && !pm.Media().empty();
    for (const audio::WwiseMedia& media : pm.Media()) {
        const auto wem = pm.MediaData(media);
        all_riff = all_riff && wem.size() == media.size && wem.size() >= 12 && std::memcmp(wem.data(), "RIFF", 4) == 0 &&
                   std::memcmp(wem.data() + 8, "WAVE", 4) == 0;
    }
    audio::WemInfo info;
    const auto first = pm_ok ? pm.MediaData(0x000EAE7Au) : std::span<const uint8_t>();
    Check(first.size() == 0x1895 && std::memcmp(first.data(), "RIFF", 4) == 0 && std::memcmp(first.data() + 8, "WAVE", 4) == 0 &&
              audio::ParseWemInfo(first, info) && info.codec == 0xFFFF,
          "PM.bnk: primeiro WEM (0x000eae7a, 6293 bytes) e RIFF....WAVE Vorbis");
    Check(all_riff, "PM.bnk: todas as 1749 midias dentro de DATA com assinatura RIFF/WAVE");

    audio::WwisePackage stream;
    const bool pck_ok = stream.Open((dir / "Stream.pck").string(), error);
    Check(pck_ok && stream.Version() == 1 && stream.HeaderSize() == 125608 && stream.Streams().size() == 6278 &&
              stream.Banks().empty() && stream.Languages().size() == 1 && stream.Languages().front().name == "sfx",
          "Stream.pck: AKPK v1, cabecalho de 125608 bytes, exatamente 6278 faixas");

    // Offsets: entradas ordenadas por id, sem sobreposicao, apos o cabecalho e terminando no fim do arquivo
    std::vector<audio::WwiseMedia> sorted = pck_ok ? stream.Streams() : std::vector<audio::WwiseMedia>{};
    std::sort(sorted.begin(), sorted.end(), [](const auto& l, const auto& r) { return l.offset < r.offset; });
    bool layout_ok = !sorted.empty() && sorted.front().offset >= 8 + stream.HeaderSize();
    for (size_t i = 1; i < sorted.size(); ++i) layout_ok = layout_ok && sorted[i - 1].offset + sorted[i - 1].size <= sorted[i].offset;
    layout_ok = layout_ok && sorted.back().offset + sorted.back().size == stream.FileSize();
    Check(layout_ok, "Stream.pck: faixas sem sobreposicao, a ultima termina no fim do arquivo");

    const audio::WwiseMedia* track = pck_ok ? stream.Find(44988) : nullptr;
    std::vector<uint8_t> wem;
    Check(track && track->offset == 812578877 && track->size == 10320 && stream.Read(*track, wem, error) && wem.size() == 10320 &&
              audio::ParseWemInfo(wem, info) && std::memcmp(wem.data(), "RIFF", 4) == 0,
          "Stream.pck: faixa 44988 em offset 812578877 (10320 bytes) lida como RIFF/WAVE");

    size_t riff = 0;
    std::vector<uint8_t> header;
    for (const audio::WwiseMedia& media : pck_ok ? stream.Streams() : std::vector<audio::WwiseMedia>{}) {
        riff += stream.Read(media, header, error, 64) && audio::ParseWemInfo(header, info) ? 1 : 0;
    }
    Check(riff == 6278, "Stream.pck: as 6278 faixas comecam com cabecalho RIFF/WAVE valido");
}

uint32_t Crc32(std::span<const uint8_t> data) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint8_t b : data) {
        crc ^= b;
        for (int i = 0; i < 8; ++i) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

void TestDecoderSynthetic() {
    // Quadro PTADPCM de 0x24 bytes: hist2 = 100, hist1 = 200, indice 0. Nibble 7 (passo 0) extrapola a reta
    // (300, 400, ...); nibble 15 soma 14 e leva ao indice 2, onde o nibble 8 soma +2
    std::vector<uint8_t> frame(0x24, 0x77);
    frame[0] = 100, frame[1] = 0, frame[2] = 200, frame[3] = 0, frame[4] = 0;
    frame[5] = 0x77, frame[6] = 0x8F;  // amostras 2, 3 = nibble 7; amostra 4 = 15, amostra 5 = 8
    std::vector<int16_t> pcm;
    std::string error;
    const bool ok = audio::DecodePtadpcm(frame, 1, 0x24, pcm, error);
    // 2: 300, 3: 400, 4: 14 + 800 - 300 = 514, 5 (indice 2, nibble 8 = +2): 2 + 1028 - 400 = 630
    Check(ok && pcm.size() == 64 && pcm[0] == 100 && pcm[1] == 200 && pcm[2] == 300 && pcm[3] == 400 && pcm[4] == 514 &&
              pcm[5] == 630, "DecodePtadpcm: 64 amostras por quadro de 0x24, historico e tabela de passos");

    audio::PcmBuffer mono{32000, 1, {0, 3200, -3200, 0}};
    const audio::PcmBuffer stereo = audio::ResampleLinear(mono, 48000, 2);
    Check(stereo.sample_rate == 48000 && stereo.channels == 2 && stereo.FrameCount() == 6 && stereo.samples[0] == 0 &&
              stereo.samples[1] == 0 && stereo.samples[2] == 2133 && stereo.samples[3] == 2133 && stereo.samples[4] == 1067,
          "ResampleLinear 32 kHz mono -> 48 kHz estereo (interpolacao linear, canais duplicados)");

    const std::vector<uint8_t> wav = audio::BuildWav(stereo);
    audio::WemInfo info;
    Check(wav.size() == 44 + 24 && audio::ParseWemInfo(wav, info) && info.codec == 1 && info.channels == 2 &&
              info.sample_rate == 48000 && info.block_align == 4 && info.bits_per_sample == 16 && info.data_size == 24 &&
              info.data_offset == 44, "BuildWav: RIFF/WAVE PCM 16 bits de 44 bytes de cabecalho");
    audio::PcmBuffer round_trip;
    Check(audio::DecodeWem(wav, round_trip, error) && round_trip.samples == stereo.samples, "DecodeWem le de volta o WAV PCM gerado");
}

void TestCries(const std::filesystem::path& romfs) {
    const std::filesystem::path dir = romfs / "audio" / "wwise" / "bin" / "Switch";
    std::string error;
    audio::WwiseBank voice;
    audio::WwisePackage stream;
    const bool loaded = voice.Load((dir / "PM_VOICE.bnk").string(), error) && stream.Open((dir / "Stream.pck").string(), error);
    size_t events = 0;
    for (const audio::HircObject& object : voice.HircObjects()) events += object.type == 4 ? 1 : 0;
    Check(loaded && voice.HircObjects().size() == 5056 && voice.HircObjectCount() == 5056 && events == 258,
          "PM_VOICE.bnk: 5056 objetos HIRC, 258 eventos");
    Check(audio::WwiseHash("PLAY_PV_0150") == 2296219905u && audio::WwiseHash("play_pv_0150") == 2296219905u,
          "WwiseHash(\"PLAY_PV_0150\") = 2296219905 (FNV-1 32 bits, sem diferenciar maiusculas)");

    uint32_t media_id = 0;
    audio::WwiseSoundSource source;
    Check(loaded && audio::ResolvePokemonCryMediaId(voice, 150, media_id, error) && media_id == 143060364 &&
              audio::ResolvePokemonCry(voice, 150, source, error) && source.InStreamPackage() && source.sound_id == 784873224,
          "Cry do Mewtwo: Event -> Play -> Switch -> Switch (padrao) -> Sound 784873224 -> media 143060364 no Stream.pck");
    Check(!audio::ResolvePokemonCryMediaId(voice, 1000, media_id, error) && !error.empty(),
          "Especie sem evento PLAY_PV (1000) devolve erro");

    // Todos os eventos PLAY_PV_0000..9999 do banco resolvem para faixas PTADPCM do Stream.pck
    size_t cries = 0, in_stream = 0, ptadpcm = 0;
    std::vector<uint8_t> header;
    for (unsigned species = 0; loaded && species < 10000; ++species) {
        char name[16];
        std::snprintf(name, sizeof(name), "PLAY_PV_%04u", species);
        if (!voice.FindObject(audio::WwiseHash(name))) continue;
        ++cries;
        audio::WemInfo info;
        const audio::WwiseMedia* media = audio::ResolvePokemonCry(voice, static_cast<uint16_t>(species), source, error) &&
                                                 source.InStreamPackage() ? stream.Find(source.media_id) : nullptr;
        in_stream += media ? 1 : 0;
        ptadpcm += media && stream.Read(*media, header, error, 256) && audio::ParseWemInfo(header, info) && info.codec == 0x8311 ? 1 : 0;
    }
    Check(cries == 235 && in_stream == 235 && ptadpcm == 235, "Os 235 cries resolvem para faixas PTADPCM do Stream.pck");

    // Decodificacao completa do cry do Mewtwo, conferida com um decodificador de referencia em Python
    std::vector<uint8_t> wem;
    audio::PcmBuffer pcm;
    const audio::WwiseMedia* media = loaded ? stream.Find(143060364) : nullptr;
    const bool decoded = media && stream.Read(*media, wem, error) && audio::DecodeWem(wem, pcm, error);
    int peak = 0;
    for (const int16_t s : pcm.samples) peak = std::max(peak, std::abs(int{s}));
    const auto raw = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(pcm.samples.data()), pcm.samples.size() * 2);
    Check(decoded && pcm.sample_rate == 32000 && pcm.channels == 1 && pcm.samples.size() == 52160 && peak == 6849 &&
              Crc32(raw) == 0x4360DABEu, "DecodeWem: cry do Mewtwo = 52160 amostras a 32 kHz (1,63 s), CRC32 da referencia");
    const audio::PcmBuffer out = audio::ResampleLinear(pcm, 48000, 2);
    Check(out.FrameCount() == 78240 && std::abs(out.DurationSeconds() - pcm.DurationSeconds()) < 1e-3f,
          "Cry reamostrado para 48 kHz estereo com a mesma duracao");

    audio::PcmBuffer vorbis;
    const audio::WwiseMedia* bgm = nullptr;
    for (const audio::WwiseMedia& m : stream.Streams()) {
        audio::WemInfo info;
        if (stream.Read(m, header, error, 256) && audio::ParseWemInfo(header, info) && info.codec == 0xFFFF) {
            bgm = &m;
            break;
        }
    }
    Check(bgm && stream.Read(*bgm, wem, error, 4096) && !audio::DecodeWem(wem, vorbis, error) &&
              error.find("Vorbis") != std::string::npos, "DecodeWem recusa Vorbis com erro explicito");
}

void TestAmbientStream(const std::filesystem::path& romfs) {
    const std::filesystem::path pck_path = romfs / "audio" / "wwise" / "bin" / "Switch" / "Stream.pck";
    std::string error;
    audio::WwisePackage stream;
    const bool pck_ok = stream.Open(pck_path.string(), error);
    Check(pck_ok, "Abertura do Stream.pck para streaming de som ambiente");
    if (!pck_ok) return;

    // Faixa 294474531: Som ambiente de Lumiose City em PTADPCM estereo a 48.000 Hz (~243.7 segundos)
    audio::AmbientAudioStream amb_stream;
    const bool open_ok = amb_stream.Open(stream, 294474531, error);
    Check(open_ok && amb_stream.IsValid() && amb_stream.GetMediaId() == 294474531 &&
              amb_stream.GetSampleRate() == 48000 && amb_stream.GetChannels() == 2 &&
              amb_stream.GetDurationSeconds() > 200.0f,
          "AmbientAudioStream: abre faixa 294474531 (PTADPCM 48 kHz stereo, ~244s)");

    // Leitura do primeiro bloco de 2048 quadros (4096 amostras PCM 16-bit interleaved)
    std::vector<int16_t> first_buf;
    const bool buf1_ok = amb_stream.GetNextBuffer(first_buf, 2048, true);
    int peak = 0;
    for (int16_t s : first_buf) peak = std::max(peak, std::abs(int{s}));
    Check(buf1_ok && first_buf.size() == 4096 && peak > 100,
          "AmbientAudioStream::GetNextBuffer: extrai 2048 quadros (4096 amostras) com amplitude real");

    // Leitura do segundo bloco sucessivo (deve continuar sem reinicio prematuro)
    std::vector<int16_t> second_buf;
    const bool buf2_ok = amb_stream.GetNextBuffer(second_buf, 2048, true);
    Check(buf2_ok && second_buf.size() == 4096 && second_buf != first_buf,
          "AmbientAudioStream::GetNextBuffer: streaming continuo sem repeticao prematura");

    // Reset() e reinicio do inicio da faixa
    amb_stream.Reset();
    std::vector<int16_t> reset_buf;
    const bool reset_ok = amb_stream.GetNextBuffer(reset_buf, 2048, true);
    Check(reset_ok && reset_buf == first_buf,
          "AmbientAudioStream::Reset: reinicia streaming do ponto zero conferindo buffer");
}

} // namespace

int main(int argc, char** argv) {
    TestSynthetic();
    TestDecoderSynthetic();
    if (argc > 1) {
        TestRomfs(argv[1]);
        TestCries(argv[1]);
        TestAmbientStream(argv[1]);
    } else {
        std::puts("(sem <romfs_dir>: testes de integracao ignorados)");
    }
    std::printf("%s (%d falhas)\n", g_failures == 0 ? "OK" : "FALHOU", g_failures);
    return g_failures == 0 ? 0 : 1;
}
