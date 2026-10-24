#pragma once

#include "za/types.h"
#include "za/subsystem.h"
#include <array>
#include <memory>
#include <vector>

namespace za {

struct SubsystemDescriptor {
    uint32_t slot{0};
    uintptr_t callback_address{0};
    uint32_t adjustment{0};
    const char* default_name{""};
};

class SubsystemRegistry {
public:
    static SubsystemRegistry& Instance();

    void RegisterSubsystem(uint32_t slot, std::unique_ptr<ISubsystem> subsystem);
    [[nodiscard]] ISubsystem* GetSubsystem(uint32_t slot);

    bool InitializeAll();
    void UpdateAll(float deltaTime);
    void FinalizeAll();

    [[nodiscard]] const std::array<SubsystemDescriptor, SubsystemTableCount>& GetDescriptors() const noexcept {
        return descriptors_;
    }

private:
    SubsystemRegistry();
    ~SubsystemRegistry() = default;

    std::array<SubsystemDescriptor, SubsystemTableCount> descriptors_{};
    std::array<std::unique_ptr<ISubsystem>, SubsystemTableCount> subsystems_{};
    bool initialized_{false};
};

} // namespace za
