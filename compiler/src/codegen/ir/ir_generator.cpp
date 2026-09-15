#include "argos/codegen/ir/ir_generator.hpp"

namespace argos::codegen::ir {

IrGenerator::IrGenerator(diagnostics::DiagnosticEngine &d) : diagnostics_(d) {}
argos::ir::Module IrGenerator::generate(const argos::ast::Program &) {
  return {};
}
} // namespace argos::codegen::ir
