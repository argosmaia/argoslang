#include "argos/ast/ast_printer.hpp"
#include "argos/codegen/ir/ir_generator.hpp"
#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/diagnostics/diagnostic_printer.hpp"
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
      << "  " << executable
      << " lex <arquivo.argos>    - Analisa os tokens lexicais do arquivo\n"
      << "  " << executable
      << " parse <arquivo.argos>  - Realiza o parsing gramatical da sintaxe "
         "(AST)\n"
      << "  " << executable
      << " ast <arquivo.argos>    - Imprime a Árvore de Sintaxe Abstrata (AST) "
         "formatada\n"
      << "  " << executable
      << " check <arquivo.argos> [outros.argos...]  - Executa a análise "
         "semântica (verificação de tipos e escopos)\n"
      << "  " << executable
      << " run <arquivo.argos> [outros.argos...]    - Executa pela VM o "
         "subconjunto suportado\n"
      << "  " << executable
      << " repl                 - Abre o shell interativo da VM\n"
      << "  " << executable
      << " <arquivo.argos> [outros.argos...] [-S] [-o saída] - Compila no host "
         "x86-64/Linux\n"
      << "  " << executable
      << " compile <arquivo.argos> [outros.argos...] [-S] [-o saída] - Mesmo "
         "comando explícito\n";
}

void print_diagnostics(
    const argos::source::SourceFile& source_file,
    const argos::diagnostics::DiagnosticEngine& diagnostics
) {
    argos::diagnostics::DiagnosticPrinter{}.print(source_file, diagnostics, std::cerr);
}

int run_repl() {
    std::cout << "ArgosLang REPL (VM). Use :help ou :quit.\n";
    argos::repl::ReplSession session;
    session.reset();
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
    std::vector<std::filesystem::path> additional_sources;
    bool assembly_only = false;
    if (command == "lex" || command == "parse" ||
        command == "ast" || command == "check" || command == "run") {
      if (argc < 3 ||
          ((command == "lex" || command == "parse" || command == "ast") &&
           argc != 3)) {
        print_usage(argv[0]);
        return 64;
      }
        source_path = argv[2];
        for (int index = 3; index < argc; ++index)
          additional_sources.emplace_back(argv[index]);
    } else {
        const int first = command == "compile" ? 2 : 1;
        command = "compile";
        bool has_output = false;
        for (int index = first; index < argc; ++index) {
            const std::string_view argument{argv[index]};
            if (argument == "-S" && !assembly_only) {
                assembly_only = true;
            } else if (argument == "-o" && !has_output && index + 1 < argc) {
                output_path = argv[++index];
                has_output = true;
                if (output_path.empty()) {
                    print_usage(argv[0]);
                    return 64;
                }
            } else if (!argument.starts_with("-")) {
              if (source_path.empty())
                source_path = argv[index];
              else
                additional_sources.emplace_back(argv[index]);
            } else {
              print_usage(argv[0]);
              return 64;
            }
        }
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
        if (output_path.empty()) {
            output_path = source_path.stem();
            if (assembly_only) output_path += ".s";
        }
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

    for (const auto &input : additional_sources) {
      std::error_code error;
      const bool same_output =
          command == "compile" && std::filesystem::exists(output_path) &&
          std::filesystem::equivalent(input, output_path, error);
      if (input.extension() != ".argos" || same_output ||
          (command == "compile" &&
           std::filesystem::absolute(input).lexically_normal() ==
               std::filesystem::absolute(output_path).lexically_normal())) {
        std::cerr << "argosc: arquivo-fonte adicional ou saída inválida\n";
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
    auto program = parser.parse_program();

    if (diagnostics.has_errors()) {
        print_diagnostics(*source_file, diagnostics);
        return 65;
    }

    for (const auto &input : additional_sources) {
      std::string error;
      const auto source = source_manager.load(input, error);
      if (!source) {
        std::cerr << "argosc: " << error << '\n';
        return 66;
      }
      argos::diagnostics::DiagnosticEngine local_diagnostics;
      argos::lexer::Lexer local_lexer{*source, local_diagnostics};
      argos::parser::Parser local_parser{local_lexer.tokenize(),
                                         local_diagnostics};
      auto unit = local_parser.parse_program();
      for (auto diagnostic : local_diagnostics.diagnostics()) {
        diagnostic.span.begin.source_file = input.string();
        diagnostic.span.end.source_file = input.string();
        diagnostics.report(std::move(diagnostic));
      }
      if (diagnostics.has_errors()) {
        print_diagnostics(*source_file, diagnostics);
        return 65;
      }
      for (auto &item : unit.items)
        program.items.push_back(std::move(item));
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
            argos::codegen::ir::IrGenerator generator{diagnostics, analyzer};
            const auto module = generator.generate(program);
            if (!diagnostics.has_errors()) {
                std::string compile_error;
                const int result = assembly_only
                    ? argos::driver::emit_assembly(module, output_path, compile_error)
                    : argos::driver::compile_native(module, output_path, compile_error);
                if (result != 0) {
                    std::cerr << "argosc: " << compile_error << '\n';
                    return result;
                }
                std::cout << (assembly_only ? "Assembly gerado: " : "Executável gerado: ") << output_path.string() << '\n';
            }
        }
        if (command == "run" && success && !diagnostics.has_errors()) {
            argos::codegen::ir::IrGenerator generator{diagnostics, analyzer};
            const auto module = generator.generate(program);
            if (!diagnostics.has_errors()) {
                try {
                    argos::vm::VM vm;
                    vm.load(module);
                    const auto result = vm.run();
                    std::cout << "Resultado da VM: " << result << '\n';
                } catch (const std::exception& error) {
                    std::cerr << "argosc: erro de execução: " << error.what() << '\n';
                    return 70;
                }
            }
        }
    }

    print_diagnostics(*source_file, diagnostics);
    return diagnostics.has_errors() ? 65 : 0;
}
