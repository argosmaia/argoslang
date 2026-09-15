#pragma once

#include "argos/ir/module.hpp"

#include <filesystem>
#include <string>

namespace argos::driver {
int compile_native(const argos::ir::Module &module,
                   const std::filesystem::path &output,
                   std::string &error);
} // namespace argos::driver
