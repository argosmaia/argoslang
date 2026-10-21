#pragma once

#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/source/source_file.hpp"

#include <iosfwd>

namespace argos::diagnostics {

class DiagnosticPrinter final {
public:
  void print(const source::SourceFile& source_file,
             const DiagnosticEngine& diagnostics, std::ostream& output) const;
};

} // namespace argos::diagnostics
