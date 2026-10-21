#include "argos/diagnostics/diagnostic_engine.hpp"

#include <utility>

namespace argos::diagnostics {

void DiagnosticEngine::report(Diagnostic diagnostic) {
    diagnostics_.push_back(std::move(diagnostic));
}

void DiagnosticEngine::error(
    std::string code,
    std::string message,
    source::SourceSpan span,
    std::string help
) {
    report(Diagnostic{
        .severity = Severity::error,
        .code = std::move(code),
        .message = std::move(message),
        .help = std::move(help),
        .span = span,
    });
}

bool DiagnosticEngine::has_errors() const noexcept {
    for (const auto& diagnostic : diagnostics_) {
        if (diagnostic.severity == Severity::error) {
            return true;
        }
    }

    return false;
}

const std::vector<Diagnostic>&
DiagnosticEngine::diagnostics() const noexcept {
    return diagnostics_;
}

} // namespace argos::diagnostics
