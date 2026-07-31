#pragma once

#include "argos/source/source_file.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace argos::source {

class SourceManager final {
public:
    [[nodiscard]]
    std::optional<SourceFile> load(
        const std::filesystem::path& path,
        std::string& error_message
    ) const;
};

} // namespace argos::source
