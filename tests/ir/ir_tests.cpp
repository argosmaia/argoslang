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
                     .parameter_types = {},
                     .slot_types = {},
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

TEST_CASE("rejects malformed closure and object IR", "[ir][oop][functional]") {
  using namespace argos::ir;
  auto module = valid_module();
  auto& function = module.functions[0];
  function.register_count = 2;
  function.register_types = {ValueType::int32, ValueType::closure};
  Instruction closure;
  closure.opcode = Opcode::make_closure;
  closure.dst = 1;
  closure.function = 99;
  function.blocks[0].instructions.insert(function.blocks[0].instructions.begin(), closure);
  REQUIRE(validate_module(module).has_value());
  function.blocks[0].instructions.front().function = 0;
  REQUIRE(validate_module(module).has_value()); // main has no environment parameter

  Instruction invoke;
  invoke.opcode = Opcode::call_closure;
  invoke.src1 = 1;
  invoke.dst = 0;
  invoke.return_type = ValueType::int32;
  invoke.arguments = {0};
  function.blocks[0].instructions.front() = invoke;
  REQUIRE(validate_module(module).has_value()); // missing argument type

  function.register_types[1] = ValueType::object;
  module.objects.push_back({"A", {ValueType::int32}});
  Instruction field;
  field.opcode = Opcode::load_field;
  field.src1 = 1;
  field.dst = 0;
  field.constant = 0;
  field.count = 1;
  function.blocks[0].instructions.front() = field;
  REQUIRE(validate_module(module).has_value()); // field out of bounds
  function.blocks[0].instructions.front().count = 0;
  REQUIRE_FALSE(validate_module(module).has_value());
  function.register_types[0] = ValueType::string;
  REQUIRE(validate_module(module).has_value());
}

TEST_CASE("optional IR rejects forged wrapping and unwrapping",
          "[optional][ir]") {
  using namespace argos::ir;
  auto module = valid_module();
  auto &f = module.functions[0];
  f.register_count = 2;
  f.register_types = {ValueType::int32, optional_type(ValueType::int32)};
  Instruction wrap;
  wrap.opcode = Opcode::make_optional;
  wrap.src1 = 0;
  wrap.dst = 1;
  f.blocks[0].instructions.insert(f.blocks[0].instructions.begin() + 1, wrap);
  REQUIRE_FALSE(validate_module(module));
  f.register_types[1] = optional_type(ValueType::boolean);
  REQUIRE(validate_module(module));
  f.register_types[1] = optional_type(ValueType::int32);
  f.blocks[0].instructions[1].opcode = Opcode::unwrap_check;
  REQUIRE(validate_module(module));
  f.blocks[0].instructions[1].src1 = 1;
  f.blocks[0].instructions[1].dst = 0;
  REQUIRE_FALSE(validate_module(module));
  f.register_types[1] = with_depth(ValueType::void_type, 1);
  REQUIRE(validate_module(module));
}

TEST_CASE("array IR verifies element and index types",
          "[optional][ir][array]") {
  using namespace argos::ir;
  auto module = valid_module();
  auto &f = module.functions[0];
  f.register_count = 3;
  f.register_types = {ValueType::int32,
                      array_type(optional_type(ValueType::int32)),
                      optional_type(ValueType::int32)};
  Instruction array;
  array.opcode = Opcode::make_array;
  array.dst = 1;
  array.arguments = {0};
  f.blocks[0].instructions.insert(f.blocks[0].instructions.begin() + 1, array);
  REQUIRE(validate_module(module));
  f.blocks[0].instructions[1].arguments = {2};
  REQUIRE_FALSE(validate_module(module));
  Instruction index;
  index.opcode = Opcode::array_load;
  index.dst = 2;
  index.src1 = 1;
  index.src2 = 0;
  f.blocks[0].instructions.insert(f.blocks[0].instructions.begin() + 2, index);
  REQUIRE_FALSE(validate_module(module));
  f.blocks[0].instructions[2].dst = 0;
  REQUIRE(validate_module(module));
  f.blocks[0].instructions[2].dst = 2;
  f.blocks[0].instructions[2].src2 = 2;
  REQUIRE(validate_module(module));
}
