#pragma once

#include "argos/source/source_location.hpp"

namespace argos::source {

struct SourceSpan final {
    SourceLocation begin{};
    SourceLocation end{};

    friend constexpr bool operator==(
        const SourceSpan&,
        const SourceSpan&
    ) = default;
};

} // namespace argos::source
