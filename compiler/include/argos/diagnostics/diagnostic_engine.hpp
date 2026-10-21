#pragma once

#include "argos/diagnostics/diagnostic.hpp"

#include <string>
#include <vector>

namespace argos::diagnostics {

class DiagnosticEngine final {
public:
    void report(Diagnostic diagnostic);

    void error(
        std::string code,
        std::string message,
        source::SourceSpan span,
        std::string help = {}
    );

    [[nodiscard]]
    bool has_errors() const noexcept;

    [[nodiscard]]
    const std::vector<Diagnostic>& diagnostics() const noexcept;

private:
    std::vector<Diagnostic> diagnostics_;
};

} // namespace argos::diagnostics
