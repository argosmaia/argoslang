#pragma once
#include "argos/ir/module.hpp"
#include <string>
namespace argos::codegen::vm {
class BytecodeEmitter final {
public:
  std::string emit(const argos::ir::Module &) const;
};
} // namespace argos::codegen::vm
