#pragma once
#include "argos/ast/ast.hpp"
#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/ir/module.hpp"
#include "argos/semantics/semantic_analyzer.hpp"
namespace argos::codegen::ir {

class IrGenerator final {
public:
  IrGenerator(diagnostics::DiagnosticEngine &diagnostics,
              const semantics::SemanticAnalyzer &semantics);
  [[nodiscard]] argos::ir::Module generate(const argos::ast::Program &program);

private:
  diagnostics::DiagnosticEngine &diagnostics_;
  const semantics::SemanticAnalyzer &semantics_;
};

} // namespace argos::codegen::ir
