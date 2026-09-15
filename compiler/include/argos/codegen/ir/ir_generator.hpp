#pragma once
#include "argos/ast/ast.hpp"
#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/ir/module.hpp"
namespace argos::codegen::ir {

class IrGenerator final {
public:
  explicit IrGenerator(diagnostics::DiagnosticEngine &diagnostics);
  [[nodiscard]] argos::ir::Module generate(const argos::ast::Program &program);

private:
  diagnostics::DiagnosticEngine &diagnostics_;
};

} // namespace argos::codegen::ir
