#include "argos/ast/ast_printer.hpp"
#include "argos/codegen/ir/executable_ir_generator.hpp"
#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/driver/native_compile.hpp"
#include "argos/lexer/lexer.hpp"
#include "argos/lexer/token_kind.hpp"
#include "argos/parser/parser.hpp"
#include "argos/repl/repl_session.hpp"
#include "argos/semantics/semantic_analyzer.hpp"
#include "argos/source/source_manager.hpp"
#include "argos/vm/vm.hpp"

#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <readline/history.h>
#include <readline/readline.h>

namespace {

argos::repl::ReplSession* active_repl_session = nullptr;

char* repl_completion_generator(const char* text, int state) {
    static std::vector<std::string> candidates;
    static std::size_t index = 0;
    if (state == 0) {
        candidates = active_repl_session ? active_repl_session->completion_candidates()
                                         : std::vector<std::string>{};
        index = 0;
    }
    const std::string_view prefix{text};
    while (index < candidates.size()) {
        const auto& candidate = candidates[index++];
        if (candidate.starts_with(prefix)) return ::strdup(candidate.c_str());
    }
    return nullptr;
}

char** repl_completion(const char* text, int start, int end) {
    (void)start;
    (void)end;
    return rl_completion_matches(text, repl_completion_generator);
}

void print_usage(std::string_view executable) {
    std::cerr
        << "ArgosLang compiler bootstrap\n\n"
        << "Uso:\n"
        << "  " << executable << " lex <arquivo.argos>    - Analisa os tokens lexicais do arquivo\n"
        << "  " << executable << " parse <arquivo.argos>  - Realiza o parsing gramatical da sintaxe (AST)\n"
        << "  " << executable << " ast <arquivo.argos>    - Imprime a Árvore de Sintaxe Abstrata (AST) formatada\n"
        << "  " << executable << " check <arquivo.argos>  - Executa a análise semântica (verificação de tipos e escopos)\n"
        << "  " << executable << " run <arquivo.argos>    - Executa pela VM o subconjunto suportado\n"
        << "  " << executable << " repl                 - Abre o interpretador interativo\n"
        << "  " << executable << " <arquivo.argos> [-o executável] - Compila no host x86-64/Linux\n"
        << "  " << executable << " compile <arquivo.argos> [-o executável] - Mesmo comando explícito\n";
}

void print_diagnostics(
    const argos::source::SourceFile& source_file,
    const argos::diagnostics::DiagnosticEngine& diagnostics
) {
    for (const auto& diagnostic : diagnostics.diagnostics()) {
        std::cerr
            << source_file.path.string()
            << ':'
            << diagnostic.span.begin.line
            << ':'
            << diagnostic.span.begin.column
            << ": " << argos::diagnostics::severity_name(diagnostic.severity) << "["
            << diagnostic.code
            << "]: "
            << diagnostic.message
            << '\n';
        std::size_t line_begin = 0;
        for (std::size_t line = 1; line < diagnostic.span.begin.line && line_begin < source_file.content.size(); ++line) {
            const auto newline = source_file.content.find('\n', line_begin);
            line_begin = newline == std::string::npos ? source_file.content.size() : newline + 1;
        }
        const auto line_end = source_file.content.find('\n', line_begin);
        std::cerr << "  " << source_file.content.substr(line_begin, line_end - line_begin) << '\n'
                  << "  " << std::string(diagnostic.span.begin.column > 0 ? diagnostic.span.begin.column - 1 : 0, ' ') << "^\n";
        if (!diagnostic.help.empty()) std::cerr << "  ajuda: " << diagnostic.help << '\n';
    }
}

int run_repl() {
    std::cout << "ArgosLang REPL. Use :help ou :quit.\n";
    argos::repl::ReplSession session;
    active_repl_session = &session;
    rl_attempted_completion_function = repl_completion;
    while (char* raw_line = readline(">>> ")) {
        std::string line{raw_line};
        std::free(raw_line);
        if (!line.empty()) add_history(line.c_str());
        if (line == ":quit" || line == ":q") break;
        if (line == ":reset") {
            session.reset();
            std::cout << "Sessão reiniciada.\n";
            continue;
        }
        if (line == ":help") {
            std::cout << "Digite uma expressão, instrução ou função, por exemplo:\n"
                      << "  6 * 7\n"
                      << "  print(\"Oi\")\n"
                      << "  var total: int = 10\n"
                      << "  if true { print(\"ok\") }\n"
                      << "  function twice(_ value: int) -> int { return value * 2 }\n"
                      << "Use :reset para limpar a sessão.\n";
            continue;
        }
        if (line.empty()) continue;
        argos::source::SourceFile source_file{
            .path = "<repl>",
            .content = {},
        };
        argos::diagnostics::DiagnosticEngine diagnostics;
        (void)session.execute(line, std::cout, source_file, diagnostics);
        print_diagnostics(source_file, diagnostics);
    }
    clear_history();
    active_repl_session = nullptr;
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argc > 0 ? argv[0] : "argosc");
        return 64;
    }

    std::string_view command = argv[1];
    if (command == "repl") {
        if (argc != 2) {
            print_usage(argv[0]);
            return 64;
        }
        return run_repl();
    }
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
               command == "ast" || command == "check" || command == "run") {
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
        command != "check" && command != "run" && command != "compile") {
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
        print_diagnostics(*source_file, diagnostics);
        return diagnostics.has_errors() ? 65 : 0;
    }

    argos::parser::Parser parser{std::move(tokens), diagnostics};
    const auto program = parser.parse_program();

    if (diagnostics.has_errors()) {
        print_diagnostics(*source_file, diagnostics);
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
    } else if (command == "check" || command == "run" || command == "compile") {
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
        if (command == "run" && success && !diagnostics.has_errors()) {
            argos::codegen::ir::ExecutableIrGenerator generator{diagnostics};
            const auto module = generator.generate(program);
            if (!diagnostics.has_errors()) {
                try {
                    argos::vm::VM vm;
                    vm.load(module);
                    const auto result = vm.run();
                    std::cout << "Resultado da VM: " << result << '\n';
                } catch (const std::exception& error) {
                    std::cerr << "argosc: erro de execução da VM: " << error.what() << '\n';
                    return 70;
                }
            }
        }
    }

    print_diagnostics(*source_file, diagnostics);
    return diagnostics.has_errors() ? 65 : 0;
}
