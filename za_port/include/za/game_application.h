#pragma once

#include "nn/types.h"
#include "za/types.h"
#include <chrono>

namespace za {

class GameApplication {
public:
    GameApplication();
    virtual ~GameApplication();

    // Virtual table methods matching exefs/main layout
    virtual bool Initialize();         // Slot +0x20 (0x71005AEB60)
    virtual bool Tick();               // Slot +0x18 (0x71019E32EC)
    virtual void Finalize();           // Slot +0x28 (0x710104624C)
    virtual void PreLoopInit();        // Slot +0x90 (0x71019E3180)

    // Main execution loop matching 0x71019E31B0
    int Run();

    void RequestExit() noexcept { running_ = false; }
    [[nodiscard]] bool IsRunning() const noexcept { return running_; }
    [[nodiscard]] uint64_t GetFrameCount() const noexcept { return frame_count_; }

private:
    bool running_{false};
    uint64_t frame_count_{0};
    std::chrono::high_resolution_clock::time_point last_frame_time_{};
};

// Bootstrap functions
void nninitStartup();
int nnMain();

} // namespace za
