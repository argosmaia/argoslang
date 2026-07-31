#include "argos/source/source_manager.hpp"

#include <fstream>
#include <sstream>
#include <system_error>

namespace argos::source {

std::optional<SourceFile> SourceManager::load(
    const std::filesystem::path& path,
    std::string& error_message
) const {
    std::ifstream input(path, std::ios::binary);

    if (!input) {
        error_message = "não foi possível abrir o arquivo: " + path.string();
        return std::nullopt;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    if (input.bad()) {
        error_message = "falha durante a leitura do arquivo: " + path.string();
        return std::nullopt;
    }

    return SourceFile{
        .path = path,
        .content = buffer.str(),
    };
}

} // namespace argos::source
