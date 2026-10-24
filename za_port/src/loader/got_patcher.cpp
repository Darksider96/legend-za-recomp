#include "loader/got_patcher.hpp"
#include "nn/fs.hpp"
#include "nn/os.hpp"
#include "nn/hid.hpp"
#include "nn/time.hpp"
#include "nn/vi.hpp"
#include "nvn/nvn.hpp"
#include <iostream>
#include <unordered_map>
#include <vector>
#include <iomanip>
#include <chrono>

namespace za::loader {

// Stubs genéricos para chamadas do binário convidado
static void GenericGuestStub() {
    // Retorno seguro padrão
}

static int GenericSuccessStub() {
    return 0; // nn::ResultSuccess (0)
}

static uint64_t GenericTickStub() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count());
}

static std::unordered_map<uintptr_t, void*> g_custom_hooks;

void GotPatcher::RegisterCustomHook(uintptr_t got_address, void* host_func) {
    g_custom_hooks[got_address] = host_func;
}

void GotPatcher::LogGuestCall(const char* symbol) {
    std::cout << "[GOT-DISPATCH] Chamada convidada interceptada: " << (symbol ? symbol : "unknown") << std::endl;
}

// Mapeamento das principais entradas da GOT (conferidas e extraídas 1:1 do binário exefs/main)
struct KnownGotBinding {
    uintptr_t got_address;
    void* host_func;
    const char* symbol_name;
};

static const std::vector<KnownGotBinding> g_known_bindings = {
    // --- nn::fs (Sistema de Arquivos VFS / RomFS) ---
    { 0x7103ce9640ULL, (void*)&nn::fs::OpenFile, "nn::fs::OpenFile" },
    { 0x7103ce9648ULL, (void*)&nn::fs::GetFileSize, "nn::fs::GetFileSize" },
    { 0x7103ce9650ULL, (void*)&nn::fs::CloseFile, "nn::fs::CloseFile" },
    { 0x7103ce9658ULL, (void*)&nn::fs::ReadFileWithSize, "nn::fs::ReadFile(size_t* outReadSize, ...)" },
    { 0x7103ce9ed0ULL, (void*)&nn::fs::ReadFile, "nn::fs::ReadFile(handle, offset, buf, size)" },
    { 0x7103ce9e80ULL, (void*)&nn::fs::QueryMountRomCacheSize, "nn::fs::QueryMountRomCacheSize" },
    { 0x7103ce9e88ULL, (void*)&nn::fs::MountRom, "nn::fs::MountRom(name, cacheBuf, cacheSize)" },
    { 0x7103cea700ULL, (void*)&nn::fs::MountRomSimple, "nn::fs::MountRom(name)" },
    { 0x7103ce92f0ULL, (void*)&nn::fs::Unmount, "nn::fs::Unmount" },
    { 0x7103ce92e8ULL, (void*)&nn::fs::GetEntryType, "nn::fs::GetEntryType" },
    
    // --- nn::os (Sincronização com Side-Table & Primitivas) ---
    { 0x7103ce9098ULL, (void*)&nn::os::InitializeLightEvent, "nn::os::InitializeLightEvent" },
    { 0x7103ce9038ULL, (void*)&nn::os::WaitLightEvent, "nn::os::WaitLightEvent" },
    { 0x7103ce9040ULL, (void*)&nn::os::SignalLightEvent, "nn::os::SignalLightEvent" },
    { 0x7103ce9060ULL, (void*)&nn::os::InitializeReaderWriterLock, "nn::os::InitializeReaderWriterLock" },
    { 0x7103ce9110ULL, (void*)&nn::os::InitializeMutex, "nn::os::InitializeMutex" },
    { 0x7103ce9118ULL, (void*)&nn::os::LockMutex, "nn::os::LockMutex" },
    { 0x7103ce9128ULL, (void*)&nn::os::UnlockMutex, "nn::os::UnlockMutex" },
    { 0x7103ce9178ULL, (void*)&GenericTickStub, "nn::os::GetSystemTick" },

    // --- nn::hid (Entrada, Gamepad e Teclado PC) ---
    { 0x7103ce9808ULL, (void*)&nn::hid::InitializeNpad, "nn::hid::InitializeNpad" },
    { 0x7103ce9820ULL, (void*)&nn::hid::SetSupportedNpadStyleSet, "nn::hid::SetSupportedNpadStyleSet" },
    { 0x7103ce9828ULL, (void*)&nn::hid::SetSupportedNpadIdType, "nn::hid::SetSupportedNpadIdType" },
    { 0x7103ce9830ULL, (void*)&nn::hid::SetNpadJoyHoldType, "nn::hid::SetNpadJoyHoldType" },
    { 0x7103ce9838ULL, (void*)&nn::hid::SetNpadHandheldActivationMode, "nn::hid::SetNpadHandheldActivationMode" },
    { 0x7103ce9278ULL, (void*)&nn::hid::ShowControllerStrapGuide, "nn::hid::ShowControllerStrapGuide" },
    { 0x7103ce93a8ULL, (void*)&nn::hid::InitializeTouchScreen, "nn::hid::InitializeTouchScreen" },
    { 0x7103ce9340ULL, (void*)&nn::hid::GetNpadStyleSet, "nn::hid::GetNpadStyleSet" },
    { 0x7103ce9860ULL, (void*)&nn::hid::GetNpadHandheldState, "nn::hid::GetNpadState (Handheld)" },
    { 0x7103ce9868ULL, (void*)&nn::hid::GetNpadFullKeyState, "nn::hid::GetNpadState (FullKey)" },
    { 0x7103ce9870ULL, (void*)&nn::hid::GetNpadJoyDualState, "nn::hid::GetNpadState (JoyDual)" },
    { 0x7103ce9920ULL, (void*)&nn::hid::GetNpadFullKeyStates, "nn::hid::GetNpadStates (FullKey)" },
    { 0x7103ce9928ULL, (void*)&nn::hid::GetNpadHandheldStates, "nn::hid::GetNpadStates (Handheld)" },
    { 0x7103ce9998ULL, (void*)&nn::hid::GetNpadJoyDualStates, "nn::hid::GetNpadStates (JoyDual)" },

    // --- Graphics (NV & VI) ---
    { 0x7103ce9a78ULL, (void*)&nv::InitializeGraphics, "nv::InitializeGraphics" },
    { 0x7103ce9a80ULL, (void*)&nv::SetGraphicsAllocator, "nv::SetGraphicsAllocator" },
    { 0x7103ce9f70ULL, (void*)&nn::vi::Initialize, "nn::vi::Initialize" },
    { 0x7103ce9f78ULL, (void*)&nn::vi::OpenDefaultDisplay, "nn::vi::OpenDefaultDisplay" },
    { 0x7103ce9f80ULL, (void*)&nn::vi::CreateManagedLayer, "nn::vi::CreateManagedLayer" },
    { 0x7103ce9f88ULL, (void*)&nn::vi::GetNativeWindow, "nn::vi::GetNativeWindow" },

    // --- NVN Bootstrap ---
    { 0x7103ce9000ULL, (void*)&nvn::nvnBootstrapLoader, "nvnBootstrapLoader" }
};

bool GotPatcher::PatchAll(uintptr_t base_address) {
    std::cout << "[GotPatcher] Iniciando redirecionamento de chamadas PLT/GOT..." << std::endl;
    std::cout << "             Base da imagem: 0x" << std::hex << base_address << std::dec << std::endl;

    size_t patched_count = 0;

    // 1. Aplica bindings conhecidos do SDK do Switch para as funções host do PC
    for (const auto& binding : g_known_bindings) {
        uintptr_t* got_ptr = reinterpret_cast<uintptr_t*>(binding.got_address);
        *got_ptr = reinterpret_cast<uintptr_t>(binding.host_func);
        patched_count++;
    }

    // 2. Aplica hooks customizados registrados dinamicamente
    for (const auto& [got_addr, func_ptr] : g_custom_hooks) {
        uintptr_t* got_ptr = reinterpret_cast<uintptr_t*>(got_addr);
        *got_ptr = reinterpret_cast<uintptr_t>(func_ptr);
        patched_count++;
    }

    std::cout << "[GotPatcher] [SUCESSO] " << patched_count 
              << " entradas da .got.plt redirecionadas para funcoes nativas do PC!" << std::endl;

    return true;
}

} // namespace za::loader
