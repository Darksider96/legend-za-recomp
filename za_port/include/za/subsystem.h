#pragma once

#include <string>
#include <cstdint>

namespace za {

class ISubsystem {
public:
    virtual ~ISubsystem() = default;

    [[nodiscard]] virtual const char* GetName() const noexcept = 0;
    [[nodiscard]] virtual uint32_t GetSlotIndex() const noexcept = 0;
    [[nodiscard]] virtual uintptr_t GetOriginalCallbackRva() const noexcept = 0;

    virtual bool Initialize() = 0;
    virtual void Update(float deltaTime) = 0;
    virtual void Finalize() = 0;
};

} // namespace za
