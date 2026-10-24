#pragma once
// Configuracoes do port para PC (za_config.ini) — leitura/gravacao INI, limites seguros e checagem do RomFS/.nsp
//
// INI: secoes [Storage] [Graphics] [Performance] [Audio] [Input], linhas "chave=valor"; espacos nas pontas sao
// ignorados, assim como linhas vazias, comentarios iniciados por '#' ou ';' e chaves/secoes desconhecidas
// (viram avisos). Valores que nao convertem por inteiro (ex.: "720A") mantem o valor anterior e geram aviso.
// Todo carregamento termina em ValidateAndClamp().

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace za {

struct StorageConfig {
    std::string romfs_path = "../romfs";
    bool auto_clean_nsp = false;      // configurador oferece remover o .nsp (sempre com confirmacao explicita)
};

struct GraphicsConfig {
    int window_width = 1280;          // 1280, 1920, 2560, 3840 (minimo 640, maximo 7680)
    int window_height = 720;          // 720, 1080, 1440, 2160 (minimo 360, maximo 4320)
    std::string display_mode = "windowed";       // "windowed", "borderless", "fullscreen"
    std::string shading = "solid";               // "solid", "wireframe"
    std::string initial_camera = "third_person"; // "third_person", "free_fly"
    bool vsync = true;                // true = 60 FPS fixo, false = livre
};

struct PerformanceConfig {
    int target_fps = 60;              // 30, 60, 120, 144, 0 (ilimitado); outros valores vao para o mais proximo
    // Orcamento do cache de packs em KB (64, 128, 256, 512). Atencao: TrpfsArchive::SetPackCacheBudget recebe
    // BYTES e o padrao do motor e 64 MB — converter com oodle_cache_kb * 1024 ao integrar
    int oodle_cache_kb = 64;
    std::string log_level = "INFO";   // "DEBUG", "INFO", "WARN", "ERROR"
    bool enable_profiler = true;      // gera za_performance.log ao sair
};

struct AudioConfig {
    int master_volume = 100;          // 0 a 100%
    int bgm_volume = 80;
    int sfx_volume = 90;
    bool bgm_enabled = true;
};

struct InputConfig {
    std::string device_preference = "auto";  // "auto", "keyboard_mouse", "gamepad_only"
    float mouse_sensitivity = 1.0f;          // 0.2 a 5.0
    bool invert_mouse_y = false;
};

struct GameConfig {
    StorageConfig storage;
    GraphicsConfig graphics;
    PerformanceConfig performance;
    AudioConfig audio;
    InputConfig input;

    bool LoadFromFile(const std::string& filepath);
    bool LoadFromFile(const std::string& filepath, std::vector<std::string>& warnings);
    bool SaveToFile(const std::string& filepath) const;
    void SetDefaults();
    void ValidateAndClamp();

    // Mesmo parser do arquivo, a partir de texto (warnings opcional)
    bool LoadFromString(std::string_view text, std::vector<std::string>* warnings = nullptr);
    std::string ToIniString() const;

    // Ajuste por "secao.chave" (sem diferenciar maiusculas na secao), ex.: Set("audio.master_volume", "70");
    // usado pelo INI e pelo "--set" do configurador. Nao chama ValidateAndClamp.
    bool Set(std::string_view dotted_key, std::string_view value, std::string* error = nullptr);
    // Valor atual formatado como no INI; vazio se a chave nao existe
    std::string Get(std::string_view dotted_key) const;
    // Todas as chaves "secao.chave" na ordem do INI
    static std::vector<std::string> Keys();
};

// Arquivos de jogo: RomFS extraido (arc/data.trpfd + arc/data.trpfs) e .nsp na pasta base e na pasta acima dela
struct NspFile {
    std::filesystem::path path;
    uint64_t size_bytes{0};
};

struct StorageStatus {
    std::filesystem::path romfs_dir;  // romfs_path resolvido contra a pasta base
    bool romfs_ready{false};
    bool trpfd_found{false};
    bool trpfs_found{false};
    std::vector<NspFile> nsp_files;

    uint64_t ReclaimableBytes() const;
    // .nsp pode ser liberado: RomFS completo e pelo menos um .nsp encontrado
    bool CanReclaimNsp() const { return romfs_ready && !nsp_files.empty(); }
};

StorageStatus CheckStorage(const GameConfig& config, const std::filesystem::path& base_dir);

} // namespace za
