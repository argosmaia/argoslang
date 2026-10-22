#include "argos/codegen/ir/ir_generator.hpp"

namespace argos::codegen::ir {

IrGenerator::IrGenerator(diagnostics::DiagnosticEngine &diagnostics,
                         const semantics::SemanticAnalyzer &semantics)
    : diagnostics_(diagnostics), semantics_(semantics) {}

} // namespace argos::codegen::ir
