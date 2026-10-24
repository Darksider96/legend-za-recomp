#pragma once
// Fabricas dos servicos nativos registrados no sm (um arquivo por familia de servicos)

#include "ipc.hpp"

#include <memory>

namespace recomp::ipc {

std::shared_ptr<Service> MakeAppletOE();  // am.cpp
std::shared_ptr<Service> MakeApm();       // am.cpp

} // namespace recomp::ipc
