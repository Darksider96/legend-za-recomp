#include "za/game_application.h"
#include "nn/os.h"
#include "nn/oe.h"
#include "nn/fs.h"
#include "nn/diag.h"
#include <cstdio>
#include <filesystem>

int main(int argc, char** argv) {
    std::printf("\n*****************************************************************\n");
    std::printf("  Pokémon Legends: Z-A (Switch Port & Re-Implementation)\n");
    std::printf("  Based on Static Analysis of exefs/main (AArch64)\n");
    std::printf("*****************************************************************\n\n");

    // Pass CLI arguments to Nintendo OS environment
    nn::os::SetHostArgs(argc, argv);

    // If a romfs or data directory exists locally, map it to the "data:" volume
    if (std::filesystem::exists("./romfs")) {
        nn::fs::SetMountDirectory("data", "./romfs");
    } else if (std::filesystem::exists("../exefs")) {
        nn::fs::SetMountDirectory("data", "../exefs");
    }

    // Queue simulated OE events (Game gained window focus and starts in Handheld/Normal mode)
    nn::oe::PushNotificationMessage(nn::oe::Message_FocusGained);
    nn::oe::PushNotificationMessage(nn::oe::Message_OperationModeHandheld);
    nn::oe::PushNotificationMessage(nn::oe::Message_PerformanceModeNormal);

    // 1. Invoke Switch early startup routine
    za::nninitStartup();

    // 2. Invoke Switch main entrypoint
    const int result = za::nnMain();

    return result;
}
