#pragma once

#include "argos/diagnostics/diagnostic.hpp"
#include "argos/diagnostics/diagnostic_definition.hpp"

#include <string>
#include <vector>

namespace argos::diagnostics {

class DiagnosticEngine final {
public:
    void report(Diagnostic diagnostic);

    void emit(const DiagnosticDefinition& definition, source::SourceSpan span,
              std::string message = {}, std::string help = {});

    void error(
        std::string code,
        std::string message,
        source::SourceSpan span,
        std::string help = {}
    );

    void warning(std::string code, std::string message, source::SourceSpan span,
                 std::string help = {});
    void note(std::string code, std::string message, source::SourceSpan span);

    [[nodiscard]]
    bool has_errors() const noexcept;

    [[nodiscard]]
    const std::vector<Diagnostic>& diagnostics() const noexcept;

private:
    std::vector<Diagnostic> diagnostics_;
};

} // namespace argos::diagnostics
