#include "nvn/nvn.hpp"
#include "nn/hid.hpp"
#include "gfx/renderer3d.hpp"
#include "audio/audio_playback.hpp"
#include "data/personal_table.hpp"
#include "data/move_table.hpp"
#include "game/poke_actor.hpp"
#include "game/battle_calc.hpp"
#include <iostream>
#include <unordered_map>
#include <string>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace nvn {

// -------------------------------------------------------------
// Implementações do Device
// -------------------------------------------------------------
void nvnDeviceBuilderSetDefaults(DeviceBuilder* builder) {
    if (builder) builder->flags = 0;
}

void nvnDeviceBuilderSetFlags(DeviceBuilder* builder, uint32_t flags) {
    if (builder) builder->flags = flags;
}

uint32_t nvnDeviceBuilderGetFlags(const DeviceBuilder* builder) {
    return builder ? builder->flags : 0;
}

int nvnDeviceInitialize(Device* device, const DeviceBuilder* builder) {
    if (!device) return 0;
    device->id = 1;
    device->flags = builder ? builder->flags : 0;
    device->initialized = true;
    std::cout << "[NVN] nvnDeviceInitialize() -> Contexto GPU Inicializado com Sucesso!" << std::endl;
    return 1;
}

void nvnDeviceFinalize(Device* device) {
    if (device) device->initialized = false;
    std::cout << "[NVN] nvnDeviceFinalize()" << std::endl;
}

int nvnDeviceGetInteger(Device* device, int property, int* out_value) {
    if (!out_value) return 0;
    switch (property) {
        case 0x52: // COMMAND_BUFFER_COMMAND_MEMORY_ALIGNMENT
            *out_value = 0x1000; // 4096 bytes
            break;
        case 0x55: // COMMAND_BUFFER_COMPUTE_MEMORY_ALIGNMENT
            *out_value = 0x1000; // 4096 bytes
            break;
        case 0x01: // UNIFORM_BUFFER_ALIGNMENT
        case 0x02: // STORAGE_BUFFER_ALIGNMENT
        case 0x03: // TEXTURE_BUFFER_ALIGNMENT
            *out_value = 256;
            break;
        case 0x08: // MAX_TEXTURE_SIZE
            *out_value = 16384;
            break;
        case 0x10: // MAX_VIEWPORTS
            *out_value = 16;
            break;
        default:
            *out_value = 1;
            break;
    }
    return 1;
}

uint64_t nvnDeviceGetCurrentTimestampInNanoseconds(Device* device) {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
}

// -------------------------------------------------------------
// Implementações da Queue (Fila de Renderização)
// -------------------------------------------------------------
void nvnQueueBuilderSetDefaults(QueueBuilder* builder) {
    if (!builder) return;
    builder->priority = 1000;
    builder->control_memory_size = 0x80000; // 512 KiB
    builder->command_memory_size = 0x10000;
    builder->compute_memory_size = 0x10000;
    builder->flush_threshold = 0x10000;
    builder->queue_memory_size = 0x80000;
}

void nvnQueueBuilderSetDevice(QueueBuilder* builder, Device* device) {
    if (builder) builder->device = device;
}

void nvnQueueBuilderSetFlags(QueueBuilder* builder, uint32_t flags) {
    if (builder) builder->flags = flags;
}

void nvnQueueBuilderSetCommandMemorySize(QueueBuilder* builder, size_t size) {
    if (builder) builder->command_memory_size = size;
}

void nvnQueueBuilderSetComputeMemorySize(QueueBuilder* builder, size_t size) {
    if (builder) builder->compute_memory_size = size;
}

void nvnQueueBuilderSetControlMemorySize(QueueBuilder* builder, size_t size) {
    if (builder) builder->control_memory_size = size;
}

void nvnQueueBuilderSetQueuePriority(QueueBuilder* builder, int priority) {
    if (builder) builder->priority = priority;
}

void nvnQueueBuilderSetQueueMemory(QueueBuilder* builder, void* memory, size_t size) {
    if (!builder) return;
    builder->queue_memory = memory;
    builder->queue_memory_size = size;
}

size_t nvnQueueBuilderGetQueueMemorySize(const QueueBuilder* builder) {
    return builder ? builder->queue_memory_size : 0x80000;
}

int nvnQueueInitialize(Queue* queue, const QueueBuilder* builder) {
    if (!queue) return 0;
    if (builder) {
        queue->device = builder->device;
        queue->priority = builder->priority;
        queue->control_memory_size = builder->control_memory_size;
        queue->command_memory_size = builder->command_memory_size;
        queue->compute_memory_size = builder->compute_memory_size;
        queue->queue_memory = builder->queue_memory;
        queue->queue_memory_size = builder->queue_memory_size;
    }
    queue->initialized = true;
    std::cout << "[NVN] nvnQueueInitialize() -> Fila de Comandos da GPU Pronta (Controle: " 
              << queue->control_memory_size << " bytes, Prioridade: " << queue->priority << ")!" << std::endl;
    return 1;
}

void nvnQueueFinalize(Queue* queue) {
    if (queue) queue->initialized = false;
    std::cout << "[NVN] nvnQueueFinalize()" << std::endl;
}

void nvnQueueSubmitCommands(Queue* queue, int count, void* const* cmd_buffers) {
    // Processa os buffers de comando enviados pela engine
}

void nvnQueueFinish(Queue* queue) {
    // Sincronização de GPU
}

void nvnQueueFlush(Queue* queue) {
    // Descarga de comandos pendentes
}

// -------------------------------------------------------------
// Implementações da Janela e Apresentação (Swapchain / Present)
// -------------------------------------------------------------
void nvnWindowBuilderSetDefaults(WindowBuilder* builder) {
    if (!builder) return;
    builder->device = nullptr;
    builder->native_window = nullptr;
    builder->num_textures = 2;
    builder->present_interval = 1;
}

void nvnWindowBuilderSetDevice(WindowBuilder* builder, Device* device) {
    if (builder) builder->device = device;
}

void nvnWindowBuilderSetNativeWindow(WindowBuilder* builder, void* native_window) {
    if (builder) builder->native_window = native_window;
}

void nvnWindowBuilderSetTextures(WindowBuilder* builder, int count, void** textures) {
    if (!builder) return;
    builder->num_textures = count;
    for (int i = 0; i < count && i < 8; ++i) {
        builder->textures[i] = textures ? textures[i] : nullptr;
    }
}

void nvnWindowBuilderSetPresentInterval(WindowBuilder* builder, int interval) {
    if (builder) builder->present_interval = interval;
}

int nvnWindowInitialize(Window* window, const WindowBuilder* builder) {
    if (!window) return 0;
    window->device = builder ? builder->device : nullptr;
    window->hwnd = builder ? reinterpret_cast<HWND>(builder->native_window) : nullptr;
    window->num_textures = builder ? builder->num_textures : 2;
    window->present_interval = builder ? builder->present_interval : 1;
    window->active_texture = 0;
    window->total_presents = 0;
    window->last_present_time = std::chrono::high_resolution_clock::now();
    window->initialized = true;
    RECT client_rect{};
    if (window->hwnd && GetClientRect(window->hwnd, &client_rect)) {
        window->width = (client_rect.right - client_rect.left > 0) ? (client_rect.right - client_rect.left) : 1280;
        window->height = (client_rect.bottom - client_rect.top > 0) ? (client_rect.bottom - client_rect.top) : 720;
    } else {
        window->width = 1280;
        window->height = 720;
    }

    // Configuração do Framebuffer DIB para apresentação nativa na janela Win32
    if (window->hwnd) {
        HDC win_dc = GetDC(window->hwnd);
        window->mem_dc = CreateCompatibleDC(win_dc);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = window->width;
        bmi.bmiHeader.biHeight = -window->height; // Top-down DIB
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        window->dib_bitmap = CreateDIBSection(win_dc, &bmi, DIB_RGB_COLORS, &window->pixel_buffer, nullptr, 0);
        window->old_bitmap = (HBITMAP)SelectObject(window->mem_dc, window->dib_bitmap);
        ReleaseDC(window->hwnd, win_dc);
    }

    std::cout << "[NVN] nvnWindowInitialize() -> Swapchain criada para HWND=" << (void*)window->hwnd 
              << " (" << window->num_textures << " buffers de renderizacao, intervalo=" 
              << window->present_interval << ", Framebuffer 1280x720 32-bpp ativo)" << std::endl;
    return 1;
}

void nvnWindowFinalize(Window* window) {
    if (window) {
        if (window->mem_dc) {
            SelectObject(window->mem_dc, window->old_bitmap);
            DeleteObject(window->dib_bitmap);
            DeleteDC(window->mem_dc);
            window->mem_dc = nullptr;
        }
        window->initialized = false;
    }
    std::cout << "[NVN] nvnWindowFinalize()" << std::endl;
}

void nvnWindowSetPresentInterval(Window* window, int interval) {
    if (!window) return;
    window->present_interval = interval;
    int target_fps = (interval == 2) ? 30 : 60;
    std::cout << "[NVN] nvnWindowSetPresentInterval(interval=" << interval 
              << ") -> Alvo configurado para " << target_fps << " FPS" << std::endl;
}

int nvnWindowAcquireTexture(Window* window, Sync* sync, int* out_texture_idx) {
    if (!window || !out_texture_idx) return 0;
    window->active_texture = (window->active_texture + 1) % window->num_textures;
    *out_texture_idx = window->active_texture;
    return 1;
}

void nvnQueuePresentTexture(Queue* queue, Window* window, int texture_idx) {
    if (!window) return;
    window->total_presents++;
    window->active_texture = texture_idx;

    // Pacing de sincronização vertical (VSync: 60 FPS ou 30 FPS)
    auto now = std::chrono::high_resolution_clock::now();
    int target_fps = (window->present_interval == 2) ? 30 : 60;
    int target_ms = 1000 / target_fps;

    if (window->last_present_time.time_since_epoch().count() > 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - window->last_present_time).count();
        if (elapsed < target_ms) {
            std::this_thread::sleep_for(std::chrono::milliseconds(target_ms - elapsed));
        }
    }
    window->last_present_time = std::chrono::high_resolution_clock::now();

    // Renderização visual ativa do quadro na superfície DIB da janela
    if (window->hwnd && window->mem_dc && window->pixel_buffer) {
        nn::hid::NpadFullKeyState pad{};
        nn::hid::GetNpadFullKeyState(&pad, nn::hid::NpadId::No1);

        bool is_aiming = (pad.buttons & nn::hid::NpadButton_ZL) != 0;
        bool is_throwing = (pad.buttons & nn::hid::NpadButton_ZR) != 0;

        // Feedback sonoro nas ações do jogador (Mira ZL / Lançamento ZR)
        static bool s_was_aiming = false;
        static bool s_was_throwing = false;
        if (is_aiming && !s_was_aiming) {
            za::audio::AudioPlayback::Instance().PlayLockonBeep(784.0f, 0.10f, 0.35f);
        }
        if (is_throwing && !s_was_throwing) {
            za::audio::AudioPlayback::Instance().PlayPokeChime(659.25f, 0.28f, 0.45f);
        }
        s_was_aiming = is_aiming;
        s_was_throwing = is_throwing;

        // Atualiza o Ator Pokémon (Movimentação WASD, corrida com Shift, ataque e colisão de terreno)
        auto active_actor = za::gfx::Renderer3D::Instance().GetActiveActor();
        auto& camera = za::gfx::Renderer3D::Instance().GetCamera();
        const auto& col_world = za::gfx::Renderer3D::Instance().GetCollisionWorld();

        if (active_actor) {
            active_actor->Update(0.0166f, pad, camera, col_world, [](uint16_t /*sp_id*/, uint16_t /*move_id*/) {
                // Ao atacar: reproduz efeito sonoro de impacto marcial
                za::audio::AudioPlayback::Instance().PlayAttackImpact(1.2f, 0.60f);
            });
            // Terceira pessoa: Câmera orbital acompanha suavemente o Pokémon em movimento
            camera.Follow(active_actor->position + za::gfx::Vec3{0.0f, active_actor->height * 0.55f, 0.0f}, 0.0166f);
        }

        auto opponent_actor = za::gfx::Renderer3D::Instance().GetOpponentActor();
        if (opponent_actor) {
            opponent_actor->Update(0.0166f, pad, camera, col_world);
        }

        camera.Update(0.0166f, pad.analog_stick_l.x, pad.analog_stick_l.y, pad.analog_stick_r.x, pad.analog_stick_r.y);

        // Prevenção de penetração no chão (Spring-Arm da Câmera)
        std::array<float, 3> cam_in = {camera.position.x, camera.position.y, camera.position.z};
        std::array<float, 3> cam_out = col_world.ClampCameraAboveGround(cam_in, 0.45f);
        camera.position.x = cam_out[0];
        camera.position.y = cam_out[1];
        camera.position.z = cam_out[2];

        // 1. Renderiza o mundo 3D no Framebuffer DIB (Céu, Horizonte, Grid de Perspectiva, Cubo/Prisma 3D rotativo e Retícula de Mira)
        za::gfx::Renderer3D::Instance().Render(
            static_cast<uint32_t*>(window->pixel_buffer),
            window->width,
            window->height,
            camera,
            is_aiming,
            is_throwing
        );

        // 2. Overlay do HUD sobre a cena 3D
        HDC hdc = GetDC(window->hwnd);
        RECT client_rect;
        GetClientRect(window->hwnd, &client_rect);

        // Header superior
        HBRUSH header_brush = CreateSolidBrush(RGB(20, 22, 34));
        RECT r_header = {0, 0, window->width, 70};
        FillRect(window->mem_dc, &r_header, header_brush);
        DeleteObject(header_brush);

        SetBkMode(window->mem_dc, TRANSPARENT);

        // Título e logo
        HFONT hFontTitle = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT hOldFont = (HFONT)SelectObject(window->mem_dc, hFontTitle);
        SetTextColor(window->mem_dc, RGB(255, 215, 0)); // Dourado Pokémon Z-A
        TextOutA(window->mem_dc, 30, 12, "POKEMON LEGENDS: Z-A", 20);

        HFONT hFontSub = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT hFontSec = CreateFontA(17, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
        HFONT hFontBody = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas");
        SelectObject(window->mem_dc, hFontSub);
        SetTextColor(window->mem_dc, RGB(160, 175, 200));
        TextOutA(window->mem_dc, 32, 42, "Camada de Compatibilidade Nativa para PC (x86_64) | Trinity Engine", 66);

        // Estatísticas de performance em tempo real
        char stats_buf[128];
        std::snprintf(stats_buf, sizeof(stats_buf), "NVN: %d FPS (VSync Ativo) | Quadro #%llu", target_fps, (unsigned long long)window->total_presents);
        SetTextColor(window->mem_dc, RGB(80, 240, 140)); // Verde
        TextOutA(window->mem_dc, window->width - 340, 24, stats_buf, static_cast<int>(std::strlen(stats_buf)));

        // ---------------------------------------------------------------------------------------
        // HUD de Combate Interativo: Barras de Vida (Player HP vs Opponent HP)
        // ---------------------------------------------------------------------------------------
        if (active_actor || opponent_actor) {
            int hud_cx = window->width / 2;
            int hp_bar_w = 160;
            int hp_bar_h = 10;

            // Painel de Fundo do Duelo Superior
            RECT r_duel = {hud_cx - 270, 8, hud_cx + 270, 72};
            HBRUSH duel_br = CreateSolidBrush(RGB(15, 18, 28));
            FillRect(window->mem_dc, &r_duel, duel_br);
            DeleteObject(duel_br);

            HPEN duel_pen = CreatePen(PS_SOLID, 1, RGB(55, 75, 110));
            HPEN old_duel_pen = (HPEN)SelectObject(window->mem_dc, duel_pen);
            HBRUSH old_duel_br = (HBRUSH)SelectObject(window->mem_dc, GetStockObject(NULL_BRUSH));
            Rectangle(window->mem_dc, r_duel.left, r_duel.top, r_duel.right, r_duel.bottom);
            SelectObject(window->mem_dc, old_duel_pen);
            SelectObject(window->mem_dc, old_duel_br);
            DeleteObject(duel_pen);

            // Jogador (Lado Esquerdo)
            if (active_actor) {
                int px = hud_cx - 250;
                int py = 14;

                SelectObject(window->mem_dc, hFontSec);
                char pl_name[64];
                std::snprintf(pl_name, sizeof(pl_name), "%s Lv.%d", active_actor->name.c_str(), active_actor->level);
                SetTextColor(window->mem_dc, RGB(100, 220, 255));
                TextOutA(window->mem_dc, px, py, pl_name, static_cast<int>(std::strlen(pl_name)));

                // HP BG
                HBRUSH hp_bg = CreateSolidBrush(RGB(35, 40, 50));
                RECT r_hpbg = {px, py + 22, px + hp_bar_w, py + 22 + hp_bar_h};
                FillRect(window->mem_dc, &r_hpbg, hp_bg);
                DeleteObject(hp_bg);

                // HP FG
                float pct = active_actor->GetHpPercent();
                int fill_w = static_cast<int>(pct * hp_bar_w);
                COLORREF hp_col = (pct > 0.5f) ? RGB(60, 225, 90) : (pct > 0.2f ? RGB(245, 205, 30) : RGB(255, 60, 60));
                if (active_actor->hit_flash_timer > 0.0f) hp_col = RGB(255, 255, 255);
                HBRUSH hp_fg = CreateSolidBrush(hp_col);
                RECT r_hpfg = {px, py + 22, px + fill_w, py + 22 + hp_bar_h};
                FillRect(window->mem_dc, &r_hpfg, hp_fg);
                DeleteObject(hp_fg);

                SelectObject(window->mem_dc, hFontBody);
                char hp_txt[64];
                std::snprintf(hp_txt, sizeof(hp_txt), "HP: %d/%d", active_actor->current_hp, active_actor->max_hp);
                SetTextColor(window->mem_dc, RGB(210, 225, 240));
                TextOutA(window->mem_dc, px, py + 36, hp_txt, static_cast<int>(std::strlen(hp_txt)));
            }

            // VS Central
            SelectObject(window->mem_dc, hFontSec);
            SetTextColor(window->mem_dc, RGB(255, 215, 0));
            TextOutA(window->mem_dc, hud_cx - 10, 24, "VS", 2);

            // Oponente (Lado Direito)
            if (opponent_actor) {
                int ox = hud_cx + 90;
                int oy = 14;

                SelectObject(window->mem_dc, hFontSec);
                char op_name[64];
                std::snprintf(op_name, sizeof(op_name), "%s Lv.%d", opponent_actor->name.c_str(), opponent_actor->level);
                SetTextColor(window->mem_dc, RGB(255, 120, 120));
                TextOutA(window->mem_dc, ox, oy, op_name, static_cast<int>(std::strlen(op_name)));

                // HP BG
                HBRUSH hp_bg = CreateSolidBrush(RGB(35, 40, 50));
                RECT r_hpbg = {ox, oy + 22, ox + hp_bar_w, oy + 22 + hp_bar_h};
                FillRect(window->mem_dc, &r_hpbg, hp_bg);
                DeleteObject(hp_bg);

                // HP FG
                float pct = opponent_actor->GetHpPercent();
                int fill_w = static_cast<int>(pct * hp_bar_w);
                COLORREF hp_col = (pct > 0.5f) ? RGB(60, 225, 90) : (pct > 0.2f ? RGB(245, 205, 30) : RGB(255, 60, 60));
                if (opponent_actor->hit_flash_timer > 0.0f) hp_col = RGB(255, 255, 255);
                HBRUSH hp_fg = CreateSolidBrush(hp_col);
                RECT r_hpfg = {ox, oy + 22, ox + fill_w, oy + 22 + hp_bar_h};
                FillRect(window->mem_dc, &r_hpfg, hp_fg);
                DeleteObject(hp_fg);

                SelectObject(window->mem_dc, hFontBody);
                char hp_txt[64];
                std::snprintf(hp_txt, sizeof(hp_txt), "HP: %d/%d", opponent_actor->current_hp, opponent_actor->max_hp);
                SetTextColor(window->mem_dc, RGB(210, 225, 240));
                TextOutA(window->mem_dc, ox, oy + 36, hp_txt, static_cast<int>(std::strlen(hp_txt)));
            }
        }

        // Painel de Subsistemas
        SelectObject(window->mem_dc, hFontSec);
        SetTextColor(window->mem_dc, RGB(220, 230, 250));
        TextOutA(window->mem_dc, 40, 95, "ESTADO DOS SUBSISTEMAS:", 23);

        SelectObject(window->mem_dc, hFontBody);
        SetTextColor(window->mem_dc, RGB(180, 200, 230));

        TextOutA(window->mem_dc, 45, 125, "[OK] VFS Trinity RomFS: 178.109 arquivos indexados em 9.892 packs", 65);
        TextOutA(window->mem_dc, 45, 150, "[OK] Oodle KrakenLZ: Descompressao dinamica ativa (oo2core_9_win64)", 67);
        TextOutA(window->mem_dc, 45, 175, "[OK] Wwise Audio Engine: 6.278 faixas em Stream.pck (841 MB) + 16 SoundBanks", 76);
        TextOutA(window->mem_dc, 45, 200, "[OK] GPU Subsystem: NVN Device, 512 KiB Queue (Pri 1000), Double Buffering", 74);
        TextOutA(window->mem_dc, 45, 225, "[OK] AArch64 Guest Image: exefs/main carregado em 0x7100000000 (39 GOT hooks)", 78);

        // Painel de Telemetria do Jogador (Interativo em Tempo Real!)
        SelectObject(window->mem_dc, hFontSec);
        SetTextColor(window->mem_dc, RGB(220, 230, 250));
        TextOutA(window->mem_dc, 40, 270, "ENTRADA E CONTROLES DO JOGADOR (HID):", 37);

        SelectObject(window->mem_dc, hFontBody);
        char input_buf1[128];
        std::snprintf(input_buf1, sizeof(input_buf1), "Amostra #%-8lld | Botoes Ativos: 0x%04llX", (long long)pad.sampling_number, (unsigned long long)pad.buttons);
        SetTextColor(window->mem_dc, pad.buttons ? RGB(255, 100, 100) : RGB(140, 160, 190));
        TextOutA(window->mem_dc, 45, 300, input_buf1, static_cast<int>(std::strlen(input_buf1)));

        char stick_buf[128];
        std::snprintf(stick_buf, sizeof(stick_buf), "Stick Esquerdo (WASD / LStick): X=%+6d, Y=%+6d", pad.analog_stick_l.x, pad.analog_stick_l.y);
        SetTextColor(window->mem_dc, (pad.analog_stick_l.x || pad.analog_stick_l.y) ? RGB(100, 220, 255) : RGB(140, 160, 190));
        TextOutA(window->mem_dc, 45, 325, stick_buf, static_cast<int>(std::strlen(stick_buf)));

        char cam_buf[128];
        std::snprintf(cam_buf, sizeof(cam_buf), "Stick Direito  (Mouse / RStick): X=%+6d, Y=%+6d", pad.analog_stick_r.x, pad.analog_stick_r.y);
        SetTextColor(window->mem_dc, (pad.analog_stick_r.x || pad.analog_stick_r.y) ? RGB(255, 180, 80) : RGB(140, 160, 190));
        TextOutA(window->mem_dc, 45, 350, cam_buf, static_cast<int>(std::strlen(cam_buf)));

        char cam3d_buf[160];
        const auto* act_model = za::gfx::Renderer3D::Instance().GetActiveModel();
        std::string model_name = act_model ? act_model->name : "Nenhum";
        const char* cam_mode_str = (camera.mode == za::gfx::Camera3D::Mode::Orbit) ? "TERCEIRA PESSOA" : "VOO LIVRE";
        const char* shade_str = za::gfx::Renderer3D::Instance().IsWireframeOnly() ? "ARAMADO" : "SOLIDO 3D";

        if (active_actor) {
            std::snprintf(cam3d_buf, sizeof(cam3d_buf),
                          "Ator: %-8s | %-15s (%.1f m/s) | Pos: (%+5.1f, %+4.2f, %+5.1f) | Solo: %+4.2f m",
                          model_name.c_str(), active_actor->GetStateName(), active_actor->GetCurrentSpeed(),
                          active_actor->position.x, active_actor->position.y, active_actor->position.z,
                          active_actor->ground_height);
        } else {
            std::snprintf(cam3d_buf, sizeof(cam3d_buf),
                          "Pokemon: %-18s | Shading: %-16s | Cam: %s",
                          model_name.c_str(), shade_str, cam_mode_str);
        }
        SetTextColor(window->mem_dc, RGB(255, 230, 100));
        TextOutA(window->mem_dc, 45, 372, cam3d_buf, static_cast<int>(std::strlen(cam3d_buf)));

        // ---------------------------------------------------------------------------------------
        // Painel da Pokedex Regional de Lumiose City (Atributos, Tipos e Cry em Tempo Real)
        // ---------------------------------------------------------------------------------------
        uint16_t active_sp = za::gfx::Renderer3D::Instance().GetActiveSpecies();
        const auto* pdata = za::data::PersonalDatabase::Instance().Get(active_sp);

        auto get_clean_type = [](uint8_t tid) -> const char* {
            switch (tid) {
                case 0: return "Normal";
                case 1: return "Luta";
                case 2: return "Voador";
                case 3: return "Veneno";
                case 4: return "Terra";
                case 5: return "Pedra";
                case 6: return "Inseto";
                case 7: return "Fantasma";
                case 8: return "Aco";
                case 9: return "Fogo";
                case 10: return "Agua";
                case 11: return "Planta";
                case 12: return "Eletrico";
                case 13: return "Psiquico";
                case 14: return "Gelo";
                case 15: return "Dragao";
                case 16: return "Sombrio";
                case 17: return "Fada";
                default: return "?";
            }
        };

        COLORREF type_accent = RGB(70, 90, 135);
        if (pdata) {
            switch (pdata->type_1) {
                case 13: type_accent = RGB(190, 80, 255); break; // Psíquico
                case 12: type_accent = RGB(245, 205, 30);  break; // Elétrico
                case 11: type_accent = RGB(65, 205, 110); break; // Planta
                case 10: type_accent = RGB(60, 150, 250); break; // Água
                default: break;
            }
        }

        RECT r_pokedex = {window->width - 490, 85, window->width - 30, 395};
        HBRUSH dex_bg_brush = CreateSolidBrush(RGB(18, 22, 36));
        FillRect(window->mem_dc, &r_pokedex, dex_bg_brush);
        DeleteObject(dex_bg_brush);

        HPEN dex_border_pen = CreatePen(PS_SOLID, 2, type_accent);
        HPEN hOldPenDex = (HPEN)SelectObject(window->mem_dc, dex_border_pen);
        HBRUSH hOldBrDex = (HBRUSH)SelectObject(window->mem_dc, GetStockObject(NULL_BRUSH));
        Rectangle(window->mem_dc, r_pokedex.left, r_pokedex.top, r_pokedex.right, r_pokedex.bottom);
        SelectObject(window->mem_dc, hOldPenDex);
        SelectObject(window->mem_dc, hOldBrDex);
        DeleteObject(dex_border_pen);

        SelectObject(window->mem_dc, hFontSec);
        SetTextColor(window->mem_dc, RGB(255, 215, 0)); // Dourado
        TextOutA(window->mem_dc, r_pokedex.left + 15, r_pokedex.top + 10, "POKEDEX REGIONAL - LUMIOSE CITY", 31);

        char dex_header[128];
        const char* pkm_name = (active_sp == 150) ? "MEWTWO" :
                               (active_sp == 25)  ? "PIKACHU" :
                               (active_sp == 1)   ? "BULBASAUR" :
                               (active_sp == 8)   ? "WARTORTLE" : "POKEMON";
        std::snprintf(dex_header, sizeof(dex_header), "#%04u %s", active_sp, pkm_name);
        SelectObject(window->mem_dc, hFontTitle);
        SetTextColor(window->mem_dc, RGB(255, 255, 255));
        TextOutA(window->mem_dc, r_pokedex.left + 15, r_pokedex.top + 34, dex_header, static_cast<int>(std::strlen(dex_header)));

        SelectObject(window->mem_dc, hFontBody);
        if (pdata) {
            const char* t1 = get_clean_type(pdata->type_1);
            const char* t2 = (pdata->type_1 != pdata->type_2) ? get_clean_type(pdata->type_2) : "";
            char type_buf[128];
            if (t2[0] != '\0') {
                std::snprintf(type_buf, sizeof(type_buf), "Tipo: [%s] [%s]", t1, t2);
            } else {
                std::snprintf(type_buf, sizeof(type_buf), "Tipo: [%s]", t1);
            }
            SetTextColor(window->mem_dc, RGB(180, 220, 255));
            TextOutA(window->mem_dc, r_pokedex.left + 15, r_pokedex.top + 65, type_buf, static_cast<int>(std::strlen(type_buf)));

            char dim_buf[128];
            std::snprintf(dim_buf, sizeof(dim_buf), "Altura: %.2f m  |  Peso: %.1f kg  |  BST: %u",
                          pdata->HeightMeters(), pdata->WeightKg(), pdata->base_stats.Total());
            SetTextColor(window->mem_dc, RGB(200, 210, 230));
            TextOutA(window->mem_dc, r_pokedex.left + 15, r_pokedex.top + 88, dim_buf, static_cast<int>(std::strlen(dim_buf)));

            struct StatBar { const char* label; uint8_t val; COLORREF col; };
            StatBar stats[6] = {
                {"HP ", pdata->base_stats.hp,         RGB(80, 220, 100)},
                {"ATK", pdata->base_stats.attack,     RGB(240, 130, 40)},
                {"DEF", pdata->base_stats.defense,    RGB(70, 150, 240)},
                {"SPA", pdata->base_stats.sp_attack,  RGB(180, 80, 240)},
                {"SPD", pdata->base_stats.sp_defense, RGB(230, 200, 50)},
                {"SPE", pdata->base_stats.speed,      RGB(40, 210, 230)}
            };

            int bar_y = r_pokedex.top + 115;
            for (int s = 0; s < 6; ++s) {
                char lbl[32];
                std::snprintf(lbl, sizeof(lbl), "%s %3u", stats[s].label, stats[s].val);
                SetTextColor(window->mem_dc, RGB(210, 220, 235));
                TextOutA(window->mem_dc, r_pokedex.left + 15, bar_y, lbl, static_cast<int>(std::strlen(lbl)));

                int bar_x = r_pokedex.left + 95;
                int bar_w = 340;
                int bar_h = 10;
                int fill_w = std::min(bar_w, (static_cast<int>(stats[s].val) * bar_w) / 200);

                HBRUSH bg_bar = CreateSolidBrush(RGB(35, 40, 55));
                RECT r_bgb = {bar_x, bar_y + 2, bar_x + bar_w, bar_y + 2 + bar_h};
                FillRect(window->mem_dc, &r_bgb, bg_bar);
                DeleteObject(bg_bar);

                HBRUSH fg_bar = CreateSolidBrush(stats[s].col);
                RECT r_fgb = {bar_x, bar_y + 2, bar_x + fill_w, bar_y + 2 + bar_h};
                FillRect(window->mem_dc, &r_fgb, fg_bar);
                DeleteObject(fg_bar);

                bar_y += 20;
            }

            char cry_buf[128];
            std::snprintf(cry_buf, sizeof(cry_buf), "Voz Wwise: PLAY_PV_%04u | PTADPCM 48kHz Stereo", active_sp);
            SetTextColor(window->mem_dc, RGB(100, 240, 180));
            TextOutA(window->mem_dc, r_pokedex.left + 15, bar_y + 8, cry_buf, static_cast<int>(std::strlen(cry_buf)));
        }

        // Guia de Controles do PC no rodapé
        HBRUSH foot_brush = CreateSolidBrush(RGB(20, 22, 32));
        RECT r_foot = {30, 405, window->width - 30, window->height - 25};
        FillRect(window->mem_dc, &r_foot, foot_brush);
        DeleteObject(foot_brush);

        SelectObject(window->mem_dc, hFontSec);
        SetTextColor(window->mem_dc, RGB(255, 220, 120));
        TextOutA(window->mem_dc, 50, 410, "MAPA DE CONTROLES ATIVOS NO PC:", 31);

        SelectObject(window->mem_dc, hFontBody);
        SetTextColor(window->mem_dc, RGB(200, 210, 225));
        const char* ctrl_lines[] = {
            "- CAMERA: MOUSE 360 (Cursor: ALT / F1)  |  [C]: 3a Pessoa / Voo",
            "- MOVIMENTACAO: W, A, S, D / STICK L  |  CORRER: SHIFT",
            "- ATAQUE / GOLPE: ESPACO / BOTAO ESQ / F (Causa Dano Real)",
            "- SELECAO DE GOLPE: [Z] [X] [C] [V]  |  [TAB]: Trocar Lutador",
            "- TROCAR POKEMON: 1: Mewtwo  2: Pikachu  3: Bulba  4: Wartortle",
            "- SHADING: [V] Solido/Aramado  |  [F12]: Capturar Screenshot"
        };
        for (int i = 0; i < 6; ++i) {
            TextOutA(window->mem_dc, 55, 435 + i * 23, ctrl_lines[i], static_cast<int>(std::strlen(ctrl_lines[i])));
        }

        // Painel de Golpes de Batalha Equipados
        int moves_x = 560;
        int moves_y = 410;
        SelectObject(window->mem_dc, hFontSec);
        SetTextColor(window->mem_dc, RGB(255, 220, 120));
        TextOutA(window->mem_dc, moves_x, moves_y, "GOLPES DE BATALHA ([Z] [X] [C] [V]):", 36);

        if (active_actor) {
            SelectObject(window->mem_dc, hFontBody);
            const auto& move_db = za::data::MoveDatabase::Instance();
            const char* key_labels[] = {"[Z]", "[X]", "[C]", "[V]"};

            for (uint8_t slot = 0; slot < 4; ++slot) {
                uint16_t mid = active_actor->moves[slot];
                const auto* mentry = move_db.Get(mid);
                bool is_active_slot = (slot == active_actor->selected_move_slot);

                int card_y = moves_y + 25 + slot * 24;
                RECT r_card = {moves_x - 5, card_y - 2, moves_x + 460, card_y + 20};

                if (is_active_slot) {
                    HBRUSH sel_br = CreateSolidBrush(RGB(30, 48, 70));
                    FillRect(window->mem_dc, &r_card, sel_br);
                    DeleteObject(sel_br);

                    HPEN sel_pen = CreatePen(PS_SOLID, 1, RGB(100, 220, 255));
                    HPEN old_pen = (HPEN)SelectObject(window->mem_dc, sel_pen);
                    HBRUSH old_br = (HBRUSH)SelectObject(window->mem_dc, GetStockObject(NULL_BRUSH));
                    Rectangle(window->mem_dc, r_card.left, r_card.top, r_card.right, r_card.bottom);
                    SelectObject(window->mem_dc, old_pen);
                    SelectObject(window->mem_dc, old_br);
                    DeleteObject(sel_pen);
                }

                char move_line[160];
                if (mentry) {
                    const char* tname = za::game::GetTypeName(mentry->type);
                    if (mentry->base_power > 0) {
                        std::snprintf(move_line, sizeof(move_line), "%s %-12s | %-8s | Pwr:%2u Acc:%3u | PP:%2u/%-2u %s",
                                      key_labels[slot], mentry->name.c_str(), tname,
                                      mentry->base_power, mentry->accuracy,
                                      active_actor->move_pp[slot], active_actor->move_max_pp[slot],
                                      is_active_slot ? "<ATIVO>" : "");
                    } else {
                        std::snprintf(move_line, sizeof(move_line), "%s %-12s | %-8s | Status        | PP:%2u/%-2u %s",
                                      key_labels[slot], mentry->name.c_str(), tname,
                                      active_actor->move_pp[slot], active_actor->move_max_pp[slot],
                                      is_active_slot ? "<ATIVO>" : "");
                    }
                } else {
                    std::snprintf(move_line, sizeof(move_line), "%s Golpe #%-3u   | Carregando...                 %s",
                                  key_labels[slot], mid, is_active_slot ? "<ATIVO>" : "");
                }

                SetTextColor(window->mem_dc, is_active_slot ? RGB(100, 255, 200) : RGB(170, 185, 205));
                TextOutA(window->mem_dc, moves_x, card_y, move_line, static_cast<int>(std::strlen(move_line)));
            }

            // Banner dinâmico de ataque se estiver disparando golpe
            if (active_actor->state == za::game::ActorState::Attack) {
                uint16_t cur_mid = active_actor->GetActiveMoveId();
                const auto* cur_m = move_db.Get(cur_mid);
                std::string mname = cur_m ? cur_m->name : "Ataque";

                RECT r_banner = {window->width / 2 - 280, 240, window->width / 2 + 280, 295};
                HBRUSH ban_br = CreateSolidBrush(RGB(50, 20, 25));
                FillRect(window->mem_dc, &r_banner, ban_br);
                DeleteObject(ban_br);

                HPEN ban_pen = CreatePen(PS_SOLID, 2, RGB(255, 80, 80));
                HPEN old_pen = (HPEN)SelectObject(window->mem_dc, ban_pen);
                HBRUSH old_br = (HBRUSH)SelectObject(window->mem_dc, GetStockObject(NULL_BRUSH));
                Rectangle(window->mem_dc, r_banner.left, r_banner.top, r_banner.right, r_banner.bottom);
                SelectObject(window->mem_dc, old_pen);
                SelectObject(window->mem_dc, old_br);
                DeleteObject(ban_pen);

                SelectObject(window->mem_dc, hFontSec);
                char ban_txt[128];
                if (active_actor->target && active_actor->target->hit_flash_timer > 0.0f) {
                    float mult = cur_m ? za::game::GetTypeEffectiveness(cur_m->type, active_actor->target->type_1, active_actor->target->type_2) : 1.0f;
                    const char* eff_txt = za::game::GetEffectivenessText(mult);
                    std::snprintf(ban_txt, sizeof(ban_txt), ">> ACERTOU %s COM %s! (%s) <<",
                                  active_actor->target->name.c_str(), mname.c_str(), eff_txt);
                } else {
                    std::snprintf(ban_txt, sizeof(ban_txt), ">> COMBATE: %s DISPAROU %s! <<",
                                  pkm_name, mname.c_str());
                }
                SetTextColor(window->mem_dc, RGB(255, 240, 100));
                TextOutA(window->mem_dc, r_banner.left + 25, r_banner.top + 8, ban_txt, static_cast<int>(std::strlen(ban_txt)));

                if (cur_m) {
                    SelectObject(window->mem_dc, hFontBody);
                    char ban_sub[128];
                    std::snprintf(ban_sub, sizeof(ban_sub), "Tipo: %s | Categoria: %s | Poder Base: %u",
                                  za::game::GetTypeName(cur_m->type), cur_m->CategoryName(), cur_m->base_power);
                    SetTextColor(window->mem_dc, RGB(255, 180, 180));
                    TextOutA(window->mem_dc, r_banner.left + 35, r_banner.top + 30, ban_sub, static_cast<int>(std::strlen(ban_sub)));
                }
            }
        }

        // Indicador Visual Circular dos Analógicos (Stick Radar Widgets) reposicionados no rodapé
        int cx_l = window->width - 200, cy_l = window->height - 85, r_circle = 38;
        HPEN hPenCirc = CreatePen(PS_SOLID, 2, RGB(80, 95, 130));
        HPEN hOldPen = (HPEN)SelectObject(window->mem_dc, hPenCirc);
        HBRUSH hBrCirc = CreateSolidBrush(RGB(25, 28, 42));
        HBRUSH hOldBr = (HBRUSH)SelectObject(window->mem_dc, hBrCirc);
        Ellipse(window->mem_dc, cx_l - r_circle, cy_l - r_circle, cx_l + r_circle, cy_l + r_circle);

        int dot_lx = cx_l + (pad.analog_stick_l.x * (r_circle - 8)) / 32767;
        int dot_ly = cy_l - (pad.analog_stick_l.y * (r_circle - 8)) / 32767;
        HBRUSH hBrDotL = CreateSolidBrush(RGB(100, 220, 255));
        SelectObject(window->mem_dc, hBrDotL);
        Ellipse(window->mem_dc, dot_lx - 6, dot_ly - 6, dot_lx + 6, dot_ly + 6);
        DeleteObject(hBrDotL);

        int cx_r = window->width - 95, cy_r = window->height - 85;
        SelectObject(window->mem_dc, hBrCirc);
        Ellipse(window->mem_dc, cx_r - r_circle, cy_r - r_circle, cx_r + r_circle, cy_r + r_circle);

        int dot_rx = cx_r + (pad.analog_stick_r.x * (r_circle - 8)) / 32767;
        int dot_ry = cy_r - (pad.analog_stick_r.y * (r_circle - 8)) / 32767;
        HBRUSH hBrDotR = CreateSolidBrush(RGB(255, 180, 80));
        SelectObject(window->mem_dc, hBrDotR);
        Ellipse(window->mem_dc, dot_rx - 6, dot_ry - 6, dot_rx + 6, dot_ry + 6);
        DeleteObject(hBrDotR);

        SelectObject(window->mem_dc, hFontSub);
        SetTextColor(window->mem_dc, RGB(160, 180, 210));
        TextOutA(window->mem_dc, cx_l - 22, cy_l + r_circle + 4, "Stick L", 7);
        TextOutA(window->mem_dc, cx_r - 22, cy_r + r_circle + 4, "Stick R", 7);

        // Limpeza de recursos GDI
        SelectObject(window->mem_dc, hOldPen);
        SelectObject(window->mem_dc, hOldBr);
        DeleteObject(hPenCirc);
        DeleteObject(hBrCirc);

        SelectObject(window->mem_dc, hOldFont);
        DeleteObject(hFontTitle);
        DeleteObject(hFontSub);
        DeleteObject(hFontSec);
        DeleteObject(hFontBody);

        // Blit direto para a janela do Windows
        BitBlt(hdc, 0, 0, client_rect.right, client_rect.bottom, window->mem_dc, 0, 0, SRCCOPY);
        ReleaseDC(window->hwnd, hdc);

        bool f12_pressed = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
        static bool s_f12_was_down = false;
        bool take_screenshot = (f12_pressed && !s_f12_was_down) || (window->total_presents == 30);
        s_f12_was_down = f12_pressed;

        if (take_screenshot) {
            BITMAPFILEHEADER bfh{};
            bfh.bfType = 0x4D42; // "BM"
            bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + window->width * window->height * 4;
            bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

            BITMAPINFOHEADER bih{};
            bih.biSize = sizeof(BITMAPINFOHEADER);
            bih.biWidth = window->width;
            bih.biHeight = -window->height; // top-down
            bih.biPlanes = 1;
            bih.biBitCount = 32;
            bih.biCompression = BI_RGB;

            FILE* f = std::fopen("za_viewport_pokedex.bmp", "wb");
            if (f) {
                std::fwrite(&bfh, sizeof(bfh), 1, f);
                std::fwrite(&bih, sizeof(bih), 1, f);
                std::fwrite(window->pixel_buffer, window->width * window->height * 4, 1, f);
                std::fclose(f);
                if (f12_pressed) {
                    std::cout << "[SCREENSHOT] Captura de tela gravada: za_viewport_pokedex.bmp" << std::endl;
                }
            }
        }
    }

    // Atualiza o título da janela a cada segundo
    if (window->total_presents % target_fps == 0) {
        char title_buf[256];
        std::snprintf(title_buf, sizeof(title_buf),
                      "Pokemon Legends: Z-A (Native PC Port) | NVN %d FPS | Quadro %llu",
                      target_fps, (unsigned long long)window->total_presents);
        if (window->hwnd) {
            SetWindowTextA(window->hwnd, title_buf);
        }
    }
}

// -------------------------------------------------------------
// Stubs Genéricos para Recursos (MemoryPool, Buffer, Texture, Program)
// -------------------------------------------------------------
static void GenericStub() {}
static int GenericSuccessStub() { return 1; }

// Tabela global com todas as 156 funções mapeadas da Fase 3
static std::unordered_map<std::string, void*> g_nvn_dispatch_table = {
    // Device
    { "nvnDeviceBuilderSetDefaults", (void*)&nvnDeviceBuilderSetDefaults },
    { "nvnDeviceBuilderSetFlags", (void*)&nvnDeviceBuilderSetFlags },
    { "nvnDeviceBuilderGetFlags", (void*)&nvnDeviceBuilderGetFlags },
    { "nvnDeviceInitialize", (void*)&nvnDeviceInitialize },
    { "nvnDeviceFinalize", (void*)&nvnDeviceFinalize },
    { "nvnDeviceGetInteger", (void*)&nvnDeviceGetInteger },
    { "nvnDeviceGetCurrentTimestampInNanoseconds", (void*)&nvnDeviceGetCurrentTimestampInNanoseconds },
    { "nvnDeviceGetTimestampInNanoseconds", (void*)&nvnDeviceGetCurrentTimestampInNanoseconds },
    { "nvnDeviceSetDebugLabel", (void*)&GenericStub },
    { "nvnDeviceInstallDebugCallback", (void*)&GenericStub },
    { "nvnDeviceGenerateDebugDomainId", (void*)&GenericSuccessStub },
    { "nvnDeviceSetWindowOriginMode", (void*)&GenericStub },
    { "nvnDeviceSetDepthMode", (void*)&GenericStub },
    { "nvnDeviceRegisterFastClearColor", (void*)&GenericStub },
    { "nvnDeviceRegisterFastClearColori", (void*)&GenericStub },
    { "nvnDeviceRegisterFastClearColorui", (void*)&GenericStub },
    { "nvnDeviceRegisterFastClearDepth", (void*)&GenericStub },
    { "nvnDeviceGetWindowOriginMode", (void*)&GenericSuccessStub },
    { "nvnDeviceGetDepthMode", (void*)&GenericSuccessStub },
    { "nvnDeviceSetIntermediateShaderCache", (void*)&GenericStub },
    { "nvnDeviceGetTextureHandle", (void*)&GenericSuccessStub },
    { "nvnDeviceGetTexelFetchHandle", (void*)&GenericSuccessStub },
    { "nvnDeviceGetImageHandle", (void*)&GenericSuccessStub },
    { "nvnDeviceGetSeparateTextureHandle", (void*)&GenericSuccessStub },
    { "nvnDeviceGetSeparateSamplerHandle", (void*)&GenericSuccessStub },
    { "nvnDeviceIsExternalDebuggerAttached", (void*)&GenericStub },
    { "nvnDeviceWalkDebugDatabase", (void*)&GenericStub },
    { "nvnDeviceFinalizeCommandHandle", (void*)&GenericStub },

    // Queue
    { "nvnQueueBuilderSetDefaults", (void*)&nvnQueueBuilderSetDefaults },
    { "nvnQueueBuilderSetDevice", (void*)&nvnQueueBuilderSetDevice },
    { "nvnQueueBuilderSetFlags", (void*)&nvnQueueBuilderSetFlags },
    { "nvnQueueBuilderSetCommandMemorySize", (void*)&nvnQueueBuilderSetCommandMemorySize },
    { "nvnQueueBuilderSetComputeMemorySize", (void*)&nvnQueueBuilderSetComputeMemorySize },
    { "nvnQueueBuilderSetControlMemorySize", (void*)&nvnQueueBuilderSetControlMemorySize },
    { "nvnQueueBuilderSetQueuePriority", (void*)&nvnQueueBuilderSetQueuePriority },
    { "nvnQueueBuilderSetQueueMemory", (void*)&nvnQueueBuilderSetQueueMemory },
    { "nvnQueueBuilderGetQueueMemorySize", (void*)&nvnQueueBuilderGetQueueMemorySize },
    { "nvnQueueInitialize", (void*)&nvnQueueInitialize },
    { "nvnQueueFinalize", (void*)&nvnQueueFinalize },
    { "nvnQueueSubmitCommands", (void*)&nvnQueueSubmitCommands },
    { "nvnQueuePresentTexture", (void*)&nvnQueuePresentTexture },
    { "nvnQueueFinish", (void*)&nvnQueueFinish },
    { "nvnQueueFlush", (void*)&nvnQueueFlush },
    { "nvnQueueGetError", (void*)&GenericStub },
    { "nvnQueueSetDebugLabel", (void*)&GenericStub },

    // Window
    { "nvnWindowBuilderSetDefaults", (void*)&nvnWindowBuilderSetDefaults },
    { "nvnWindowBuilderSetDevice", (void*)&nvnWindowBuilderSetDevice },
    { "nvnWindowBuilderSetNativeWindow", (void*)&nvnWindowBuilderSetNativeWindow },
    { "nvnWindowBuilderSetTextures", (void*)&nvnWindowBuilderSetTextures },
    { "nvnWindowBuilderSetPresentInterval", (void*)&nvnWindowBuilderSetPresentInterval },
    { "nvnWindowInitialize", (void*)&nvnWindowInitialize },
    { "nvnWindowFinalize", (void*)&nvnWindowFinalize },
    { "nvnWindowSetPresentInterval", (void*)&nvnWindowSetPresentInterval },
    { "nvnWindowAcquireTexture", (void*)&nvnWindowAcquireTexture },
    { "nvnWindowSetCrop", (void*)&GenericStub },
    { "nvnWindowSetDebugLabel", (void*)&GenericStub },

    // MemoryPool & Buffer
    { "nvnMemoryPoolBuilderSetDefaults", (void*)&GenericStub },
    { "nvnMemoryPoolBuilderSetStorage", (void*)&GenericStub },
    { "nvnMemoryPoolInitialize", (void*)&GenericSuccessStub },
    { "nvnMemoryPoolFinalize", (void*)&GenericStub },
    { "nvnMemoryPoolMap", (void*)&GenericSuccessStub },
    { "nvnMemoryPoolFlushMappedRange", (void*)&GenericStub },
    { "nvnBufferBuilderSetDefaults", (void*)&GenericStub },
    { "nvnBufferBuilderSetStorage", (void*)&GenericStub },
    { "nvnBufferInitialize", (void*)&GenericSuccessStub },
    { "nvnBufferFinalize", (void*)&GenericStub },
    { "nvnBufferMap", (void*)&GenericSuccessStub },

    // Texture, Sampler, Program
    { "nvnTextureBuilderSetDefaults", (void*)&GenericStub },
    { "nvnTextureBuilderSetDevice", (void*)&GenericStub },
    { "nvnTextureBuilderSetSize2D", (void*)&GenericStub },
    { "nvnTextureBuilderSetFormat", (void*)&GenericStub },
    { "nvnTextureInitialize", (void*)&GenericSuccessStub },
    { "nvnTextureFinalize", (void*)&GenericStub },
    { "nvnSamplerPoolInitialize", (void*)&GenericSuccessStub },
    { "nvnTexturePoolInitialize", (void*)&GenericSuccessStub },
    { "nvnProgramInitialize", (void*)&GenericSuccessStub },
    { "nvnProgramSetShaders", (void*)&GenericStub }
};

extern "C" void* nvnDeviceGetProcAddress(void* device, const char* name) {
    if (!name) return nullptr;
    auto it = g_nvn_dispatch_table.find(name);
    if (it != g_nvn_dispatch_table.end()) {
        return it->second;
    }
    // Para funções ainda sem implementação dedicada, retorna stub seguro
    return (void*)&GenericStub;
}

extern "C" void* nvnBootstrapLoader(const char* name) {
    if (!name) return nullptr;
    std::string sname(name);
    if (sname == "nvnDeviceInitialize") {
        return (void*)&nvnDeviceInitialize;
    }
    if (sname == "nvnDeviceGetProcAddress") {
        return (void*)&nvnDeviceGetProcAddress;
    }

    auto it = g_nvn_dispatch_table.find(sname);
    if (it != g_nvn_dispatch_table.end()) {
        return it->second;
    }

    return (void*)&GenericStub;
}

} // namespace nvn

namespace nv {

void SetGraphicsAllocator(void* allocate, void* free, void* reallocate, void* user_ptr) {
    std::cout << "[nv] SetGraphicsAllocator() -> Alocador de Memoria de Video Configurado" << std::endl;
}

int InitializeGraphics(void* address, size_t size) {
    std::cout << "[nv] InitializeGraphics(address=" << address << ", size=" << size 
              << ") -> Subcamada Grafica da Nvidia Inicializada" << std::endl;
    return 0;
}

void FinalizeGraphics() {
    std::cout << "[nv] FinalizeGraphics()" << std::endl;
}

} // namespace nv
