#include "nn/hid.hpp"
#include <iostream>
#include <atomic>
#include <algorithm>
#include <cmath>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace nn::hid {

static std::atomic<int64_t> s_sampling_number{1};

// Definições de estruturas do XInput sem dependência de SDK externo
#pragma pack(push, 1)
struct XINPUT_GAMEPAD_CUSTOM {
    WORD wButtons;
    BYTE bLeftTrigger;
    BYTE bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
};

struct XINPUT_STATE_CUSTOM {
    DWORD dwPacketNumber;
    XINPUT_GAMEPAD_CUSTOM Gamepad;
};
#pragma pack(pop)

using PFN_XInputGetState = DWORD(WINAPI*)(DWORD dwUserIndex, XINPUT_STATE_CUSTOM* pState);
static PFN_XInputGetState s_pfnXInputGetState = nullptr;
static HMODULE s_hXInputDll = nullptr;
static bool s_xinput_initialized = false;

// Estado do Mouse para controle de câmera
static HWND s_target_hwnd = nullptr;
static bool s_mouse_capture_enabled = true;
static bool s_mouse_captured = false;
static float s_mouse_sensitivity = 950.0f; // Sensibilidade de conversão delta do mouse -> analógico
static bool s_mouse_invert_y = false;
static float s_smoothed_cam_x = 0.0f;
static float s_smoothed_cam_y = 0.0f;
static POINT s_last_cursor_pos{0, 0};
static bool s_has_last_cursor = false;
static bool s_alt_was_down = false;

void SetTargetWindow(void* hwnd) {
    s_target_hwnd = reinterpret_cast<HWND>(hwnd);
}

void SetMouseCaptureEnabled(bool enabled) {
    s_mouse_capture_enabled = enabled;
    if (!enabled && s_mouse_captured) {
        s_mouse_captured = false;
        ClipCursor(nullptr);
        ShowCursor(TRUE);
    }
}

bool IsMouseCaptureEnabled() {
    return s_mouse_capture_enabled;
}

void SetMouseSensitivity(float sensitivity) {
    s_mouse_sensitivity = sensitivity;
}

float GetMouseSensitivity() {
    return s_mouse_sensitivity;
}

void SetMouseInvertY(bool invert) {
    s_mouse_invert_y = invert;
}

bool GetMouseInvertY() {
    return s_mouse_invert_y;
}

#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y                0x8000

constexpr int XINPUT_DEADZONE_LEFT  = 7849;
constexpr int XINPUT_DEADZONE_RIGHT = 8689;
constexpr int XINPUT_TRIGGER_THRESHOLD = 30;

static void InitXInput() {
    if (s_xinput_initialized) return;
    s_xinput_initialized = true;

    const char* dll_names[] = {
        "xinput1_4.dll",   // Windows 8, 10, 11
        "xinput9_1_0.dll", // Windows Vista, 7, 8
        "xinput1_3.dll"    // DirectX Redistributable
    };

    for (const char* name : dll_names) {
        s_hXInputDll = LoadLibraryA(name);
        if (s_hXInputDll) {
            s_pfnXInputGetState = reinterpret_cast<PFN_XInputGetState>(
                GetProcAddress(s_hXInputDll, "XInputGetState"));
            if (s_pfnXInputGetState) {
                std::cout << "[nn::hid] XInput carregado com sucesso via " << name 
                          << " (Controles Xbox / DualSense / Gamepad habilitados!)" << std::endl;
                return;
            }
            FreeLibrary(s_hXInputDll);
            s_hXInputDll = nullptr;
        }
    }

    std::cout << "[nn::hid] Aviso: XInput DLL nao encontrada. Polling restrito a Teclado/Mouse Win32." << std::endl;
}

void InitializeNpad() {
    InitXInput();
    std::cout << "[nn::hid] InitializeNpad() -> Camada de Entrada Pronta (Teclado + Mouse Cam + Gamepad XInput)" << std::endl;
}

void SetSupportedNpadIdType(const uint32_t* npad_ids, size_t count) {
    std::cout << "[nn::hid] SetSupportedNpadIdType(count=" << count << ")" << std::endl;
    for (size_t i = 0; i < count; ++i) {
        std::cout << "         -> NpadId[" << i << "] = 0x" << std::hex << npad_ids[i] << std::dec << std::endl;
    }
}

void SetSupportedNpadStyleSet(NpadStyleSet style_set) {
    std::cout << "[nn::hid] SetSupportedNpadStyleSet(flags=0x" << std::hex << style_set.flags << std::dec << ")" << std::endl;
}

void SetNpadJoyHoldType(NpadJoyHoldType hold_type) {
    std::cout << "[nn::hid] SetNpadJoyHoldType(" << (hold_type == NpadJoyHoldType::Vertical ? "Vertical" : "Horizontal") << ")" << std::endl;
}

void SetNpadHandheldActivationMode(NpadHandheldActivationMode mode) {
    std::cout << "[nn::hid] SetNpadHandheldActivationMode(" << static_cast<int>(mode) << ")" << std::endl;
}

void ShowControllerStrapGuide() {
    std::cout << "[nn::hid] ShowControllerStrapGuide() [Ignorado no PC]" << std::endl;
}

void InitializeTouchScreen(int width, int height) {
    std::cout << "[nn::hid] InitializeTouchScreen(" << width << "x" << height << ")" << std::endl;
}

NpadStyleSet GetNpadStyleSet(const uint32_t& npad_id) {
    (void)npad_id;
    return NpadStyleSet{ DefaultStyleMask };
}

static void PollWindowsInput(NpadFullKeyState* state) {
    if (!state) return;

    state->sampling_number = s_sampling_number.fetch_add(1);
    state->buttons = 0;
    state->analog_stick_l = {0, 0};
    state->analog_stick_r = {0, 0};
    state->attributes = 1; // isConnected = true

    auto is_down = [](int vk) -> bool {
        return (GetAsyncKeyState(vk) & 0x8000) != 0;
    };

    // 1. Teclado: Movimentação (WASD) -> Analógico Esquerdo
    int stick_l_x = 0;
    int stick_l_y = 0;
    if (is_down('W')) stick_l_y += 32767;
    if (is_down('S')) stick_l_y -= 32767;
    if (is_down('A')) stick_l_x -= 32767;
    if (is_down('D')) stick_l_x += 32767;

    // 2. Câmera: Teclas de Seta -> Analógico Direito
    int stick_r_x = 0;
    int stick_r_y = 0;
    if (is_down(VK_UP))    stick_r_y += 32767;
    if (is_down(VK_DOWN))  stick_r_y -= 32767;
    if (is_down(VK_LEFT))  stick_r_x -= 32767;
    if (is_down(VK_RIGHT)) stick_r_x += 32767;

    // 3. Câmera via MOUSE: Suporte a Modo Relativo com Trava Central (Infinite 360) e Suavização
    HWND active_wnd = GetForegroundWindow();
    bool is_our_window_active = (s_target_hwnd && active_wnd == s_target_hwnd);

    // Tecla ALT ou F1 alterna a trava do cursor para liberar/capturar o mouse
    bool alt_is_down = is_down(VK_MENU) || is_down(VK_F1);
    if (alt_is_down && !s_alt_was_down) {
        if (s_mouse_captured) {
            s_mouse_captured = false;
            ClipCursor(nullptr);
            ShowCursor(TRUE);
        } else if (is_our_window_active && s_mouse_capture_enabled) {
            s_mouse_captured = true;
            ShowCursor(FALSE);
        }
    }
    s_alt_was_down = alt_is_down;

    // Se clicar dentro da janela ativa, reativa captura automaticamente
    if (is_our_window_active && s_mouse_capture_enabled && !s_mouse_captured) {
        if (is_down(VK_LBUTTON) || is_down(VK_RBUTTON)) {
            s_mouse_captured = true;
            ShowCursor(FALSE);
        }
    } else if (!is_our_window_active && s_mouse_captured) {
        // Janela perdeu foco -> libera cursor
        s_mouse_captured = false;
        ClipCursor(nullptr);
        ShowCursor(TRUE);
    }

    if (s_mouse_captured && s_target_hwnd) {
        RECT client_rc{};
        GetClientRect(s_target_hwnd, &client_rc);
        POINT center = { (client_rc.left + client_rc.right) / 2, (client_rc.top + client_rc.bottom) / 2 };
        ClientToScreen(s_target_hwnd, &center);

        RECT clip_rc{};
        GetWindowRect(s_target_hwnd, &clip_rc);
        ClipCursor(&clip_rc);

        POINT cur_cursor{};
        if (GetCursorPos(&cur_cursor)) {
            int delta_x = cur_cursor.x - center.x;
            int delta_y = cur_cursor.y - center.y;

            if (delta_x != 0 || delta_y != 0) {
                float target_x = delta_x * s_mouse_sensitivity;
                float target_y = (s_mouse_invert_y ? delta_y : -delta_y) * s_mouse_sensitivity;

                // Filtro exponencial para resposta instantânea e sem trepidação
                s_smoothed_cam_x = s_smoothed_cam_x * 0.25f + target_x * 0.75f;
                s_smoothed_cam_y = s_smoothed_cam_y * 0.25f + target_y * 0.75f;

                // Recentraliza o cursor no centro da janela (360 graus infinito sem sair da tela)
                SetCursorPos(center.x, center.y);
            } else {
                // Decaimento rápido quando parado
                s_smoothed_cam_x *= 0.3f;
                s_smoothed_cam_y *= 0.3f;
                if (std::abs(s_smoothed_cam_x) < 50.0f) s_smoothed_cam_x = 0.0f;
                if (std::abs(s_smoothed_cam_y) < 50.0f) s_smoothed_cam_y = 0.0f;
            }
        }
    } else {
        // Modo livre de cursor (fora da janela ou captura desativada)
        POINT cur_cursor{};
        if (GetCursorPos(&cur_cursor)) {
            if (s_has_last_cursor && is_our_window_active) {
                int delta_x = cur_cursor.x - s_last_cursor_pos.x;
                int delta_y = cur_cursor.y - s_last_cursor_pos.y;
                float target_x = delta_x * s_mouse_sensitivity;
                float target_y = (s_mouse_invert_y ? delta_y : -delta_y) * s_mouse_sensitivity;
                s_smoothed_cam_x = s_smoothed_cam_x * 0.3f + target_x * 0.7f;
                s_smoothed_cam_y = s_smoothed_cam_y * 0.3f + target_y * 0.7f;
            } else {
                s_smoothed_cam_x = 0.0f;
                s_smoothed_cam_y = 0.0f;
            }
            s_last_cursor_pos = cur_cursor;
            s_has_last_cursor = true;
        }
    }

    stick_r_x += static_cast<int>(s_smoothed_cam_x);
    stick_r_y += static_cast<int>(s_smoothed_cam_y);

    // 4. Botões do Mouse
    if (is_down(VK_LBUTTON)) state->buttons |= NpadButton_ZR;     // Botão Esquerdo do Mouse: Lançar Pokébola / Ataque
    if (is_down(VK_RBUTTON)) state->buttons |= NpadButton_ZL;     // Botão Direito do Mouse: Mirar com precisão
    if (is_down(VK_MBUTTON)) state->buttons |= NpadButton_StickR; // Botão do Meio (Scroll): Resetar câmera / Travar mira

    // 5. Botões de Ação do Teclado
    if (is_down(VK_SPACE))                  state->buttons |= NpadButton_A;     // Interagir / Pular
    if (is_down('B') || is_down(VK_ESCAPE)) state->buttons |= NpadButton_B;     // Cancelar / Agachar
    if (is_down('E') || is_down('F'))       state->buttons |= NpadButton_X;     // Menu / Pokébola
    if (is_down('Q') || is_down('R'))       state->buttons |= NpadButton_Y;     // Esquivar
    if (is_down(VK_SHIFT))                  state->buttons |= NpadButton_ZL;    // Mirar alternativa
    if (is_down(VK_CONTROL))                state->buttons |= NpadButton_ZR;    // Lançar alternativa
    if (is_down('1'))                       state->buttons |= NpadButton_L;
    if (is_down('2'))                       state->buttons |= NpadButton_R;
    if (is_down(VK_RETURN))                 state->buttons |= NpadButton_Plus;   // Start
    if (is_down(VK_TAB))                    state->buttons |= NpadButton_Minus;  // Mapa

    // 6. Gamepad Físico via XInput
    if (s_pfnXInputGetState) {
        XINPUT_STATE_CUSTOM xstate{};
        DWORD res = s_pfnXInputGetState(0, &xstate);
        if (res == 0) { // Conectado
            const auto& pad = xstate.Gamepad;

            if (std::abs(pad.sThumbLX) > XINPUT_DEADZONE_LEFT)  stick_l_x += pad.sThumbLX;
            if (std::abs(pad.sThumbLY) > XINPUT_DEADZONE_LEFT)  stick_l_y += pad.sThumbLY;
            if (std::abs(pad.sThumbRX) > XINPUT_DEADZONE_RIGHT) stick_r_x += pad.sThumbRX;
            if (std::abs(pad.sThumbRY) > XINPUT_DEADZONE_RIGHT) stick_r_y += pad.sThumbRY;

            // Mapeamento Switch <-> Xbox
            if (pad.wButtons & XINPUT_GAMEPAD_B) state->buttons |= NpadButton_A;
            if (pad.wButtons & XINPUT_GAMEPAD_A) state->buttons |= NpadButton_B;
            if (pad.wButtons & XINPUT_GAMEPAD_Y) state->buttons |= NpadButton_X;
            if (pad.wButtons & XINPUT_GAMEPAD_X) state->buttons |= NpadButton_Y;

            if (pad.bLeftTrigger > XINPUT_TRIGGER_THRESHOLD)   state->buttons |= NpadButton_ZL;
            if (pad.bRightTrigger > XINPUT_TRIGGER_THRESHOLD)  state->buttons |= NpadButton_ZR;
            if (pad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)   state->buttons |= NpadButton_L;
            if (pad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)  state->buttons |= NpadButton_R;

            if (pad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB)  state->buttons |= NpadButton_StickL;
            if (pad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) state->buttons |= NpadButton_StickR;

            if (pad.wButtons & XINPUT_GAMEPAD_DPAD_UP)    state->buttons |= NpadButton_Up;
            if (pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)  state->buttons |= NpadButton_Down;
            if (pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)  state->buttons |= NpadButton_Left;
            if (pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) state->buttons |= NpadButton_Right;

            if (pad.wButtons & XINPUT_GAMEPAD_START) state->buttons |= NpadButton_Plus;
            if (pad.wButtons & XINPUT_GAMEPAD_BACK)  state->buttons |= NpadButton_Minus;
        }
    }

    // Clamping final
    state->analog_stick_l.x = std::clamp(stick_l_x, -32767, 32767);
    state->analog_stick_l.y = std::clamp(stick_l_y, -32767, 32767);
    state->analog_stick_r.x = std::clamp(stick_r_x, -32767, 32767);
    state->analog_stick_r.y = std::clamp(stick_r_y, -32767, 32767);
}

void GetNpadFullKeyState(NpadFullKeyState* out_state, const uint32_t& npad_id) {
    (void)npad_id;
    PollWindowsInput(out_state);
}

void GetNpadHandheldState(NpadHandheldState* out_state, const uint32_t& npad_id) {
    (void)npad_id;
    PollWindowsInput(out_state);
}

void GetNpadJoyDualState(NpadJoyDualState* out_state, const uint32_t& npad_id) {
    (void)npad_id;
    PollWindowsInput(out_state);
}

int GetNpadFullKeyStates(NpadFullKeyState* out_states, int count, const uint32_t& npad_id) {
    (void)npad_id;
    if (!out_states || count <= 0) return 0;
    PollWindowsInput(&out_states[0]);
    return 1;
}

int GetNpadHandheldStates(NpadHandheldState* out_states, int count, const uint32_t& npad_id) {
    (void)npad_id;
    if (!out_states || count <= 0) return 0;
    PollWindowsInput(&out_states[0]);
    return 1;
}

int GetNpadJoyDualStates(NpadJoyDualState* out_states, int count, const uint32_t& npad_id) {
    (void)npad_id;
    if (!out_states || count <= 0) return 0;
    PollWindowsInput(&out_states[0]);
    return 1;
}

} // namespace nn::hid
