#pragma once
#include "argos/ir/module.hpp"

namespace argos::ir {
// Makes the implicit true continuation of conditional branches explicit.
void canonicalize_control_flow(Module &module);
// Removes unreachable blocks without inventing terminators or values.
void remove_unreachable_blocks(Module &module);
// Validates both boundaries. Folds local scalar constants and branches and
// removes unused, non-trapping results. Throws on invalid input/output IR.
void optimize_module(Module &module);
} // namespace argos::ir
