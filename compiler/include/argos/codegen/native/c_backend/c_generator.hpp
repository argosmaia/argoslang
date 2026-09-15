#pragma once
#include "argos/codegen/native/native_codegen.hpp"
namespace argos::codegen::native::c_backend {
class CGenerator final : public NativeCodeGenerator {
public:
  std::string generate(const argos::ir::Module &) override;
};
} // namespace argos::codegen::native::c_backend
