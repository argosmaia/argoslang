#include "argos/codegen/native/avr/avr_codegen.hpp"
#include <sstream>
namespace argos::codegen::native::avr {
std::string CodeGenerator::generate(const argos::ir::Module &m) {
  std::ostringstream o;
  o << ".text\n";
  for (const auto &f : m.functions)
    o << ".global " << f.name << "\n" << f.name << ":\n    ret\n";
  return o.str();
}
} // namespace argos::codegen::native::avr
