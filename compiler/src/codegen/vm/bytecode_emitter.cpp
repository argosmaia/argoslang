#include "argos/codegen/vm/bytecode_emitter.hpp"
#include "argos/ir/instruction.hpp"
#include <sstream>
namespace argos::codegen::vm {
std::string BytecodeEmitter::emit(const argos::ir::Module &m) const {
  std::ostringstream o;
  for (const auto &f : m.functions) {
    o << ".func " << f.name << " [id=" << f.id
      << ", params=" << f.parameter_count << "]\n";
    for (const auto &b : f.blocks) {
      o << "L" << b.label << ":\n";
      for (const auto &i : b.instructions)
        o << "  " << argos::ir::opcode_name(i.opcode) << "\n";
    }
  }
  return o.str();
}
} // namespace argos::codegen::vm
