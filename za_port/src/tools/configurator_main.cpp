// za_configurator — configurador inicial do port para PC (za_config.ini), verificacao do RomFS e do .nsp
//
// uso: za_configurator [--config <arquivo.ini>] [--base <pasta>] [--check | --reset | --set <secao.chave> <valor>...]
//   sem opcoes de acao abre o menu interativo. --base e a pasta do jogo (padrao: pasta atual); romfs_path
//   relativo e resolvido contra ela e os .nsp sao procurados nela e na pasta acima.
//   --check  retorna 0 se o RomFS e a DLL do Oodle estao prontos e o INI nao tem erros
//   --reset  grava os padroes de fabrica
//   --set    aplica um ou mais pares, valida os limites e grava

#include "config/game_config.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

// Cores ANSI; ficam vazias quando a saida nao e um console (pipe, arquivo)
const char* kReset = "";
const char* kBold = "";
const char* kGreen = "";
const char* kYellow = "";
const char* kRed = "";
const char* kCyan = "";

void EnableConsole() {
    bool color = false;
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    color = out != INVALID_HANDLE_VALUE && GetConsoleMode(out, &mode) &&
            SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
    if (color) {
        kReset = "\x1b[0m";
        kBold = "\x1b[1m";
        kGreen = "\x1b[32m";
        kYellow = "\x1b[33m";
        kRed = "\x1b[31m";
        kCyan = "\x1b[36m";
    }
}

std::string FormatGb(uint64_t bytes) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    return buffer;
}

bool OodleAvailable(const fs::path& base) {
    std::error_code ec;
    const char* env = std::getenv("ZA_OODLE_DLL");
    return fs::is_regular_file(base / "oo2core_9_win64.dll", ec) || (env && fs::is_regular_file(env, ec));
}

void PrintStorage(const za::StorageStatus& status, const fs::path& base) {
    std::cout << "  RomFS:  " << status.romfs_dir.string() << "  "
              << (status.romfs_ready ? std::string(kGreen) + "[OK]" : std::string(kRed) + "[INCOMPLETO]") << kReset << "\n";
    if (!status.romfs_ready) {
        std::cout << "          arc/data.trpfd " << (status.trpfd_found ? "ok" : "ausente") << ", arc/data.trpfs "
                  << (status.trpfs_found ? "ok" : "ausente") << "\n";
    }
    std::cout << "  Oodle:  " << (OodleAvailable(base) ? std::string(kGreen) + "oo2core_9_win64.dll encontrada" : std::string(kRed) + "oo2core_9_win64.dll ausente (pasta base ou ZA_OODLE_DLL)")
              << kReset << "\n";
    for (const za::NspFile& nsp : status.nsp_files) std::cout << "  .nsp:   " << nsp.path.string() << " (" << FormatGb(nsp.size_bytes) << ")\n";
    if (status.CanReclaimNsp()) {
        std::cout << kYellow << "  O RomFS ja esta extraido: remover o(s) .nsp libera " << FormatGb(status.ReclaimableBytes()) << "." << kReset << "\n";
    }
}

bool ReadLine(const std::string& prompt, std::string& out) {
    std::cout << kCyan << prompt << kReset;
    std::cout.flush();
    return static_cast<bool>(std::getline(std::cin, out));
}

std::string Hint(const std::string& key) {
    static const std::pair<const char*, const char*> hints[] = {
        {"storage.romfs_path", "caminho (relativo a pasta base ou absoluto)"},
        {"storage.auto_clean_nsp", "true/false — oferecer remover o .nsp ao abrir (sempre pede confirmacao)"},
        {"graphics.window_width", "1280, 1920, 2560, 3840 (640..7680)"},
        {"graphics.window_height", "720, 1080, 1440, 2160 (360..4320)"},
        {"graphics.display_mode", "windowed, borderless, fullscreen"},
        {"graphics.shading", "solid, wireframe"},
        {"graphics.initial_camera", "third_person, free_fly"},
        {"graphics.vsync", "true/false"},
        {"performance.target_fps", "30, 60, 120, 144 ou 0 (ilimitado)"},
        {"performance.oodle_cache_kb", "64, 128, 256, 512"},
        {"performance.log_level", "DEBUG, INFO, WARN, ERROR"},
        {"performance.enable_profiler", "true/false — grava za_performance.log ao sair"},
        {"audio.master_volume", "0..100"},
        {"audio.bgm_volume", "0..100"},
        {"audio.sfx_volume", "0..100"},
        {"audio.bgm_enabled", "true/false"},
        {"input.device_preference", "auto, keyboard_mouse, gamepad_only"},
        {"input.mouse_sensitivity", "0.2..5.0"},
        {"input.invert_mouse_y", "true/false"},
    };
    for (const auto& [name, text] : hints) {
        if (key == name) return text;
    }
    return {};
}

// Submenu generico de uma secao: lista as chaves com os valores atuais e edita via GameConfig::Set
void EditSection(za::GameConfig& config, const std::string& section, const std::string& title, bool& dirty) {
    std::vector<std::string> keys;
    for (const std::string& key : za::GameConfig::Keys()) {
        if (key.starts_with(section + ".")) keys.push_back(key);
    }
    for (std::string line;;) {
        std::cout << "\n" << kBold << title << kReset << "\n";
        for (size_t i = 0; i < keys.size(); ++i) {
            std::cout << "  [" << i + 1 << "] " << keys[i].substr(section.size() + 1) << " = " << kGreen << config.Get(keys[i]) << kReset
                      << "   (" << Hint(keys[i]) << ")\n";
        }
        std::cout << "  [0] Voltar\n";
        if (!ReadLine("Opcao: ", line) || line == "0" || line.empty()) return;
        const int choice = std::atoi(line.c_str());
        if (choice < 1 || static_cast<size_t>(choice) > keys.size()) continue;
        const std::string& key = keys[static_cast<size_t>(choice) - 1];
        std::string value, error;
        if (!ReadLine("Novo valor para " + key + ": ", value)) return;
        if (!config.Set(key, value, &error)) {
            std::cout << kRed << "  " << error << kReset << "\n";
            continue;
        }
        const std::string requested = config.Get(key);
        config.ValidateAndClamp();
        if (config.Get(key) != requested) std::cout << kYellow << "  ajustado para " << config.Get(key) << kReset << "\n";
        dirty = true;
    }
}

void StorageMenu(za::GameConfig& config, const fs::path& base, bool& dirty) {
    for (std::string line;;) {
        const za::StorageStatus status = za::CheckStorage(config, base);
        std::cout << "\n" << kBold << "Armazenamento & RomFS" << kReset << "\n";
        PrintStorage(status, base);
        std::cout << "  [1] Alterar caminho do RomFS (" << config.storage.romfs_path << ")\n"
                  << "  [2] auto_clean_nsp = " << (config.storage.auto_clean_nsp ? "true" : "false") << "\n";
        if (status.CanReclaimNsp()) std::cout << "  [3] Remover .nsp e liberar " << FormatGb(status.ReclaimableBytes()) << "\n";
        std::cout << "  [0] Voltar\n";
        if (!ReadLine("Opcao: ", line) || line == "0" || line.empty()) return;
        if (line == "1") {
            std::string path;
            if (ReadLine("Novo caminho: ", path) && config.Set("storage.romfs_path", path)) {
                config.ValidateAndClamp();
                dirty = true;
            }
        } else if (line == "2") {
            config.storage.auto_clean_nsp = !config.storage.auto_clean_nsp;
            dirty = true;
        } else if (line == "3" && status.CanReclaimNsp()) {
            std::cout << kRed << "  Isto apaga permanentemente:" << kReset << "\n";
            for (const za::NspFile& nsp : status.nsp_files) std::cout << "    " << nsp.path.string() << "\n";
            std::string confirm;
            if (!ReadLine("Digite REMOVER para confirmar: ", confirm) || confirm != "REMOVER") {
                std::cout << "  Cancelado.\n";
                continue;
            }
            for (const za::NspFile& nsp : status.nsp_files) {
                std::error_code ec;
                const bool removed = fs::remove(nsp.path, ec);
                std::cout << "  " << nsp.path.filename().string() << ": " << (removed ? "removido" : "falhou: " + ec.message()) << "\n";
            }
        }
    }
}

int Interactive(za::GameConfig& config, const fs::path& base, const std::string& config_path) {
    bool dirty = false;
    const za::StorageStatus status = za::CheckStorage(config, base);
    std::cout << kBold << "za_configurator — Pokemon Legends Z-A (port para PC)" << kReset << "\n";
    PrintStorage(status, base);
    if (config.storage.auto_clean_nsp && status.CanReclaimNsp()) StorageMenu(config, base, dirty);

    for (std::string line;;) {
        std::cout << "\n" << kBold << "Menu" << kReset << "  (" << config_path << (dirty ? ", alteracoes nao salvas" : "") << ")\n"
                  << "  [1] Gerenciar Armazenamento & RomFS (.nsp)\n"
                  << "  [2] Configuracoes Graficas (Resolucao, Modo de Exibicao, VSync)\n"
                  << "  [3] Configuracoes de Desempenho (FPS Alvo, Cache Oodle, Telemetria)\n"
                  << "  [4] Configuracoes de Audio (Volumes Master, BGM, SFX)\n"
                  << "  [5] Configuracoes de Controles (Sensibilidade do Mouse, Inverter Eixo)\n"
                  << "  [6] Restaurar Padroes\n"
                  << "  [7] Salvar e Sair\n"
                  << "  [0] Sair sem salvar\n";
        if (!ReadLine("Opcao: ", line)) return 1;  // fim da entrada: nao salva
        if (line == "1") StorageMenu(config, base, dirty);
        else if (line == "2") EditSection(config, "graphics", "Configuracoes Graficas", dirty);
        else if (line == "3") EditSection(config, "performance", "Desempenho", dirty);
        else if (line == "4") EditSection(config, "audio", "Audio", dirty);
        else if (line == "5") EditSection(config, "input", "Controles", dirty);
        else if (line == "6") {
            std::string confirm;
            if (ReadLine("Restaurar todos os padroes? (s/N): ", confirm) && (confirm == "s" || confirm == "S")) {
                config.SetDefaults();
                dirty = true;
            }
        } else if (line == "7") {
            const bool saved = config.SaveToFile(config_path);
            std::cout << (saved ? std::string(kGreen) + "Salvo em " : std::string(kRed) + "Falha ao gravar ") << config_path << kReset << "\n";
            return saved ? 0 : 1;
        } else if (line == "0") {
            std::string confirm;
            if (!dirty || (ReadLine("Descartar alteracoes? (s/N): ", confirm) && (confirm == "s" || confirm == "S"))) return 0;
        }
    }
}

void PrintUsage() {
    std::cout << "uso: za_configurator [--config <arquivo.ini>] [--base <pasta>] [--check | --reset | --set <secao.chave> <valor>...]\n"
                 "chaves:";
    for (const std::string& key : za::GameConfig::Keys()) std::cout << " " << key;
    std::cout << "\n";
}

} // namespace

int main(int argc, char** argv) {
    EnableConsole();
    std::string config_path = "za_config.ini";
    fs::path base = fs::current_path();
    std::string action;
    std::vector<std::pair<std::string, std::string>> assignments;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--base" && i + 1 < argc) {
            base = argv[++i];
        } else if (arg == "--check" || arg == "--reset") {
            action = arg;
        } else if (arg == "--set" && i + 2 < argc) {
            action = arg;
            assignments.emplace_back(argv[i + 1], argv[i + 2]);
            i += 2;
        } else {
            PrintUsage();
            return arg == "-h" || arg == "--help" ? 0 : 2;
        }
    }

    za::GameConfig config;
    std::vector<std::string> warnings;
    std::error_code ec;
    const bool has_file = fs::is_regular_file(config_path, ec);
    if (has_file && !config.LoadFromFile(config_path, warnings)) {
        std::cerr << "nao foi possivel ler " << config_path << "\n";
        return 1;
    }
    for (const std::string& warning : warnings) std::cout << kYellow << config_path << " " << warning << kReset << "\n";

    if (action == "--reset") {
        config.SetDefaults();
        const bool saved = config.SaveToFile(config_path);
        std::cout << (saved ? "Padroes gravados em " : "Falha ao gravar ") << config_path << "\n";
        return saved ? 0 : 1;
    }
    if (action == "--set") {
        for (const auto& [key, value] : assignments) {
            std::string error;
            if (!config.Set(key, value, &error)) {
                std::cerr << error << "\n";
                return 2;
            }
        }
        config.ValidateAndClamp();
        for (const auto& [key, value] : assignments) std::cout << key << " = " << config.Get(key) << "\n";
        return config.SaveToFile(config_path) ? 0 : 1;
    }
    if (action == "--check") {
        const za::StorageStatus status = za::CheckStorage(config, base);
        std::cout << "Config: " << config_path << (has_file ? "" : " (ausente: usando padroes)") << ", " << warnings.size() << " avisos\n";
        PrintStorage(status, base);
        const bool ready = status.romfs_ready && OodleAvailable(base) && warnings.empty();
        std::cout << (ready ? std::string(kGreen) + "Pronto para jogar." : std::string(kRed) + "Nao esta pronto.") << kReset << "\n";
        return ready ? 0 : 1;
    }
    return Interactive(config, base, config_path);
}
