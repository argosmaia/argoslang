#pragma once
#include "argos/ir/value.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace argos::ir {
enum class Opcode {
  nop,
  load_const,
  load_var,
  store_var,
  add,
  sub,
  mul,
  div,
  mod,
  cmp_eq,
  cmp_ne,
  cmp_lt,
  cmp_le,
  cmp_gt,
  cmp_ge,
  logical_not,
  neg,
  jmp,
  jmp_if_false,
  call,
  print_int,
  print_bool,
  print_string,
  print_newline,
  ret,
  unwrap_check,
  c_embed
};
struct Instruction {
  Opcode opcode{Opcode::nop};
  Register dst{}, src1{}, src2{};
  Slot slot{};
  ConstantId constant{};
  FunctionId function{};
  LabelId label{};
  std::uint32_t count{};
  std::vector<Register> arguments;
  std::string embedded_code;
};
const char *opcode_name(Opcode) noexcept;
} // namespace argos::ir
