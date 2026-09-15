#pragma once
#include "argos/codegen/native/machine/machine_instruction.hpp"
#include <string>
#include <vector>
namespace argos::codegen::native::machine {
struct MachineFunction {
  std::string name;
  std::vector<MachineInstruction> instructions;
};
struct MachineModule {
  std::vector<MachineFunction> functions;
};
} // namespace argos::codegen::native::machine
