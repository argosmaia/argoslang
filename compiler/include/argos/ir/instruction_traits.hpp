#pragma once
#include "argos/ir/instruction.hpp"

namespace argos::ir {
inline bool writes_register(Opcode opcode) {
  switch (opcode) {
  case Opcode::nop:
  case Opcode::store_var:
  case Opcode::store_field:
  case Opcode::array_store:
  case Opcode::jmp:
  case Opcode::jmp_if_false:
  case Opcode::print_int:
  case Opcode::print_bool:
  case Opcode::print_double:
  case Opcode::print_string:
  case Opcode::print_string_value:
  case Opcode::print_newline:
  case Opcode::ret:
  case Opcode::c_embed:
    return false;
  default:
    return true;
  }
}

template <typename Visitor>
void visit_register_reads(const Instruction &i, Visitor visit) {
  switch (i.opcode) {
  case Opcode::store_var:
  case Opcode::int_to_double:
  case Opcode::round_float:
  case Opcode::logical_not:
  case Opcode::raw_alloc:
  case Opcode::raw_free:
  case Opcode::neg:
  case Opcode::jmp_if_false:
  case Opcode::hash_scalar:
  case Opcode::check_uint8:
  case Opcode::check_narrow:
  case Opcode::stringify_scalar:
  case Opcode::load_field:
  case Opcode::load_capture:
  case Opcode::print_int:
  case Opcode::print_bool:
  case Opcode::print_double:
  case Opcode::print_string_value:
  case Opcode::ret:
  case Opcode::make_optional:
  case Opcode::is_nil:
  case Opcode::unwrap_check:
  case Opcode::call_closure:
    visit(i.src1);
    break;
  case Opcode::add:
  case Opcode::sub:
  case Opcode::mul:
  case Opcode::div:
  case Opcode::mod:
  case Opcode::cmp_eq:
  case Opcode::cmp_ne:
  case Opcode::cmp_lt:
  case Opcode::cmp_le:
  case Opcode::cmp_gt:
  case Opcode::cmp_ge:
  case Opcode::concat_strings:
  case Opcode::store_field:
  case Opcode::array_load:
  case Opcode::array_store:
  case Opcode::raw_load:
  case Opcode::raw_store:
    visit(i.src1);
    visit(i.src2);
    break;
  default:
    break;
  }
  switch (i.opcode) {
  case Opcode::call:
  case Opcode::call_virtual:
  case Opcode::call_closure:
  case Opcode::make_closure:
  case Opcode::make_array:
  case Opcode::array_store:
  case Opcode::raw_store:
    for (const auto r : i.arguments)
      visit(r);
    break;
  default:
    break;
  }
}
} // namespace argos::ir
