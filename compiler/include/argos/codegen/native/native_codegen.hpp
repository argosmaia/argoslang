#pragma once
#include "argos/ir/module.hpp"
#include <string>
namespace argos::codegen::native {
class NativeCodeGenerator {
public:
  virtual ~NativeCodeGenerator() = default;
  virtual std::string generate(const argos::ir::Module &) = 0;
};
} // namespace argos::codegen::native
