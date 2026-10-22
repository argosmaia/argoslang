#pragma once

#include "argos/source/source_span.hpp"

#include <string>
#include <string_view>

namespace argos::diagnostics {

enum class Severity {
    error,
    warning,
    note,
};

[[nodiscard]] constexpr std::string_view severity_name(Severity severity) noexcept {
    switch (severity) {
    case Severity::error: return "error";
    case Severity::warning: return "warning";
    case Severity::note: return "note";
    }
    return "error";
}

struct Diagnostic final {
    Severity severity{Severity::error};
    std::string code;
    std::string message;
    std::string help;
    source::SourceSpan span{};
};

} // namespace argos::diagnostics
