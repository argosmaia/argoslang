#pragma once

#include "argos/ir/module.hpp"

#include <optional>
#include <string>

namespace argos::ir {

[[nodiscard]] std::optional<std::string> validate_module(const Module &module);

} // namespace argos::ir
