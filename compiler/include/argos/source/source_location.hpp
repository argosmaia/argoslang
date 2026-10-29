#pragma once

#include <cstddef>
#include <string>

namespace argos::source {

struct SourceLocation final {
    std::size_t offset{0};
    std::size_t line{1};
    std::size_t column{1};
    std::string source_file{};

    friend constexpr bool operator==(
        const SourceLocation&,
        const SourceLocation&
    ) = default;
};

} // namespace argos::source
