#pragma once
#include "argos/ir/basic_block.hpp"
#include <string>
#include <vector>
namespace argos::ir {
struct Function {
  FunctionId id{};
  std::string name;
  std::uint32_t parameter_count{};
  Slot slot_count{};
  Register register_count{};
  std::vector<BasicBlock> blocks;
};
struct Module {
  std::vector<Constant> constants;
  std::vector<Function> functions;
};
} // namespace argos::ir
