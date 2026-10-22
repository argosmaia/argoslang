#include "argos/ir/validator.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {

argos::ir::Module valid_module() {
  using namespace argos::ir;
  Instruction load;
  load.opcode = Opcode::load_const;
  load.dst = 0;
  load.constant = 0;
  Instruction ret;
  ret.opcode = Opcode::ret;
  ret.src1 = 0;
  return Module{
      .constants = {{.id = 0, .value = "0"}},
      .functions = {{.id = 0,
                     .name = "main",
                     .parameter_count = 0,
                     .slot_count = 0,
                     .register_count = 1,
                     .register_types = {ValueType::int32},
                     .blocks = {{.label = 0, .instructions = {load, ret}}}}}};
}

} // namespace

TEST_CASE("validates a complete executable IR module") {
  REQUIRE_FALSE(argos::ir::validate_module(valid_module()).has_value());
}

TEST_CASE("rejects control-flow edges that target missing blocks") {
  auto module = valid_module();
  module.functions[0].blocks[0].instructions[0].opcode = argos::ir::Opcode::jmp;
  module.functions[0].blocks[0].instructions[0].label = 12;
  REQUIRE(argos::ir::validate_module(module).has_value());
}

TEST_CASE("rejects calls with mismatched parameter counts") {
  using namespace argos::ir;
  auto module = valid_module();
  Instruction callee_return;
  callee_return.opcode = Opcode::ret;
  callee_return.src1 = 0;
  Function callee;
  callee.id = 1;
  callee.name = "callee";
  callee.parameter_count = 1;
  callee.slot_count = 1;
  callee.register_count = 1;
  callee.parameter_types = {ValueType::int32};
  callee.slot_types = {ValueType::int32};
  callee.register_types = {ValueType::int32};
  callee.blocks = {{.label = 0, .instructions = {callee_return}}};
  module.functions.push_back(std::move(callee));
  module.functions[0].register_count = 2;
  Instruction call;
  call.opcode = Opcode::call;
  call.dst = 1;
  call.function = 1;
  module.functions[0].blocks[0].instructions.insert(
      module.functions[0].blocks[0].instructions.begin(), call);
  REQUIRE(argos::ir::validate_module(module).has_value());
}
