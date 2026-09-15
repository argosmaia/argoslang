#include "argos/codegen/native/riscv64/riscv64_codegen.hpp"
#include <sstream>
namespace argos::codegen::native::riscv64 {
std::string CodeGenerator::generate(const argos::ir::Module &m) {
  std::ostringstream o;
  o << ".text\n";
  for (const auto &f : m.functions)
    o << ".globl " << f.name << "\n" << f.name << ":\n    ret\n";
  return o.str();
}
} // namespace argos::codegen::native::riscv64
