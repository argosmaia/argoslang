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
  std::vector<ValueType> capture_types{};
  std::vector<ValueType> parameter_types;
  std::vector<ValueType> slot_types;
  std::vector<ValueType> register_types;
  ValueType return_type{ValueType::void_type};
  std::vector<BasicBlock> blocks;
};
struct ObjectLayout {
  std::string name;
  std::vector<ValueType> fields;
  std::vector<std::uint32_t> ancestors{};
  std::vector<FunctionId> methods{};
  std::vector<std::uint32_t> owned_fields{};
};
struct Module {
  std::vector<ObjectLayout> objects{};
  std::vector<Constant> constants;
  std::vector<Function> functions;
  std::vector<FunctionId> virtual_functions{};
};
} // namespace argos::ir
