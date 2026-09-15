#pragma once
#include <string>
#include <vector>
namespace argos::codegen::native::machine {
enum class MachineOpcode {
  label,
  mov,
  add,
  sub,
  mul,
  div,
  load,
  store,
  cmp,
  branch,
  branch_if_false,
  call,
  ret,
  inline_asm
};
struct MachineInstruction {
  MachineOpcode opcode{MachineOpcode::label};
  std::vector<std::string> operands;
};
} // namespace argos::codegen::native::machine
