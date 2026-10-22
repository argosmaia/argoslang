#include "argos/diagnostics/diagnostic_engine.hpp"

#include <utility>

namespace argos::diagnostics {

void DiagnosticEngine::report(Diagnostic diagnostic) {
    diagnostics_.push_back(std::move(diagnostic));
}

void DiagnosticEngine::emit(const DiagnosticDefinition& definition,
                            source::SourceSpan span, std::string message,
                            std::string help) {
    report(Diagnostic{
        .severity = definition.severity,
        .code = std::string(definition.code),
        .title = std::string(definition.title),
        .message = std::move(message),
        .explanation = std::string(definition.explanation),
        .help = help.empty() ? std::string(definition.help) : std::move(help),
        .span = span,
    });
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
        .title = {},
        .message = std::move(message),
        .explanation = {},
        .help = std::move(help),
        .span = span,
    });
}

void DiagnosticEngine::warning(std::string code, std::string message,
                               source::SourceSpan span, std::string help) {
    report(Diagnostic{.severity = Severity::warning, .code = std::move(code),
                      .title = {},
                      .message = std::move(message), .explanation = {},
                      .help = std::move(help), .span = span});
}

void DiagnosticEngine::note(std::string code, std::string message,
                            source::SourceSpan span) {
    report(Diagnostic{.severity = Severity::note, .code = std::move(code),
                      .title = {},
                      .message = std::move(message), .explanation = {},
                      .help = {}, .span = span});
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
