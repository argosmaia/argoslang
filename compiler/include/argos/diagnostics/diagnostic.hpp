#pragma once

#include "argos/source/source_span.hpp"

#include <string>

namespace argos::diagnostics {

enum class Severity {
    warning,
    error,
};

struct Diagnostic final {
    Severity severity{Severity::error};
    std::string code;
    std::string message;
    source::SourceSpan span{};
};

} // namespace argos::diagnostics
