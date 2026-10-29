#pragma once
#include "argos/ir/value.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace argos::ir {
enum class Opcode {
  nop,
  load_const,
  load_string,
  load_var,
  store_var,
  add,
  sub,
  mul,
  div,
  mod,
  int_to_double,
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
  call_virtual,
  hash_scalar,
  check_uint8,
  stringify_scalar,
  concat_strings,
  make_closure,
  new_object,
  load_field,
  store_field,
  load_capture,
  call_closure,
  print_int,
  print_bool,
  print_double,
  print_string,
  print_string_value,
  print_newline,
  ret,
  make_nil,
  make_optional,
  is_nil,
  array_load,
  array_store,
  make_array,
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
  std::vector<ValueType> parameter_types;
  ValueType return_type{ValueType::void_type};
  std::string embedded_code;
  std::uint32_t source_line{}, source_column{};
};
const char *opcode_name(Opcode) noexcept;
} // namespace argos::ir
