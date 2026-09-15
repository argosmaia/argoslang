#include "argos/ast/ast_printer.hpp"
#include "argos/codegen/ir/executable_ir_generator.hpp"
#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/driver/native_compile.hpp"
#include "argos/lexer/lexer.hpp"
#include "argos/lexer/token_kind.hpp"
#include "argos/parser/parser.hpp"
#include "argos/semantics/semantic_analyzer.hpp"
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
        << "  " << executable << " lex <arquivo.argos>    - Analisa os tokens lexicais do arquivo\n"
        << "  " << executable << " parse <arquivo.argos>  - Realiza o parsing gramatical da sintaxe (AST)\n"
        << "  " << executable << " ast <arquivo.argos>    - Imprime a Árvore de Sintaxe Abstrata (AST) formatada\n"
        << "  " << executable << " check <arquivo.argos>  - Executa a análise semântica (verificação de tipos e escopos)\n"
        << "  " << executable << " <arquivo.argos> [-o executável] - Compila no host x86-64/Linux\n"
        << "  " << executable << " compile <arquivo.argos> [-o executável] - Mesmo comando explícito\n";
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
    if (argc < 2) {
        print_usage(argc > 0 ? argv[0] : "argosc");
        return 64;
    }

    std::string_view command = argv[1];
    std::filesystem::path source_path;
    std::filesystem::path output_path;
    if (command == "compile") {
        if (argc != 3 && argc != 5) {
            print_usage(argv[0]);
            return 64;
        }
        source_path = argv[2];
        if (argc == 5) {
            if (std::string_view{argv[3]} != "-o") {
                print_usage(argv[0]);
                return 64;
            }
            output_path = argv[4];
        }
    } else if (command == "lex" || command == "parse" ||
               command == "ast" || command == "check") {
        if (argc != 3) {
            print_usage(argv[0]);
            return 64;
        }
        source_path = argv[2];
    } else if (argc == 2 || (argc == 4 && std::string_view{argv[2]} == "-o")) {
        command = "compile";
        source_path = argv[1];
        if (argc == 4) output_path = argv[3];
    }

    if (command != "lex" && command != "parse" && command != "ast" &&
        command != "check" && command != "compile") {
        std::cerr << "comando desconhecido: " << command << '\n';
        print_usage(argv[0]);
        return 64;
    }
    if (source_path.empty() || (command == "compile" && source_path.extension() != ".argos")) {
        print_usage(argv[0]);
        return 64;
    }
    if (command == "compile") {
        if (output_path.empty()) output_path = source_path.stem();
        std::error_code equivalent_error;
        const bool same_file = std::filesystem::exists(output_path) &&
            std::filesystem::equivalent(source_path, output_path, equivalent_error);
        if (output_path.empty() ||
            std::filesystem::absolute(source_path).lexically_normal() ==
                std::filesystem::absolute(output_path).lexically_normal() ||
            same_file) {
            std::cerr << "argosc: saída inválida ou igual ao arquivo-fonte\n";
            return 64;
        }
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
    auto tokens = lexer.tokenize();

    if (command == "lex") {
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

    argos::parser::Parser parser{std::move(tokens), diagnostics};
    const auto program = parser.parse_program();

    if (diagnostics.has_errors()) {
        print_diagnostics(source_path, diagnostics);
        return 65;
    }

    if (command == "parse") {
        std::cout
            << "[ArgosLang Compiler] Parsing concluído com sucesso para: "
            << source_path.string() << '\n'
            << "Total de declarações/instruções parsed no programa: "
            << program.items.size() << '\n';
    } else if (command == "ast") {
        argos::ast::AstPrinter printer;
        std::cout << printer.print(program);
    } else if (command == "check" || command == "compile") {
        argos::semantics::SemanticAnalyzer analyzer{diagnostics};
        const bool success = analyzer.analyze(program);

        if (command == "check" && success && !diagnostics.has_errors()) {
            std::cout
                << "[ArgosLang Compiler] Análise semântica concluída com sucesso (0 erros) para: "
                << source_path.string() << '\n';
        }
        if (command == "compile" && success && !diagnostics.has_errors()) {
            argos::codegen::ir::ExecutableIrGenerator generator{diagnostics};
            const auto module = generator.generate(program);
            if (!diagnostics.has_errors()) {
                std::string compile_error;
                const int result = argos::driver::compile_native(module, output_path, compile_error);
                if (result != 0) {
                    std::cerr << "argosc: " << compile_error << '\n';
                    return result;
                }
                std::cout << "Executável gerado: " << output_path.string() << '\n';
            }
        }
    }

    print_diagnostics(source_path, diagnostics);
    return diagnostics.has_errors() ? 65 : 0;
}
