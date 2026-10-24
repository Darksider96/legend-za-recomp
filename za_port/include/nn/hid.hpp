#pragma once
#include "types.hpp"
#include <cstdint>
#include <cstddef>

namespace nn::hid {

struct NpadStyleSet {
    uint32_t flags{0x7}; // Default: FullKey | Handheld | JoyDual
};

inline constexpr uint32_t NpadStyle_FullKey  = 1 << 0; // Pro Controller
inline constexpr uint32_t NpadStyle_Handheld = 1 << 1; // Attached Joy-Cons
inline constexpr uint32_t NpadStyle_JoyDual  = 1 << 2; // Detached Joy-Con pair
inline constexpr uint32_t NpadStyle_JoyLeft  = 1 << 3; // Single Left Joy-Con
inline constexpr uint32_t NpadStyle_JoyRight = 1 << 4; // Single Right Joy-Con
inline constexpr uint32_t DefaultStyleMask   = 0x7;

using NpadIdType = uint32_t;
namespace NpadId {
    inline constexpr NpadIdType No1      = 0x0;
    inline constexpr NpadIdType No2      = 0x1;
    inline constexpr NpadIdType No3      = 0x2;
    inline constexpr NpadIdType No4      = 0x3;
    inline constexpr NpadIdType Handheld = 0x20;
}

enum class NpadJoyHoldType : int32_t {
    Vertical = 0,
    Horizontal = 1
};

enum class NpadHandheldActivationMode : int32_t {
    Dual = 0,
    Single = 1,
    None = 2
};

struct AnalogStickState {
    int32_t x{0};
    int32_t y{0};
};

// Layout exato de 40 bytes (0x28) descoberto pelo Codex (Fase 4 - phase4_hid_types.h)
struct alignas(8) NpadFullKeyState {
    int64_t sampling_number{0};       // 0x00; SDK incrementa continuamente a cada amostra
    uint64_t buttons{0};              // 0x08 (A, B, X, Y, L, R, ZL, ZR, DPad...)
    AnalogStickState analog_stick_l{};// 0x10 (x, y) normalizado para [-32767, +32767]
    AnalogStickState analog_stick_r{};// 0x18 (x, y) normalizado para [-32767, +32767]
    uint64_t attributes{1};           // 0x20 (Bit 0 = isConnected)
};

static_assert(sizeof(NpadFullKeyState) == 0x28, "NpadFullKeyState deve ter exatamente 40 bytes (0x28)");

using NpadHandheldState = NpadFullKeyState;
using NpadJoyDualState = NpadFullKeyState;

// Constantes de Botões Nintendo Switch
inline constexpr uint64_t NpadButton_A     = 1ULL << 0;
inline constexpr uint64_t NpadButton_B     = 1ULL << 1;
inline constexpr uint64_t NpadButton_X     = 1ULL << 2;
inline constexpr uint64_t NpadButton_Y     = 1ULL << 3;
inline constexpr uint64_t NpadButton_StickL= 1ULL << 4;
inline constexpr uint64_t NpadButton_StickR= 1ULL << 5;
inline constexpr uint64_t NpadButton_L     = 1ULL << 6;
inline constexpr uint64_t NpadButton_R     = 1ULL << 7;
inline constexpr uint64_t NpadButton_ZL    = 1ULL << 8;
inline constexpr uint64_t NpadButton_ZR    = 1ULL << 9;
inline constexpr uint64_t NpadButton_Plus  = 1ULL << 10;
inline constexpr uint64_t NpadButton_Minus = 1ULL << 11;
inline constexpr uint64_t NpadButton_Left  = 1ULL << 12;
inline constexpr uint64_t NpadButton_Up    = 1ULL << 13;
inline constexpr uint64_t NpadButton_Right = 1ULL << 14;
inline constexpr uint64_t NpadButton_Down  = 1ULL << 15;

// Inicialização e Configuração
void InitializeNpad();
void SetSupportedNpadIdType(const uint32_t* npad_ids, size_t count);
void SetSupportedNpadStyleSet(NpadStyleSet style_set);
void SetNpadJoyHoldType(NpadJoyHoldType hold_type);
void SetNpadHandheldActivationMode(NpadHandheldActivationMode mode);
void ShowControllerStrapGuide();
void InitializeTouchScreen(int width, int height);

// Consultas de Estilo e Polling de Estado (0x710118F2B4 / 0x71011900C8)
NpadStyleSet GetNpadStyleSet(const uint32_t& npad_id);

void GetNpadFullKeyState(NpadFullKeyState* out_state, const uint32_t& npad_id);
void GetNpadHandheldState(NpadHandheldState* out_state, const uint32_t& npad_id);
void GetNpadJoyDualState(NpadJoyDualState* out_state, const uint32_t& npad_id);

int GetNpadFullKeyStates(NpadFullKeyState* out_states, int count, const uint32_t& npad_id);
int GetNpadHandheldStates(NpadHandheldState* out_states, int count, const uint32_t& npad_id);
int GetNpadJoyDualStates(NpadJoyDualState* out_states, int count, const uint32_t& npad_id);

// Wrapper genérico
inline void GetNpadState(NpadFullKeyState* out_state, const uint32_t& npad_id) {
    GetNpadFullKeyState(out_state, npad_id);
}

// Funções Estendidas de Mouse e Câmera para PC
void SetTargetWindow(void* hwnd);
void SetMouseCaptureEnabled(bool enabled);
bool IsMouseCaptureEnabled();
void SetMouseSensitivity(float sensitivity);
float GetMouseSensitivity();
void SetMouseInvertY(bool invert);
bool GetMouseInvertY();

} // namespace nn::hid
