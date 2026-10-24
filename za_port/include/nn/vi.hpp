#pragma once
#include "types.hpp"
#include <windows.h>

namespace nn::vi {

using DisplayHandle = uint64_t;
using LayerHandle = uint64_t;

// Inicialização da interface visual da Nintendo
void Initialize();
nn::Result OpenDefaultDisplay(DisplayHandle* out_display);
nn::Result CreateManagedLayer(LayerHandle* out_layer, DisplayHandle display, uint32_t flags);
nn::Result GetNativeWindow(void** out_native_window, LayerHandle layer);

// Controle da Janela do Windows (Host)
void SetWindowResolution(int width, int height);
HWND GetWindowHandle();
bool ProcessWindowEvents();
void SetWindowTitle(const char* title);

} // namespace nn::vi
