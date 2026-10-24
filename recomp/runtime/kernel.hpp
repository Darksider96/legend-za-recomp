#pragma once
// HLE do kernel do Horizon (SVCs); a entrada SupervisorCall esta em recomp/context.hpp

#include <cstdint>

namespace recomp {

// Dados do processo que o kernel informa (GetInfo ProgramId/UsedMemorySize)
void SetKernelProcessInfo(uint64_t program_id, uint64_t code_size);

} // namespace recomp
