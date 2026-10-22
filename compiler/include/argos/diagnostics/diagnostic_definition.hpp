#pragma once

#include "argos/diagnostics/diagnostic.hpp"

#include <string_view>

namespace argos::diagnostics {

// Contrato estável de um diagnóstico. Mensagens emitidas podem acrescentar
// contexto local, mas código, categoria e orientação permanecem centralizados.
struct DiagnosticDefinition final {
  std::string_view code;
  Severity severity{Severity::error};
  std::string_view title;
  std::string_view explanation;
  std::string_view help;
};

} // namespace argos::diagnostics
