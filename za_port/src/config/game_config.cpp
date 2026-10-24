#include "config/game_config.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>

namespace za {

namespace {

std::string Lower(std::string_view text) {
    std::string out(text);
    for (char& c : out) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    return out;
}

std::string_view Trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

bool ParseBool(std::string_view text, bool& out) {
    const std::string value = Lower(text);
    if (value == "true" || value == "1" || value == "yes" || value == "on") {
        out = true;
        return true;
    }
    if (value == "false" || value == "0" || value == "no" || value == "off") {
        out = false;
        return true;
    }
    return false;
}

// Exige que o texto inteiro seja o numero ("720A" falha)
template <typename T>
bool ParseNumber(std::string_view text, T& out) {
    if (!text.empty() && text.front() == '+') text.remove_prefix(1);
    T value{};
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || ec != std::errc() || end != text.data() + text.size()) return false;
    out = value;
    return true;
}

std::string FormatBool(bool value) { return value ? "true" : "false"; }

struct Field {
    std::string_view section;   // como aparece no INI
    std::string_view key;
    std::function<bool(GameConfig&, std::string_view)> set;
    std::function<std::string(const GameConfig&)> get;
};

template <typename Member>
Field IntField(std::string_view section, std::string_view key, Member member) {
    return {section, key, [member](GameConfig& c, std::string_view v) { return ParseNumber(v, member(c)); },
            [member](const GameConfig& c) { return std::to_string(member(const_cast<GameConfig&>(c))); }};
}

template <typename Member>
Field BoolField(std::string_view section, std::string_view key, Member member) {
    return {section, key, [member](GameConfig& c, std::string_view v) { return ParseBool(v, member(c)); },
            [member](const GameConfig& c) { return FormatBool(member(const_cast<GameConfig&>(c))); }};
}

template <typename Member>
Field TextField(std::string_view section, std::string_view key, Member member) {
    return {section, key,
            [member](GameConfig& c, std::string_view v) {
                member(c) = std::string(v);
                return true;
            },
            [member](const GameConfig& c) { return member(const_cast<GameConfig&>(c)); }};
}

const std::vector<Field>& Fields() {
    static const std::vector<Field> fields = {
        TextField("Storage", "romfs_path", [](GameConfig& c) -> std::string& { return c.storage.romfs_path; }),
        BoolField("Storage", "auto_clean_nsp", [](GameConfig& c) -> bool& { return c.storage.auto_clean_nsp; }),
        IntField("Graphics", "window_width", [](GameConfig& c) -> int& { return c.graphics.window_width; }),
        IntField("Graphics", "window_height", [](GameConfig& c) -> int& { return c.graphics.window_height; }),
        TextField("Graphics", "display_mode", [](GameConfig& c) -> std::string& { return c.graphics.display_mode; }),
        TextField("Graphics", "shading", [](GameConfig& c) -> std::string& { return c.graphics.shading; }),
        TextField("Graphics", "initial_camera", [](GameConfig& c) -> std::string& { return c.graphics.initial_camera; }),
        BoolField("Graphics", "vsync", [](GameConfig& c) -> bool& { return c.graphics.vsync; }),
        IntField("Performance", "target_fps", [](GameConfig& c) -> int& { return c.performance.target_fps; }),
        IntField("Performance", "oodle_cache_kb", [](GameConfig& c) -> int& { return c.performance.oodle_cache_kb; }),
        TextField("Performance", "log_level", [](GameConfig& c) -> std::string& { return c.performance.log_level; }),
        BoolField("Performance", "enable_profiler", [](GameConfig& c) -> bool& { return c.performance.enable_profiler; }),
        IntField("Audio", "master_volume", [](GameConfig& c) -> int& { return c.audio.master_volume; }),
        IntField("Audio", "bgm_volume", [](GameConfig& c) -> int& { return c.audio.bgm_volume; }),
        IntField("Audio", "sfx_volume", [](GameConfig& c) -> int& { return c.audio.sfx_volume; }),
        BoolField("Audio", "bgm_enabled", [](GameConfig& c) -> bool& { return c.audio.bgm_enabled; }),
        TextField("Input", "device_preference", [](GameConfig& c) -> std::string& { return c.input.device_preference; }),
        {"Input", "mouse_sensitivity",
         [](GameConfig& c, std::string_view v) { return ParseNumber(v, c.input.mouse_sensitivity); },
         [](const GameConfig& c) { return std::to_string(c.input.mouse_sensitivity); }},
        BoolField("Input", "invert_mouse_y", [](GameConfig& c) -> bool& { return c.input.invert_mouse_y; }),
    };
    return fields;
}

const Field* FindField(std::string_view section, std::string_view key) {
    const std::string s = Lower(section), k = Lower(key);
    for (const Field& field : Fields()) {
        if (Lower(field.section) == s && field.key == k) return &field;
    }
    return nullptr;
}

// Valor permitido mais proximo (empate fica com o menor)
int Nearest(int value, std::initializer_list<int> allowed) {
    int best = *allowed.begin();
    for (const int option : allowed) {
        if (std::abs(option - value) < std::abs(best - value)) best = option;
    }
    return best;
}

void NormalizeChoice(std::string& value, std::initializer_list<std::string_view> allowed, std::string_view fallback, bool upper = false) {
    std::string candidate = Lower(Trim(value));
    if (upper) {
        for (char& c : candidate) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
    }
    value = std::find(allowed.begin(), allowed.end(), candidate) != allowed.end() ? candidate : std::string(fallback);
}

} // namespace

void GameConfig::SetDefaults() {
    *this = GameConfig{};
}

void GameConfig::ValidateAndClamp() {
    const GameConfig defaults;
    storage.romfs_path = std::string(Trim(storage.romfs_path));
    if (storage.romfs_path.empty()) storage.romfs_path = defaults.storage.romfs_path;

    graphics.window_width = std::clamp(graphics.window_width, 640, 7680);
    graphics.window_height = std::clamp(graphics.window_height, 360, 4320);
    NormalizeChoice(graphics.display_mode, {"windowed", "borderless", "fullscreen"}, defaults.graphics.display_mode);
    NormalizeChoice(graphics.shading, {"solid", "wireframe"}, defaults.graphics.shading);
    NormalizeChoice(graphics.initial_camera, {"third_person", "free_fly"}, defaults.graphics.initial_camera);

    // 0 = ilimitado; negativo volta ao padrao; demais valores vao para o alvo suportado mais proximo
    if (performance.target_fps < 0) {
        performance.target_fps = defaults.performance.target_fps;
    } else if (performance.target_fps != 0) {
        performance.target_fps = Nearest(performance.target_fps, {30, 60, 120, 144});
    }
    performance.oodle_cache_kb = Nearest(std::clamp(performance.oodle_cache_kb, 64, 512), {64, 128, 256, 512});
    NormalizeChoice(performance.log_level, {"DEBUG", "INFO", "WARN", "ERROR"}, defaults.performance.log_level, true);

    audio.master_volume = std::clamp(audio.master_volume, 0, 100);
    audio.bgm_volume = std::clamp(audio.bgm_volume, 0, 100);
    audio.sfx_volume = std::clamp(audio.sfx_volume, 0, 100);

    NormalizeChoice(input.device_preference, {"auto", "keyboard_mouse", "gamepad_only"}, defaults.input.device_preference);
    input.mouse_sensitivity = std::isfinite(input.mouse_sensitivity) ? std::clamp(input.mouse_sensitivity, 0.2f, 5.0f)
                                                                     : defaults.input.mouse_sensitivity;
}

bool GameConfig::Set(std::string_view dotted_key, std::string_view value, std::string* error) {
    const size_t dot = dotted_key.find('.');
    const Field* field = dot == std::string_view::npos ? nullptr : FindField(dotted_key.substr(0, dot), dotted_key.substr(dot + 1));
    if (!field) {
        if (error) *error = "chave desconhecida: " + std::string(dotted_key);
        return false;
    }
    if (!field->set(*this, Trim(value))) {
        if (error) *error = "valor invalido para " + std::string(dotted_key) + ": \"" + std::string(Trim(value)) + "\"";
        return false;
    }
    return true;
}

std::string GameConfig::Get(std::string_view dotted_key) const {
    const size_t dot = dotted_key.find('.');
    const Field* field = dot == std::string_view::npos ? nullptr : FindField(dotted_key.substr(0, dot), dotted_key.substr(dot + 1));
    return field ? field->get(*this) : std::string();
}

std::vector<std::string> GameConfig::Keys() {
    std::vector<std::string> keys;
    for (const Field& field : Fields()) keys.push_back(Lower(field.section) + "." + std::string(field.key));
    return keys;
}

bool GameConfig::LoadFromString(std::string_view text, std::vector<std::string>* warnings) {
    const auto warn = [warnings](size_t line, const std::string& message) {
        if (warnings) warnings->push_back("linha " + std::to_string(line) + ": " + message);
    };
    std::string section;
    size_t line_number = 0;
    for (size_t start = 0; start <= text.size();) {
        const size_t end = std::min(text.find('\n', start), text.size());
        const std::string_view line = Trim(text.substr(start, end - start));
        start = end + 1;
        ++line_number;
        if (line.empty() || line.front() == '#' || line.front() == ';') continue;
        if (line.front() == '[') {
            const size_t close = line.find(']');
            section = std::string(Trim(line.substr(1, close == std::string_view::npos ? std::string_view::npos : close - 1)));
            if (close == std::string_view::npos) warn(line_number, "secao sem ']': " + section);
            continue;
        }
        const size_t equals = line.find('=');
        if (equals == std::string_view::npos) {
            warn(line_number, "linha ignorada (sem '='): " + std::string(line));
            continue;
        }
        const std::string_view key = Trim(line.substr(0, equals));
        const std::string_view value = Trim(line.substr(equals + 1));
        if (section.empty()) {
            warn(line_number, "chave fora de secao: " + std::string(key));
            continue;
        }
        std::string error;
        if (!Set(section + "." + std::string(key), value, &error)) warn(line_number, error);
    }
    ValidateAndClamp();
    return true;
}

bool GameConfig::LoadFromFile(const std::string& filepath) {
    std::vector<std::string> ignored;
    return LoadFromFile(filepath, ignored);
}

// Comeca dos padroes: chaves ausentes do arquivo ficam com o valor de fabrica
bool GameConfig::LoadFromFile(const std::string& filepath, std::vector<std::string>& warnings) {
    std::ifstream stream(filepath, std::ios::binary);
    if (!stream) return false;
    std::stringstream buffer;
    buffer << stream.rdbuf();
    SetDefaults();
    return LoadFromString(buffer.str(), &warnings);
}

std::string GameConfig::ToIniString() const {
    std::string out;
    std::string_view section;
    for (const Field& field : Fields()) {
        if (field.section != section) {
            if (!section.empty()) out += "\n";
            section = field.section;
            out += "[" + std::string(section) + "]\n";
        }
        out += std::string(field.key) + "=" + field.get(*this) + "\n";
    }
    return out;
}

bool GameConfig::SaveToFile(const std::string& filepath) const {
    std::ofstream stream(filepath, std::ios::binary | std::ios::trunc);
    const std::string text = ToIniString();
    return stream && stream.write(text.data(), static_cast<std::streamsize>(text.size()));
}

uint64_t StorageStatus::ReclaimableBytes() const {
    uint64_t total = 0;
    for (const NspFile& file : nsp_files) total += file.size_bytes;
    return total;
}

StorageStatus CheckStorage(const GameConfig& config, const std::filesystem::path& base_dir) {
    namespace fs = std::filesystem;
    StorageStatus status;
    std::error_code ec;
    const fs::path romfs(config.storage.romfs_path);
    status.romfs_dir = (romfs.is_absolute() ? romfs : base_dir / romfs).lexically_normal();
    status.trpfd_found = fs::is_regular_file(status.romfs_dir / "arc" / "data.trpfd", ec);
    status.trpfs_found = fs::is_regular_file(status.romfs_dir / "arc" / "data.trpfs", ec);
    status.romfs_ready = status.trpfd_found && status.trpfs_found;

    std::set<fs::path> seen;
    for (const fs::path& dir : {base_dir, base_dir.parent_path()}) {
        if (dir.empty() || !fs::is_directory(dir, ec)) continue;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
            if (!it->is_regular_file(ec) || Lower(it->path().extension().string()) != ".nsp") continue;
            const fs::path canonical = fs::weakly_canonical(it->path(), ec);
            if (!seen.insert(canonical).second) continue;
            status.nsp_files.push_back({it->path(), static_cast<uint64_t>(it->file_size(ec))});
        }
    }
    std::sort(status.nsp_files.begin(), status.nsp_files.end(), [](const NspFile& a, const NspFile& b) { return a.path < b.path; });
    return status;
}

} // namespace za
