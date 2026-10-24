// Testes do GameConfig (za_config.ini): padroes, gravacao/leitura, tolerancia do parser, limites e deteccao do .nsp
// uso: config_test   (usa apenas uma pasta temporaria; nao toca no RomFS nem em .nsp reais)

#include "config/game_config.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace za;
namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    std::printf("%s %s\n", condition ? "[PASS]" : "[FAIL]", what);
    if (!condition) ++g_failures;
}

void WriteFile(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

void TestDefaults() {
    GameConfig c;
    c.audio.master_volume = 5;
    c.graphics.display_mode = "fullscreen";
    c.SetDefaults();
    Check(c.storage.romfs_path == "../romfs" && !c.storage.auto_clean_nsp && c.graphics.window_width == 1280 &&
              c.graphics.window_height == 720 && c.graphics.display_mode == "windowed" && c.graphics.vsync &&
              c.performance.target_fps == 60 && c.performance.oodle_cache_kb == 64 && c.performance.log_level == "INFO" &&
              c.audio.master_volume == 100 && c.audio.bgm_volume == 80 && c.audio.sfx_volume == 90 &&
              c.input.device_preference == "auto" && c.input.mouse_sensitivity == 1.0f && !c.input.invert_mouse_y,
          "1. SetDefaults restaura todos os valores de fabrica");
}

void TestSaveAndLoad(const fs::path& dir) {
    const fs::path file = dir / "za_config_test.ini";
    GameConfig c;
    c.graphics.window_width = 2560;
    c.graphics.window_height = 1440;
    c.graphics.display_mode = "borderless";
    c.graphics.vsync = false;
    c.performance.target_fps = 144;
    c.performance.log_level = "DEBUG";
    c.audio.bgm_volume = 35;
    c.input.mouse_sensitivity = 2.5f;
    c.input.invert_mouse_y = true;
    c.storage.romfs_path = "D:/jogos/za/romfs";
    const bool saved = c.SaveToFile(file.string());
    std::ifstream in(file);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    Check(saved && text.starts_with("[Storage]\nromfs_path=D:/jogos/za/romfs\n") && text.find("[Graphics]\nwindow_width=2560\n") != std::string::npos &&
              text.find("mouse_sensitivity=2.500000\n") != std::string::npos && text.find("vsync=false\n") != std::string::npos,
          "2. SaveToFile grava za_config_test.ini no formato da especificacao");

    GameConfig loaded;
    std::vector<std::string> warnings;
    const bool ok = loaded.LoadFromFile(file.string(), warnings);
    Check(ok && warnings.empty() && loaded.ToIniString() == c.ToIniString() && loaded.graphics.display_mode == "borderless" &&
              loaded.performance.target_fps == 144 && loaded.input.mouse_sensitivity == 2.5f && loaded.input.invert_mouse_y,
          "3. LoadFromFile le de volta exatamente os valores gravados");
    Check(!loaded.LoadFromFile((dir / "nao_existe.ini").string()), "LoadFromFile de arquivo ausente devolve false");
}

void TestTolerantParser() {
    GameConfig c;
    std::vector<std::string> warnings;
    c.LoadFromString("# comentario\n"
                     "; outro comentario\n"
                     "\n"
                     "  [ Audio ]  \r\n"
                     "   master_volume   =   55   \r\n"
                     "volume_desconhecido=3\n"
                     "[SecaoNova]\n"
                     "x=1\n"
                     "[graphics]\n"
                     "window_height=720A\n"
                     "vsync = OFF\n"
                     "linha sem igual\n",
                     &warnings);
    Check(c.audio.master_volume == 55 && !c.graphics.vsync && c.graphics.window_height == 720 && warnings.size() == 4,
          "4. Parser tolera espacos, CRLF, comentarios #/;, secoes/chaves desconhecidas; \"720A\" e rejeitado com aviso");
}

void TestClamp() {
    GameConfig c;
    c.audio.master_volume = -10;
    c.audio.bgm_volume = 150;
    c.audio.sfx_volume = 101;
    c.input.mouse_sensitivity = 9.0f;
    c.performance.target_fps = 75;
    c.performance.oodle_cache_kb = 5000;
    c.performance.log_level = "warn";
    c.ValidateAndClamp();
    Check(c.audio.master_volume == 0 && c.audio.bgm_volume == 100 && c.audio.sfx_volume == 100 && c.input.mouse_sensitivity == 5.0f &&
              c.performance.target_fps == 60 && c.performance.oodle_cache_kb == 512 && c.performance.log_level == "WARN",
          "5. Clamp: volume -10 -> 0, 150 -> 100; sensibilidade 9 -> 5; FPS 75 -> 60; cache 5000 -> 512 KB; log em maiusculas");
    c.input.mouse_sensitivity = std::nanf("");
    c.performance.target_fps = 0;
    c.ValidateAndClamp();
    Check(c.input.mouse_sensitivity == 1.0f && c.performance.target_fps == 0, "Sensibilidade NaN volta a 1.0; target_fps 0 (ilimitado) e mantido");
}

void TestResolution() {
    GameConfig c;
    c.LoadFromString("[Graphics]\nwindow_width=320\nwindow_height=200\n");
    const bool minimum = c.graphics.window_width == 640 && c.graphics.window_height == 360;
    c.LoadFromString("[Graphics]\nwindow_width=99999\nwindow_height=-5\n");
    Check(minimum && c.graphics.window_width == 7680 && c.graphics.window_height == 360,
          "6. Resolucao limitada a 640x360 .. 7680x4320");
}

void TestStoragePaths(const fs::path& dir) {
    const fs::path game = dir / "jogo";
    const fs::path port = game / "za_port";
    fs::create_directories(port);
    GameConfig c;  // romfs_path = ../romfs
    const StorageStatus missing = CheckStorage(c, port);
    WriteFile(game / "romfs" / "arc" / "data.trpfd", "x");
    const StorageStatus partial = CheckStorage(c, port);
    WriteFile(game / "romfs" / "arc" / "data.trpfs", "x");
    const StorageStatus ready = CheckStorage(c, port);
    c.storage.romfs_path = "   ";
    c.ValidateAndClamp();
    Check(!missing.romfs_ready && partial.trpfd_found && !partial.romfs_ready && ready.romfs_ready &&
              ready.romfs_dir == (game / "romfs").lexically_normal() && c.storage.romfs_path == "../romfs",
          "7. Caminho do RomFS: relativo a pasta base, exige data.trpfd + data.trpfs; caminho vazio volta ao padrao");
}

void TestDisplayModes() {
    GameConfig c;
    bool accepted = true;
    const std::pair<const char*, const char*> modes[] = {
        {"windowed", "windowed"}, {"fullscreen", "fullscreen"}, {"borderless", "borderless"}, {"FullScreen", "fullscreen"}};
    for (const auto& [written, expected] : modes) {
        c.LoadFromString(std::string("[Graphics]\ndisplay_mode=") + written + "\n");
        accepted = accepted && c.graphics.display_mode == expected;
    }
    c.LoadFromString("[Graphics]\ndisplay_mode=exclusive\nshading=raytraced\ninitial_camera=drone\n[Input]\ndevice_preference=wheel\n");
    Check(accepted && c.graphics.display_mode == "windowed" && c.graphics.shading == "solid" && c.graphics.initial_camera == "third_person" &&
              c.input.device_preference == "auto",
          "8. display_mode aceita windowed/fullscreen/borderless (sem diferenciar maiusculas); opcoes invalidas voltam ao padrao");
}

void TestResetAndSet(const fs::path& dir) {
    const fs::path file = dir / "reset.ini";
    GameConfig c;
    std::string error;
    const bool set_ok = c.Set("audio.master_volume", "70", &error) && c.Set("GRAPHICS.vsync", "false", &error) &&
                        c.Set("input.mouse_sensitivity", "0.75", &error);
    const bool rejects = !c.Set("audio.volume", "1", &error) && !c.Set("audio.sfx_volume", "alto", &error) && !c.Set("semponto", "1", &error);
    c.SaveToFile(file.string());
    c.SetDefaults();
    c.SaveToFile(file.string());
    GameConfig reloaded;
    reloaded.audio.master_volume = 3;
    reloaded.LoadFromFile(file.string());
    Check(set_ok && rejects && c.Get("audio.master_volume") == "100" && reloaded.ToIniString() == GameConfig{}.ToIniString() &&
              GameConfig::Keys().size() == 19,
          "9. Set/Get por secao.chave (rejeita chave e valor invalidos); reset grava e recarrega os padroes de fabrica");
}

void TestNspDetection(const fs::path& dir) {
    const fs::path game = dir / "nsp";
    const fs::path port = game / "za_port";
    fs::create_directories(port);
    GameConfig c;
    const StorageStatus none = CheckStorage(c, port);
    WriteFile(game / "Jogo Base.NSP", std::string(4096, 'n'));
    WriteFile(port / "update.nsp", std::string(1000, 'u'));
    WriteFile(game / "leia.txt", "x");
    const StorageStatus without_romfs = CheckStorage(c, port);
    WriteFile(game / "romfs" / "arc" / "data.trpfd", "x");
    WriteFile(game / "romfs" / "arc" / "data.trpfs", "x");
    const StorageStatus with_romfs = CheckStorage(c, port);
    Check(none.nsp_files.empty() && without_romfs.nsp_files.size() == 2 && !without_romfs.CanReclaimNsp() &&
              with_romfs.CanReclaimNsp() && with_romfs.ReclaimableBytes() == 5096,
          "10. Deteccao simulada de .nsp (pasta base e acima, extensao sem diferenciar maiusculas); liberavel so com RomFS pronto");
}

} // namespace

int main() {
    const fs::path dir = fs::temp_directory_path() / "za_config_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);

    TestDefaults();
    TestSaveAndLoad(dir);
    TestTolerantParser();
    TestClamp();
    TestResolution();
    TestStoragePaths(dir);
    TestDisplayModes();
    TestResetAndSet(dir);
    TestNspDetection(dir);

    fs::remove_all(dir, ec);
    std::printf("%s (%d falhas)\n", g_failures == 0 ? "OK" : "FALHOU", g_failures);
    return g_failures == 0 ? 0 : 1;
}
