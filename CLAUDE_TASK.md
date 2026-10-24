# ESCOPO PARA CLAUDE CODE — FASE 13: CONFIGURADOR INICIAL E GERENCIADOR DE CONFIGURAÇÕES (PC PORT)

## 1. Contexto e Alinhamento
Excelente trabalho nas entregas anteriores (`LearnsetTable`, `stat_stages.hpp`, `combat_rules_test.cpp` com 11/11)!
Agora que o motor já possui combate com barras de HP, telemetria de 60 FPS e logging de crash, o próximo passo essencial para a experiência do jogador no PC é o **Configurador Inicial (`za_configurator`)** e o **Sistema de Persistência de Configurações (`za_config.ini`)**.

---

## 2. Seu Domínio de Propriedade Exclusivo nesta Fase
Para respeitar estritamente o `ORCHESTRATION_MANIFEST.md`, seus arquivos de trabalho são:
- `include/config/game_config.hpp` (Estrutura de dados `GameConfig`, valores padrão, métodos de serialização/desserialização INI)
- `src/config/game_config.cpp` (Implementação do leitor e escritor do arquivo `za_config.ini`)
- `src/tools/configurator_main.cpp` (Executável interativo de configuração `za_configurator.exe` via console/Win32)
- `tests/config_test.cpp` (Suíte de testes cobrindo parsing, escrita, validação de limites e detecção de arquivos)

> **Nota:** Não altere `src/main.cpp` ou `src/services/nvn.cpp`. A integração com o loop principal do `za_port.exe` será feita pelo Antigravity após seus testes passarem.

---

## 3. Especificação Técnica Detalhada

### A. Estrutura de Configurações (`include/config/game_config.hpp`)
Crie a estrutura `za::GameConfig` com as seguintes seções canônicas:

```cpp
namespace za {

struct StorageConfig {
    std::string romfs_path = "../romfs";
    bool auto_clean_nsp = false;      // Pergunta ou remove .nsp apos confirmacao
};

struct GraphicsConfig {
    int window_width = 1280;          // 1280, 1920, 2560, 3840
    int window_height = 720;          // 720, 1080, 1440, 2160
    std::string display_mode = "windowed"; // "windowed", "borderless", "fullscreen"
    std::string shading = "solid";    // "solid", "wireframe"
    std::string initial_camera = "third_person"; // "third_person", "free_fly"
    bool vsync = true;                // true = 60 FPS fixo, false = livre
};

struct PerformanceConfig {
    int target_fps = 60;              // 30, 60, 120, 144, 0 (ilimitado)
    int oodle_cache_kb = 64;          // 64, 128, 256, 512 KB
    std::string log_level = "INFO";   // "DEBUG", "INFO", "WARN", "ERROR"
    bool enable_profiler = true;      // Gera za_performance.log ao sair
};

struct AudioConfig {
    int master_volume = 100;          // 0 a 100%
    int bgm_volume = 80;              // 0 a 100%
    int sfx_volume = 90;              // 0 a 100%
    bool bgm_enabled = true;
};

struct InputConfig {
    std::string device_preference = "auto"; // "auto", "keyboard_mouse", "gamepad_only"
    float mouse_sensitivity = 1.0f;         // 0.2f a 5.0f
    bool invert_mouse_y = false;
};

struct GameConfig {
    StorageConfig storage;
    GraphicsConfig graphics;
    PerformanceConfig performance;
    AudioConfig audio;
    InputConfig input;

    // Metodos necessarios:
    bool LoadFromFile(const std::string& filepath);
    bool SaveToFile(const std::string& filepath) const;
    void SetDefaults();
    void ValidateAndClamp(); // Garante limites seguros (ex: volume entre 0 e 100, resolucao minima 640x360)
};

} // namespace za
```

### B. Formato de Persistência INI (`za_config.ini`)
O leitor e gravador em `src/config/game_config.cpp` deve gerar e ler um arquivo `.ini` legível e limpo:

```ini
[Storage]
romfs_path=../romfs
auto_clean_nsp=false

[Graphics]
window_width=1280
window_height=720
display_mode=windowed
shading=solid
initial_camera=third_person
vsync=true

[Performance]
target_fps=60
oodle_cache_kb=64
log_level=INFO
enable_profiler=true

[Audio]
master_volume=100
bgm_volume=80
sfx_volume=90
bgm_enabled=true

[Input]
device_preference=auto
mouse_sensitivity=1.000000
invert_mouse_y=false
```

### C. O Configurador Interativo (`src/tools/configurator_main.cpp`)
O executável `za_configurator.exe` deve ser leve, sem dependências externas (apenas C++ padrão e Win32 se necessário):
1. **Verificação de ROM / .NSP:**
   - Checa se `romfs_path/arc/data.trpfs` existe.
   - Procura arquivos `*.nsp` na pasta raiz (`../` ou `./`).
   - Se encontrar um `.nsp` e o RomFS já estiver extraído, avisa o usuário que pode economizar 4.3 GB liberando o `.nsp`.
2. **Menu Interativo (TUI / Console limpo e colorido via Windows console):**
   - `[1] Gerenciar Armazenamento & RomFS (.nsp)`
   - `[2] Configurações Gráficas (Resolução, Modo de Exibição, VSync)`
   - `[3] Configurações de Desempenho (FPS Alvo, Cache Oodle, Telemetria)`
   - `[4] Configurações de Áudio (Volumes Master, BGM, SFX)`
   - `[5] Configurações de Controles (Sensibilidade do Mouse, Inverter Eixo)`
   - `[6] Restaurar Padrões`
   - `[7] Salvar e Sair`
3. **Suporte a Linha de Comando:**
   - `--check`: Apenas valida arquivos e configurações e retorna 0 se tudo estiver pronto para jogar.
   - `--reset`: Restaura `za_config.ini` para os valores padrão e sai.
   - `--set <secao.chave> <valor>`: Permite scripts automatizados ajustarem configurações diretamente.

### D. Suíte de Testes Unitários (`tests/config_test.cpp`)
Deve conter pelo menos 10 testes validando:
1. Valores padrão (`SetDefaults`)
2. Gravação de arquivo `za_config_test.ini`
3. Leitura do arquivo gravado e correspondência exata de valores
4. Tolerância a espaços em branco, comentários com `#` ou `;` e chaves desconhecidas no INI
5. Clamp de valores inválidos (ex: volume -10 -> 0, volume 150 -> 100)
6. Clamp de resolução (mínimo 640x360)
7. Validação de caminhos de armazenamento
8. Opções de display_mode ("windowed", "fullscreen", "borderless")
9. Reset para os padrões de fábrica
10. Detecção simulada de arquivo `.nsp`

---

## 4. Como Compilar os Testes e a Ferramenta

```powershell
# Compilar a suíte de testes:
g++ -std=c++20 -Wall -Wextra -Iinclude src/config/game_config.cpp tests/config_test.cpp -o tests/config_test.exe

# Compilar o configurador standalone:
g++ -std=c++20 -O2 -Wall -Wextra -Iinclude src/config/game_config.cpp src/tools/configurator_main.cpp -o za_configurator.exe
```

Boa sorte! Estamos prontos para integrar a leitura do `za_config.ini` no `main.cpp` assim que seus testes estiverem verdes!
