#pragma once

#include "argos/ir/module.hpp"

#include <filesystem>
#include <string>

namespace argos::driver {
// Emits GNU/AT&T x86-64 assembly without invoking the host toolchain.
int emit_assembly(const argos::ir::Module &module,
                  const std::filesystem::path &output,
                  std::string &error);
int compile_native(const argos::ir::Module &module,
                   const std::filesystem::path &output,
                   std::string &error);
} // namespace argos::driver
