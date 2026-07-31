#pragma once

#include <cstddef>

namespace argos::source {

struct SourceLocation final {
    std::size_t offset{0};
    std::size_t line{1};
    std::size_t column{1};

    friend constexpr bool operator==(
        const SourceLocation&,
        const SourceLocation&
    ) = default;
};

} // namespace argos::source
