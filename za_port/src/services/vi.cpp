#include "nn/vi.hpp"
#include <iostream>

namespace nn::vi {

static HWND g_hwnd = nullptr;
static bool g_window_running = false;
static uint64_t g_next_display = 1;
static uint64_t g_next_layer = 1;
static int g_custom_width = 1280;
static int g_custom_height = 720;

void SetWindowResolution(int width, int height) {
    if (width >= 640 && height >= 360) {
        g_custom_width = width;
        g_custom_height = height;
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CLOSE:
            std::cout << "[nn::vi] Evento de Fechamento da Janela recebido." << std::endl;
            g_window_running = false;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1; // Evita flickering durante apresentação contínua
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN:
            // Tecla ESC para fechar
            if (wParam == VK_ESCAPE) {
                PostMessage(hwnd, WM_CLOSE, 0, 0);
            }
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void Initialize() {
    std::cout << "[nn::vi] Initialize() -> Inicializando Interface Visual Win32" << std::endl;

    if (g_hwnd) return;

    HINSTANCE hInstance = GetModuleHandleA(nullptr);
    const char CLASS_NAME[] = "ZaPortNativeWindowClass";

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassExA(&wc);

    // Resolução configurada (padrão 1280x720) centralizada
    int width = g_custom_width;
    int height = g_custom_height;
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int pos_x = (screen_w - width) / 2;
    int pos_y = (screen_h - height) / 2;

    g_hwnd = CreateWindowExA(
        0,
        CLASS_NAME,
        "Pokemon Legends: Z-A (PC Native Port)",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        pos_x, pos_y, width, height,
        nullptr, nullptr, hInstance, nullptr
    );

    if (g_hwnd) {
        g_window_running = true;
        ShowWindow(g_hwnd, SW_SHOW);
        UpdateWindow(g_hwnd);
        std::cout << "[nn::vi] Janela Nativa criada com sucesso (1280x720) [HWND=" 
                  << g_hwnd << "]" << std::endl;
    } else {
        std::cerr << "[nn::vi] Falha ao criar a janela do Windows" << std::endl;
    }
}

nn::Result OpenDefaultDisplay(DisplayHandle* out_display) {
    if (out_display) {
        *out_display = g_next_display++;
    }
    std::cout << "[nn::vi] OpenDefaultDisplay() -> Display 1080p Padrao Aberto" << std::endl;
    return nn::ResultSuccess;
}

nn::Result CreateManagedLayer(LayerHandle* out_layer, DisplayHandle display, uint32_t flags) {
    if (out_layer) {
        *out_layer = g_next_layer++;
    }
    std::cout << "[nn::vi] CreateManagedLayer(display=" << display << ") -> Camada de Composicao Criada" << std::endl;
    return nn::ResultSuccess;
}

nn::Result GetNativeWindow(void** out_native_window, LayerHandle layer) {
    if (out_native_window) {
        *out_native_window = reinterpret_cast<void*>(g_hwnd);
    }
    std::cout << "[nn::vi] GetNativeWindow(layer=" << layer << ") -> Retornando HWND da Janela" << std::endl;
    return nn::ResultSuccess;
}

HWND GetWindowHandle() {
    return g_hwnd;
}

bool ProcessWindowEvents() {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            g_window_running = false;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return g_window_running;
}

void SetWindowTitle(const char* title) {
    if (g_hwnd && title) {
        SetWindowTextA(g_hwnd, title);
    }
}

} // namespace nn::vi
