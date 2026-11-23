#include "argos/ir/optimizer.hpp"
#include "argos/ir/validator.hpp"
#include "argos/vm/vm.hpp"
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
using namespace argos::ir;

namespace {
Module fixture() {
  Module m;
  m.constants = {{0, "20"}, {1, "22"}};
  Function f;
  f.name = "main";
  f.return_type = ValueType::int32;
  f.register_count = 3;
  f.register_types = {ValueType::int32, ValueType::int32, ValueType::int32};
  Instruction a, b, sum, ret;
  a.opcode = b.opcode = Opcode::load_const;
  a.dst = 0;
  b.dst = 1;
  b.constant = 1;
  sum.opcode = Opcode::add;
  sum.dst = 2;
  sum.src1 = 0;
  sum.src2 = 1;
  ret.opcode = Opcode::ret;
  ret.src1 = 2;
  f.blocks = {{0, {a, b, sum, ret}}};
  m.functions.push_back(f);
  return m;
}
} // namespace
TEST_CASE("optimizer folds arithmetic without changing execution",
          "[ir][optimizer]") {
  auto m = fixture();
  argos::vm::VM vm;
  vm.load(m);
  REQUIRE(vm.run() == 42);
  optimize_module(m);
  REQUIRE_FALSE(validate_module(m));
  REQUIRE(m.functions[0].blocks[0].instructions.size() == 2);
  REQUIRE(vm.run() == 42);
}
TEST_CASE("IR verifier rejects undefined values and incomplete blocks",
          "[ir][validation]") {
  auto m = fixture();
  auto &instructions = m.functions[0].blocks[0].instructions;
  SECTION("undefined register") { instructions.erase(instructions.begin()); }
  SECTION("no terminator") { instructions.pop_back(); }
  SECTION("empty block") { instructions.clear(); }
  SECTION("instruction after terminator") {
    instructions.push_back(instructions.front());
  }
  SECTION("unknown opcode") {
    instructions.front().opcode = static_cast<Opcode>(65535);
  }
  SECTION("duplicate main") {
    auto copy = m.functions[0];
    copy.id = 1;
    m.functions.push_back(copy);
  }
  REQUIRE(validate_module(m));
  REQUIRE_THROWS_AS(optimize_module(m), std::runtime_error);
}
TEST_CASE("IR verifier intersects definitions at branches and loop backedges",
          "[ir][validation]") {
  auto m = fixture();
  auto &f = m.functions[0];
  f.register_types[0] = ValueType::boolean;
  m.constants[0].value = "1";
  auto define = f.blocks[0].instructions[1];
  Instruction branch, left, right, ret;
  branch.opcode = Opcode::jmp_if_false;
  branch.src1 = 0;
  branch.label = 2;
  left.opcode = right.opcode = Opcode::jmp;
  left.label = 1;
  right.label = 3;
  ret.opcode = Opcode::ret;
  ret.src1 = 1;
  f.blocks = {{0, {f.blocks[0].instructions[0], branch, left}},
              {1, {define, right}},
              {2, {right}},
              {3, {ret}}};
  REQUIRE(validate_module(m));
  f.blocks[2].instructions.insert(f.blocks[2].instructions.begin(), define);
  REQUIRE_FALSE(validate_module(m));
  f.blocks[0].instructions.erase(f.blocks[0].instructions.begin());
  REQUIRE(validate_module(m));
}
TEST_CASE("IR verifier tracks initialization of slots independently",
          "[ir][validation]") {
  auto m = fixture();
  auto &f = m.functions[0];
  f.slot_count = 1;
  f.slot_types = {ValueType::int32};
  f.blocks[0].instructions[0].opcode = Opcode::load_var;
  REQUIRE(validate_module(m));
}

TEST_CASE("backedges cannot initialize values used on the first iteration",
          "[ir][validation]") {
  auto m = fixture();
  auto &f = m.functions[0];
  Instruction jump;
  jump.opcode = Opcode::jmp;
  jump.label = 1;
  Instruction use;
  use.opcode = Opcode::store_var;
  use.src1 = 1;
  f.slot_count = 1;
  f.slot_types = {ValueType::int32};
  const auto define = f.blocks[0].instructions[1];
  f.blocks = {{0, {jump}}, {1, {use, define, jump}}};
  REQUIRE(validate_module(m));
  f.blocks[0].instructions.insert(f.blocks[0].instructions.begin(), define);
  REQUIRE_FALSE(validate_module(m));
}

TEST_CASE("optimizer preserves unused arithmetic traps", "[ir][optimizer]") {
  auto m = fixture();
  auto &f = m.functions[0];
  m.constants[1].value = "0";
  f.blocks[0].instructions[2].opcode = Opcode::mod;
  f.blocks[0].instructions.back().src1 = 0;
  optimize_module(m);
  argos::vm::VM vm;
  vm.load(m);
  REQUIRE_THROWS_AS(vm.run(), std::runtime_error);
}
