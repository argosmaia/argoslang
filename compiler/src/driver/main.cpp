#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/lexer/lexer.hpp"
#include "argos/lexer/token_kind.hpp"
#include "argos/source/source_manager.hpp"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void print_usage(std::string_view executable) {
    std::cerr
        << "ArgosLang compiler bootstrap\n\n"
        << "Uso:\n"
        << "  " << executable << " lex <arquivo.argos>\n";
}

void print_diagnostics(
    const std::filesystem::path& path,
    const argos::diagnostics::DiagnosticEngine& diagnostics
) {
    for (const auto& diagnostic : diagnostics.diagnostics()) {
        std::cerr
            << path.string()
            << ':'
            << diagnostic.span.begin.line
            << ':'
            << diagnostic.span.begin.column
            << ": error["
            << diagnostic.code
            << "]: "
            << diagnostic.message
            << '\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        print_usage(argc > 0 ? argv[0] : "argosc");
        return 64;
    }

    const std::string_view command = argv[1];
    const std::filesystem::path source_path = argv[2];

    if (command != "lex") {
        std::cerr << "comando desconhecido: " << command << '\n';
        print_usage(argv[0]);
        return 64;
    }

    argos::source::SourceManager source_manager;
    std::string load_error;

    const auto source_file = source_manager.load(
        source_path,
        load_error
    );

    if (!source_file.has_value()) {
        std::cerr << "argosc: " << load_error << '\n';
        return 66;
    }

    argos::diagnostics::DiagnosticEngine diagnostics;
    argos::lexer::Lexer lexer{*source_file, diagnostics};
    const auto tokens = lexer.tokenize();

    for (const auto& token : tokens) {
        std::cout
            << std::left
            << std::setw(22)
            << argos::lexer::token_kind_name(token.kind)
            << "  "
            << std::quoted(token.lexeme)
            << "  @ "
            << token.span.begin.line
            << ':'
            << token.span.begin.column
            << '\n';
    }

    print_diagnostics(source_path, diagnostics);
    return diagnostics.has_errors() ? 65 : 0;
}
