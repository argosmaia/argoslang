#pragma once

#include "argos/diagnostics/diagnostic_catalog.hpp"

// Mantido para inclusões existentes. Novas fases devem importar o catálogo
// central e usar argos::diagnostics::catalog::semantic::calls diretamente.
namespace argos::diagnostics::semantic_calls {
using namespace catalog::semantic::calls;
} // namespace argos::diagnostics::semantic_calls
