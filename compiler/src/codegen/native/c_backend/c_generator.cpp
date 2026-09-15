#include "argos/codegen/native/c_backend/c_generator.hpp"
#include <sstream>
namespace argos::codegen::native::c_backend {
std::string CGenerator::generate(const argos::ir::Module &m) {
  std::ostringstream o;
  o << "#include <stdint.h>\n#include <stdbool.h>\n\n";
  for (const auto &f : m.functions)
    o << "static void argos_fn_" << f.id << "(void) { /* " << f.name
      << " */ }\n";
  return o.str();
}
} // namespace argos::codegen::native::c_backend
