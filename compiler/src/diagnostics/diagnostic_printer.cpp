#include "argos/diagnostics/diagnostic_printer.hpp"

#include <ostream>
#include <string>

namespace argos::diagnostics {

void DiagnosticPrinter::print(const source::SourceFile& source_file,
                              const DiagnosticEngine& diagnostics,
                              std::ostream& output) const {
  for (const auto& diagnostic : diagnostics.diagnostics()) {
    output << source_file.path.string() << ':' << diagnostic.span.begin.line
           << ':' << diagnostic.span.begin.column << ": "
           << severity_name(diagnostic.severity) << "["
           << diagnostic.code << "]: " << diagnostic.message << '\n';
    std::size_t begin = 0;
    for (std::size_t line = 1; line < diagnostic.span.begin.line && begin < source_file.content.size(); ++line) {
      const auto newline = source_file.content.find('\n', begin);
      begin = newline == std::string::npos ? source_file.content.size() : newline + 1;
    }
    const auto end = source_file.content.find('\n', begin);
    output << "  " << source_file.content.substr(begin, end - begin) << '\n'
           << "  " << std::string(diagnostic.span.begin.column > 0 ? diagnostic.span.begin.column - 1 : 0, ' ') << "^\n";
    if (!diagnostic.help.empty()) output << "  ajuda: " << diagnostic.help << '\n';
  }
}

} // namespace argos::diagnostics
