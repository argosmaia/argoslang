#pragma once
#include "argos/codegen/native/native_codegen.hpp"
namespace argos::codegen::native::avr {
class CodeGenerator final : public NativeCodeGenerator {
public:
  std::string generate(const argos::ir::Module &) override;
};
} // namespace argos::codegen::native::avr
