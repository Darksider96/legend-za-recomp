#pragma once

#include "nn/types.h"
#include <cstdint>

namespace nn::oe {

enum MessageType : uint32_t {
    Message_None                  = 0x0,
    Message_FocusGained           = 0xF,
    Message_FocusLost             = 0x10,
    Message_OperationModeHandheld = 0x1E,
    Message_OperationModeDocked   = 0x1F,
    Message_PerformanceModeNormal = 0x20,
    Message_PerformanceModeBoost  = 0x21,
    Message_ResumeFromSleep       = 0x64
};

bool TryPopNotificationMessage(uint32_t* outMessage);
void PushNotificationMessage(uint32_t message);

} // namespace nn::oe
