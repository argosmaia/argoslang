#pragma once

#include <filesystem>
#include <string>

namespace argos::source {

struct SourceFile final {
    std::filesystem::path path;
    std::string content;
};

} // namespace argos::source
