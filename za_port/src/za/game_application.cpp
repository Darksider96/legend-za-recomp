#include "za/game_application.h"
#include "za/subsystem_registry.h"
#include "nn/os.h"
#include "nn/fs.h"
#include "nn/oe.h"
#include "nn/hid.h"
#include "nn/diag.h"
#include <thread>
#include <chrono>

namespace za {

GameApplication::GameApplication() = default;
GameApplication::~GameApplication() = default;

bool GameApplication::Initialize() {
    NN_LOG("\n>>> [GameApplication::Initialize] (Vtable +0x20 / 0x71005AEB60) <<<\n");
    if (!SubsystemRegistry::Instance().InitializeAll()) {
        NN_LOG("[GameApplication::Initialize] Failed to initialize engine subsystems!\n");
        return false;
    }
    running_ = true;
    return true;
}

void GameApplication::PreLoopInit() {
    NN_LOG("[GameApplication::PreLoopInit] (Vtable +0x90 / 0x71019E3180)\n");
    last_frame_time_ = std::chrono::high_resolution_clock::now();
    frame_count_ = 0;
}

bool GameApplication::Tick() {
    // Vtable +0x18 / 0x71019E32EC: The core game frame loop
    const auto now = std::chrono::high_resolution_clock::now();
    const float delta_time = std::chrono::duration<float>(now - last_frame_time_).count();
    last_frame_time_ = now;

    // 1. Drain OE notification messages (Sleep, Focus, Docked mode)
    uint32_t oe_msg = 0;
    while (nn::oe::TryPopNotificationMessage(&oe_msg)) {
        NN_LOG("[GameApplication::Tick] Handling OE Notification: 0x%08X\n", oe_msg);
    }

    // 2. Query System Tick
    const nn::os::Tick current_tick = nn::os::GetSystemTick();
    const nn::TimeSpan time_span = nn::os::ConvertToTimeSpan(current_tick);

    // 3. Update all 34 subsystems
    SubsystemRegistry::Instance().UpdateAll(delta_time);

    frame_count_++;

    if (frame_count_ % 30 == 0 || frame_count_ == 1) {
        NN_LOG("[GameApplication::Tick] Frame #%llu | dt: %.3f ms | SysTick: %lld (%lld ms)\n",
               (unsigned long long)frame_count_,
               delta_time * 1000.0f,
               (long long)current_tick.GetInt64Value(),
               (long long)time_span.GetMilliSeconds());
    }

    // For bootstrap/smoke-test purposes in CLI, run 60 frames (~1 sec) then gracefully exit
    if (frame_count_ >= 60) {
        NN_LOG("[GameApplication::Tick] Target frame count reached (60 frames). Gracefully exiting game loop.\n");
        RequestExit();
    }

    // Cap at ~60 FPS
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    return running_;
}

void GameApplication::Finalize() {
    NN_LOG("\n>>> [GameApplication::Finalize] (Vtable +0x28 / 0x710104624C) <<<\n");
    SubsystemRegistry::Instance().FinalizeAll();
    running_ = false;
    NN_LOG("[GameApplication::Finalize] Engine shutdown complete.\n");
}

int GameApplication::Run() {
    NN_LOG("=================================================================\n");
    NN_LOG(" Starting Pokémon Legends: Z-A Execution Loop (0x71019E31B0)\n");
    NN_LOG("=================================================================\n");

    if (!Initialize()) {
        NN_LOG("[GameApplication] Fatal: Initialization failed!\n");
        return -1;
    }

    PreLoopInit();

    while (running_) {
        if (!Tick()) {
            break;
        }
    }

    Finalize();
    return 0;
}

// Memory allocator callbacks used during nninitStartup (0x71019E8C60, 0x71019E8C64, etc.)
static void* StartupAlloc(size_t size) {
    return std::malloc(size);
}

static void StartupFree(void* ptr) {
    std::free(ptr);
}

void nninitStartup() {
    NN_LOG("=================================================================\n");
    NN_LOG(" [nninitStartup] Entry: 0x71019E8720 (RVA 0x019E8720)\n");
    NN_LOG("=================================================================\n");

    // 1. Query memory info (0x71019E873C -> 0x7102FC5760 nn::os::QueryMemoryInfo)
    nn::os::MemoryInfo mem_info{};
    nn::os::QueryMemoryInfo(&mem_info);

    // 2. Set memory allocator for thread local (0x71019E8910 -> 0x7102FC58F0)
    nn::os::SetMemoryAllocatorForThreadLocal(StartupAlloc, StartupFree);

    // 3. Set filesystem allocator (0x71019E8924 -> 0x7102FC5900 nn::fs::SetAllocator)
    nn::fs::SetAllocator(StartupAlloc, StartupFree);

    // 4. Allocate TLS slot (0x7101A0A7AC -> 0x7102FC4770 nn::os::AllocateTlsSlot)
    nn::os::TlsSlot slot = 0;
    nn::Result res = nn::os::AllocateTlsSlot(&slot, nullptr);
    if (res.IsSuccess()) {
        NN_LOG("[nninitStartup] TLS Slot allocated: %u\n", slot);
    }

    NN_LOG("[nninitStartup] Early hardware/memory startup sequence completed.\n\n");
}

int nnMain() {
    NN_LOG("=================================================================\n");
    NN_LOG(" [nnMain] Entry: 0x71019E8950 (RVA 0x019E8950)\n");
    NN_LOG("=================================================================\n");

    // 1. Retrieve arguments (0x71019E896C nn::os::GetHostArgc, 0x71019E8974 GetHostArgv)
    const int argc = nn::os::GetHostArgc();
    char** argv = nn::os::GetHostArgv();
    NN_LOG("[nnMain] Host Arguments: argc=%d, argv=%p\n", argc, (void*)argv);

    // 2. Construct GameApplication / AppFramework (0x7100B10BD0)
    NN_LOG("[nnMain] Instantiating GameApplication object...\n");
    GameApplication app;

    // 3. Dispatch to main loop runner (0x71019E8BC4 -> 0x71019E31B0)
    const int exit_code = app.Run();

    NN_LOG("=================================================================\n");
    NN_LOG(" [nnMain] Process terminated cleanly with exit code: %d\n", exit_code);
    NN_LOG("=================================================================\n");
    return exit_code;
}

} // namespace za
