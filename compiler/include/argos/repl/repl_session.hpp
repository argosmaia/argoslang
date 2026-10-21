#pragma once

#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/source/source_manager.hpp"

#include <iosfwd>
#include <string>

namespace argos::repl {

// Uma sessão interativa preserva declarações entre entradas, mas sempre passa
// cada entrada pelo frontend completo antes de a interpretar.
class ReplSession final {
public:
  [[nodiscard]] bool execute(const std::string& input, std::ostream& output,
                             source::SourceFile& source,
                             diagnostics::DiagnosticEngine& diagnostics);
  void reset();
};

} // namespace argos::repl
