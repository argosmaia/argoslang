#include "argos/ir/instruction.hpp"
namespace argos::ir {
const char* opcode_name(Opcode o) noexcept {
 switch(o){
 case Opcode::nop:return "NOP"; case Opcode::load_const:return "LOAD_CONST";
 case Opcode::load_var:return "LOAD_VAR"; case Opcode::store_var:return "STORE_VAR";
 case Opcode::add:return "ADD"; case Opcode::sub:return "SUB"; case Opcode::mul:return "MUL";
 case Opcode::div:return "DIV"; case Opcode::mod:return "MOD";
 case Opcode::int_to_double:return "INT_TO_DOUBLE"; case Opcode::cmp_eq:return "CMP_EQ";
 case Opcode::cmp_ne:return "CMP_NE"; case Opcode::cmp_lt:return "CMP_LT";
 case Opcode::cmp_le:return "CMP_LE"; case Opcode::cmp_gt:return "CMP_GT";
 case Opcode::cmp_ge:return "CMP_GE"; case Opcode::logical_not:return "NOT";
 case Opcode::neg:return "NEG"; case Opcode::jmp:return "JMP"; case Opcode::jmp_if_false:return "JMP_IF_FALSE";
 case Opcode::call:return "CALL"; case Opcode::print_int:return "PRINT_INT";
 case Opcode::print_bool:return "PRINT_BOOL"; case Opcode::print_double:return "PRINT_DOUBLE";
 case Opcode::print_string:return "PRINT_STRING"; case Opcode::print_newline:return "PRINT_NEWLINE";
 case Opcode::ret:return "RET"; case Opcode::unwrap_check:return "UNWRAP_CHECK";
 case Opcode::c_embed:return "C_EMBED"; } return "UNKNOWN";
}}
