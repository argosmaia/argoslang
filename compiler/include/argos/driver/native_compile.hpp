#pragma once

#include "argos/ir/module.hpp"

#include <filesystem>
#include <string>

namespace argos::driver {
enum class Target { x86_64_linux, avr_atmega328p };
// Emits GNU/AT&T x86-64 assembly without invoking the host toolchain.
int emit_assembly(const argos::ir::Module &module,
                  const std::filesystem::path &output,
                  std::string &error, Target target = Target::x86_64_linux);
int compile_native(const argos::ir::Module &module,
                   const std::filesystem::path &output,
                   std::string &error, Target target = Target::x86_64_linux);
} // namespace argos::driver
