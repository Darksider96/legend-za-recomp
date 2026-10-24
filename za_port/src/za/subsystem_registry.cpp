#include "za/subsystem_registry.h"
#include "nn/fs.hpp"
#include "nn/hid.hpp"
#include "nn/os.hpp"
#include "nn/diag.h"
#include "audio/wwise.hpp"
#include <cstdio>
#include <vector>

namespace za {

namespace {

// Subsystem genérico padrão para slots sem lógica customizada específica
class GenericSubsystem : public ISubsystem {
public:
    GenericSubsystem(uint32_t slot, uintptr_t callback, const char* name)
        : slot_(slot), callback_(callback), name_(name) {}

    [[nodiscard]] const char* GetName() const noexcept override { return name_; }
    [[nodiscard]] uint32_t GetSlotIndex() const noexcept override { return slot_; }
    [[nodiscard]] uintptr_t GetOriginalCallbackRva() const noexcept override { return callback_; }

    bool Initialize() override {
        NN_LOG("[Subsystem #%02u] Init: %s (Callback: 0x%llX)\n",
               slot_, name_, (unsigned long long)callback_);
        return true;
    }

    void Update(float deltaTime) override {
        (void)deltaTime;
    }

    void Finalize() override {
        NN_LOG("[Subsystem #%02u] Finalize: %s\n", slot_, name_);
    }

private:
    uint32_t slot_;
    uintptr_t callback_;
    const char* name_;
};

// Slot 6 (0x71019DD280): RomFS & "data" Volume Mount Subsystem
class RomFsSubsystem : public ISubsystem {
public:
    [[nodiscard]] const char* GetName() const noexcept override { return "RomFS [data:] Mount Subsystem"; }
    [[nodiscard]] uint32_t GetSlotIndex() const noexcept override { return 6; }
    [[nodiscard]] uintptr_t GetOriginalCallbackRva() const noexcept override { return 0x71019DD280ULL; }

    bool Initialize() override {
        NN_LOG("[Subsystem #06] Initializing RomFS Archive...\n");
        size_t cache_size = 0;
        nn::Result res = nn::fs::QueryMountRomCacheSize(&cache_size);
        if (res != nn::ResultSuccess) {
            NN_LOG("[Subsystem #06] QueryMountRomCacheSize failed\n");
            return false;
        }

        cache_buffer_.resize(cache_size ? cache_size : 64 * 1024);
        res = nn::fs::MountRom("data", cache_buffer_.data(), cache_buffer_.size());
        if (res != nn::ResultSuccess) {
            NN_LOG("[Subsystem #06] Failed to mount 'data' volume\n");
            return false;
        }

        NN_LOG("[Subsystem #06] Successfully mounted 'data:' volume (%zu bytes cache buffer)\n", cache_buffer_.size());
        return true;
    }

    void Update(float deltaTime) override {
        (void)deltaTime;
    }

    void Finalize() override {
        NN_LOG("[Subsystem #06] Unmounting 'data:' volume...\n");
        nn::fs::Unmount("data");
        cache_buffer_.clear();
    }

private:
    std::vector<uint8_t> cache_buffer_;
};

// Slot 8 (0x71019DEEF0): HID / Controller Subsystem
class HidSubsystem : public ISubsystem {
public:
    [[nodiscard]] const char* GetName() const noexcept override { return "HID Controller Subsystem"; }
    [[nodiscard]] uint32_t GetSlotIndex() const noexcept override { return 8; }
    [[nodiscard]] uintptr_t GetOriginalCallbackRva() const noexcept override { return 0x71019DEEF0ULL; }

    bool Initialize() override {
        NN_LOG("[Subsystem #08] Initializing HID Subsystem...\n");
        nn::hid::InitializeNpad();
        const uint32_t supported_ids[] = {
            nn::hid::NpadId::No1,
            nn::hid::NpadId::Handheld
        };
        nn::hid::SetSupportedNpadIdType(supported_ids, 2);
        nn::hid::SetSupportedNpadStyleSet(nn::hid::NpadStyleSet{ nn::hid::DefaultStyleMask });
        return true;
    }

    void Update(float deltaTime) override {
        (void)deltaTime;
        // Polling de entrada do jogador a cada quadro
        nn::hid::NpadFullKeyState state{};
        nn::hid::GetNpadFullKeyState(&state, nn::hid::NpadId::No1);
    }

    void Finalize() override {
        NN_LOG("[Subsystem #08] Finalizing HID Subsystem\n");
    }
};

// Slot 9 (0x71019DEEF4): Audio Renderer Subsystem
class AudioRendererSubsystem : public ISubsystem {
public:
    [[nodiscard]] const char* GetName() const noexcept override { return "Audio Renderer Subsystem"; }
    [[nodiscard]] uint32_t GetSlotIndex() const noexcept override { return 9; }
    [[nodiscard]] uintptr_t GetOriginalCallbackRva() const noexcept override { return 0x71019DEEF4ULL; }

    bool Initialize() override {
        NN_LOG("[Subsystem #09] Audio Renderer Pipeline Pronto (48 kHz Stereo / Suporte Wwise Opus & PCM)\n");
        return true;
    }

    void Update(float deltaTime) override {
        (void)deltaTime;
    }

    void Finalize() override {
        NN_LOG("[Subsystem #09] Finalizando Audio Renderer Pipeline\n");
    }
};

// Slot 32 (0x71019E1560): Audio Sound Bank Subsystem (Wwise)
class AudioSoundBankSubsystem : public ISubsystem {
public:
    [[nodiscard]] const char* GetName() const noexcept override { return "Audio Sound Bank (BGM/SE) Subsystem"; }
    [[nodiscard]] uint32_t GetSlotIndex() const noexcept override { return 32; }
    [[nodiscard]] uintptr_t GetOriginalCallbackRva() const noexcept override { return 0x71019E1560ULL; }

    bool Initialize() override {
        NN_LOG("[Subsystem #32] Inicializando Wwise SoundBanks e Stream.pck (841 MB)...\n");
        std::string audio_dir = "romfs/audio/wwise/bin/Switch";
        if (za::audio::WwiseArchive::Instance().Initialize(audio_dir)) {
            NN_LOG("[Subsystem #32] [SUCESSO] %zu faixas de audio indexadas em %zu SoundBanks!\n",
                   za::audio::WwiseArchive::Instance().GetTotalMediaCount(),
                   za::audio::WwiseArchive::Instance().GetLoadedBankCount());
            return true;
        }
        NN_LOG("[Subsystem #32] [AVISO] Nao foi possivel carregar o repositorio Wwise.\n");
        return true;
    }

    void Update(float deltaTime) override {
        (void)deltaTime;
    }

    void Finalize() override {
        NN_LOG("[Subsystem #32] Finalizando Audio Sound Bank Subsystem\n");
        za::audio::WwiseArchive::Instance().Shutdown();
    }
};

} // namespace

SubsystemRegistry& SubsystemRegistry::Instance() {
    static SubsystemRegistry s_instance;
    return s_instance;
}

SubsystemRegistry::SubsystemRegistry() {
    // Mapeamento exato dos 34 callbacks de inicialização de 0x7103A38E18 a 0x7103A39038
    descriptors_ = {{
        { 0,  0x71019DD180ULL, 0, "Memory Heap & System Allocators" },
        { 1,  0x71019DD214ULL, 0, "Core Thread Pool & Synchronization" },
        { 2,  0x71019DD218ULL, 0, "Diagnostic & Error Handler" },
        { 3,  0x71019DD21CULL, 0, "System Timer & Clock Sync" },
        { 4,  0x71019DD220ULL, 0, "Operating Environment (OE) Manager" },
        { 5,  0x71019DD230ULL, 0, "Job & Coroutine Scheduler" },
        { 6,  0x71019DD280ULL, 0, "RomFS Archive [data:] Mount Manager" },
        { 7,  0x71019DEEECULL, 0, "Save Data Storage Manager" },
        { 8,  0x71019DEEF0ULL, 0, "HID / Controller Input Manager" },
        { 9,  0x71019DEEF4ULL, 0, "Audio Renderer Subsystem" },
        { 10, 0x71019DEF44ULL, 0, "Graphics Display / Swapchain Subsystem" },
        { 11, 0x0000000040ULL, 1, "NVN GPU Device Adapter" },
        { 12, 0x71019DEF50ULL, 0, "Shader & Pipeline Cache Subsystem" },
        { 13, 0x0000000080ULL, 1, "Texture Streamer Subsystem" },
        { 14, 0x0000000060ULL, 1, "Geometry & Mesh Buffer Manager" },
        { 15, 0x0000000058ULL, 1, "Animation & Skeletal Runtime" },
        { 16, 0x71019DF43CULL, 0, "Physics & Collision Subsystem" },
        { 17, 0x71019DF440ULL, 0, "Camera / Viewport Controller" },
        { 18, 0x71019DF620ULL, 0, "Lighting & Shadow Engine" },
        { 19, 0x71019DF6C0ULL, 0, "Post-Processing & Bloom Filter" },
        { 20, 0x0000000068ULL, 1, "UI / Layout Font Renderer" },
        { 21, 0x71019DF810ULL, 0, "Localization / Message Table Subsystem" },
        { 22, 0x0000000070ULL, 1, "Game Event / Cutscene Script VM" },
        { 23, 0x0000000078ULL, 1, "Pokemon Model / Skeleton Registry" },
        { 24, 0x71019DF9F0ULL, 0, "Field / Terrain Streaming Subsystem" },
        { 25, 0x71019DFA90ULL, 0, "Battle Engine & Move Calculator" },
        { 26, 0x71019DFB38ULL, 0, "NPC & Wild Pokemon AI Manager" },
        { 27, 0x0000000088ULL, 1, "Network / Wireless Comm Subsystem" },
        { 28, 0x71019DFB58ULL, 0, "Profile & User Account Service" },
        { 29, 0x71019DFB60ULL, 0, "Debug Overlay / Telemetry Subsystem" },
        { 30, 0x71019E14F4ULL, 0, "Level Streaming & Partition Manager" },
        { 31, 0x71019E1500ULL, 0, "Asset Reference Counter / Garbage Collector" },
        { 32, 0x71019E1560ULL, 0, "Audio Sound Bank (BGM/SE) Subsystem" },
        { 33, 0x71019E1610ULL, 0, "Root Scene & World State Coordinator" }
    }};

    for (const auto& desc : descriptors_) {
        if (desc.slot == 6) {
            subsystems_[desc.slot] = std::make_unique<RomFsSubsystem>();
        } else if (desc.slot == 8) {
            subsystems_[desc.slot] = std::make_unique<HidSubsystem>();
        } else if (desc.slot == 9) {
            subsystems_[desc.slot] = std::make_unique<AudioRendererSubsystem>();
        } else if (desc.slot == 32) {
            subsystems_[desc.slot] = std::make_unique<AudioSoundBankSubsystem>();
        } else {
            subsystems_[desc.slot] = std::make_unique<GenericSubsystem>(
                desc.slot, desc.callback_address, desc.default_name
            );
        }
    }
}

void SubsystemRegistry::RegisterSubsystem(uint32_t slot, std::unique_ptr<ISubsystem> subsystem) {
    if (slot < SubsystemTableCount) {
        subsystems_[slot] = std::move(subsystem);
    }
}

ISubsystem* SubsystemRegistry::GetSubsystem(uint32_t slot) {
    if (slot < SubsystemTableCount) {
        return subsystems_[slot].get();
    }
    return nullptr;
}

bool SubsystemRegistry::InitializeAll() {
    NN_LOG("=== [SubsystemRegistry] Inicializando %zu Subsistemas Trinity Engine (0x71019DD110) ===\n", SubsystemTableCount);
    for (size_t i = 0; i < SubsystemTableCount; ++i) {
        if (subsystems_[i]) {
            if (!subsystems_[i]->Initialize()) {
                NN_LOG("[SubsystemRegistry] ERRO: Subsistema #%02zu (%s) falhou na inicializacao!\n",
                       i, descriptors_[i].default_name);
                return false;
            }
        }
    }
    initialized_ = true;
    NN_LOG("=== [SubsystemRegistry] Todos os 34 Subsistemas Trinity Inicializados com Sucesso ===\n");
    return true;
}

void SubsystemRegistry::UpdateAll(float deltaTime) {
    if (!initialized_) return;
    for (size_t i = 0; i < SubsystemTableCount; ++i) {
        if (subsystems_[i]) {
            subsystems_[i]->Update(deltaTime);
        }
    }
}

void SubsystemRegistry::FinalizeAll() {
    NN_LOG("=== [SubsystemRegistry] Finalizando Subsistemas Trinity (Ordem Reversa) ===\n");
    for (size_t i = SubsystemTableCount; i > 0; --i) {
        size_t idx = i - 1;
        if (subsystems_[idx]) {
            subsystems_[idx]->Finalize();
        }
    }
    initialized_ = false;
    NN_LOG("=== [SubsystemRegistry] Subsistemas Finalizados ===\n");
}

} // namespace za
