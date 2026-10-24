#include "nn/hid.h"
#include "nn/diag.h"
#include <vector>
#include <mutex>

namespace nn::hid {

namespace {
    std::vector<NpadIdType> g_supported_ids;
    NpadStyleSet g_supported_styles = DefaultStyleMask;
    bool g_npad_initialized = false;
    std::mutex g_hid_mutex;
}

void InitializeNpad() {
    std::lock_guard<std::mutex> lock(g_hid_mutex);
    g_npad_initialized = true;
    NN_LOG("[nn::hid] InitializeNpad()\n");
}

void SetSupportedNpadIdType(const NpadIdType* idArray, size_t count) {
    if (!idArray || count == 0) return;
    std::lock_guard<std::mutex> lock(g_hid_mutex);
    g_supported_ids.assign(idArray, idArray + count);
    NN_LOG("[nn::hid] SetSupportedNpadIdType(count=%zu):", count);
    for (size_t i = 0; i < count; ++i) {
        NN_LOG(" [0x%x]", idArray[i]);
    }
    NN_LOG("\n");
}

void SetSupportedNpadStyleSet(NpadStyleSet style) {
    std::lock_guard<std::mutex> lock(g_hid_mutex);
    g_supported_styles = style;
    NN_LOG("[nn::hid] SetSupportedNpadStyleSet(0x%x) [FullKey=%d, Handheld=%d, JoyDual=%d]\n",
           style,
           (style & NpadStyle::FullKey) != 0,
           (style & NpadStyle::Handheld) != 0,
           (style & NpadStyle::JoyDual) != 0);
}

NpadStyleSet GetNpadStyleSet(const NpadIdType& id) {
    return g_supported_styles;
}

void ShowControllerStrapGuide() {
    NN_LOG("[nn::hid] ShowControllerStrapGuide() stub invoked\n");
}

void InitializeTouchScreen(int width, int height) {
    NN_LOG("[nn::hid] InitializeTouchScreen(width=%d, height=%d)\n", width, height);
}

void GetNpadState(NpadState* outState, const NpadIdType& id) {
    if (!outState) return;
    // Basic neutral controller state
    outState->sampling_number++;
    outState->buttons = 0;
    outState->analog_stick_l_x = 0;
    outState->analog_stick_l_y = 0;
    outState->analog_stick_r_x = 0;
    outState->analog_stick_r_y = 0;
    outState->attributes = 1; // Connected
}

} // namespace nn::hid
