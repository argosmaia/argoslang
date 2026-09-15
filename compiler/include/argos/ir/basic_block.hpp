#pragma once
#include "argos/ir/instruction.hpp"
#include <vector>
namespace argos::ir {
struct BasicBlock {
  LabelId label{};
  std::vector<Instruction> instructions;
};
} // namespace argos::ir
