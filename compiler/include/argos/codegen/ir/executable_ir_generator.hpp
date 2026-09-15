#pragma once

#include "argos/ast/ast.hpp"
#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/ir/module.hpp"

namespace argos::codegen::ir {
class ExecutableIrGenerator final {
public:
  explicit ExecutableIrGenerator(diagnostics::DiagnosticEngine &diagnostics)
      : diagnostics_(diagnostics) {}
  [[nodiscard]] argos::ir::Module generate(const ast::Program &program);

private:
  diagnostics::DiagnosticEngine &diagnostics_;
};
} // namespace argos::codegen::ir
