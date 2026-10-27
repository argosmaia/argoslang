#include "argos/repl/repl_session.hpp"

#include "argos/ast/ast.hpp"
#include "argos/codegen/ir/ir_generator.hpp"
#include "argos/diagnostics/diagnostic_catalog.hpp"
#include "argos/lexer/lexer.hpp"
#include "argos/parser/parser.hpp"
#include "argos/semantics/semantic_analyzer.hpp"
#include "argos/vm/vm.hpp"

#include <algorithm>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace argos::repl {
namespace {

constexpr std::string_view state_marker = "__argos_repl_state_9e2e6f15__";

struct Variable final {
  std::string name;
  std::string type;
  std::string value;
  bool mutable_{false};
  bool initialized{false};
};

std::string_view trim(std::string_view text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) return {};
  const auto last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

bool is_function_definition(std::string_view input) {
  input = trim(input);
  return input.starts_with("function ");
}

std::string variable_source(const Variable &variable) {
  std::string source = "var " +
      std::string(variable.mutable_ ? "mut " : "") + "`" + variable.name +
      "`: " + variable.type;
  if (variable.initialized) source += " = " + variable.value;
  return source + ";\n";
}

std::optional<std::string> executable_type(const semantics::Type &type) {
  if (type.nullable_depth != 0) return std::nullopt;
  if (type.kind == semantics::TypeKind::int_type) return "int";
  if (type.kind == semantics::TypeKind::bool_type) return "bool";
  if (type.kind == semantics::TypeKind::float_type) return "float";
  if (type.kind == semantics::TypeKind::double_type) return "double";
  return std::nullopt;
}

std::optional<std::string> executable_type(const ast::TypeSyntax &type) {
  if (type.nullable_depth != 0) return std::nullopt;
  if (type.name == "int") return "int";
  if (type.name == "bool") return "bool";
  if (type.name == "float") return "float";
  if (type.name == "double") return "double";
  return std::nullopt;
}

bool is_string_type(const ast::TypeSyntax &type) {
  return type.nullable_depth == 0 && type.name == "string";
}

std::string compose_source(const std::vector<std::string> &functions,
                           const std::vector<Variable> &variables,
                           std::string_view input, bool echo_expression,
                           bool function_definition,
                           const std::vector<Variable> &snapshot_variables) {
  std::string content;
  for (const auto &function : functions) content += function + '\n';
  content += "function main() -> int {\n";
  for (const auto &variable : variables) content += variable_source(variable);
  if (!function_definition) {
    if (echo_expression) {
      input = trim(input);
      if (!input.empty() && input.back() == ';') input.remove_suffix(1);
      content += "print(" + std::string(input) + ")\n";
    } else {
      content += input;
      content += '\n';
    }
  }
  const bool has_initialized_snapshot =
      std::any_of(snapshot_variables.begin(), snapshot_variables.end(),
                  [](const Variable &variable) { return variable.initialized; });
  if (has_initialized_snapshot) {
    content += "print(\"" + std::string(state_marker) + "\")\n";
    for (const auto &variable : snapshot_variables)
      if (variable.initialized)
        content += "print(`" + variable.name + "`)\n";
  }
  content += "return 0\n}\n";
  return content;
}

const ast::FunctionDeclaration *main_function(const ast::Program &program) {
  for (const auto &item : program.items)
    if (const auto *function =
            dynamic_cast<const ast::FunctionDeclaration *>(item.get());
        function && function->name == "main")
      return function;
  return nullptr;
}

bool is_print_call(const ast::Expression &expression) {
  const auto *call = dynamic_cast<const ast::CallExpression *>(&expression);
  const auto *callee =
      call ? dynamic_cast<const ast::IdentifierExpression *>(call->callee.get())
           : nullptr;
  return callee && callee->name == "print";
}

class State final {
public:
  bool execute(const std::string &input, std::ostream &output,
               source::SourceFile &source_file,
               diagnostics::DiagnosticEngine &diagnostics) {
    const bool function_definition = is_function_definition(input);
    auto next_functions = functions_;
    if (function_definition) next_functions.push_back(input);

    source::SourceFile probe_source{
        .path = "<repl>",
        .content = compose_source(next_functions, variables_, input, false,
                                  function_definition, {})};
    diagnostics::DiagnosticEngine probe_diagnostics;
    lexer::Lexer probe_lexer{probe_source, probe_diagnostics};
    parser::Parser probe_parser{probe_lexer.tokenize(), probe_diagnostics};
    auto probe_program = probe_parser.parse_program();
    semantics::SemanticAnalyzer probe_analyzer{probe_diagnostics};
    const bool probe_valid =
        !probe_diagnostics.has_errors() && probe_analyzer.analyze(probe_program) &&
        !probe_diagnostics.has_errors();

    bool echo_expression = false;
    auto next_variables = variables_;
    if (probe_valid && !function_definition) {
      if (const auto *entry = main_function(probe_program)) {
        const auto first_input = variables_.size();
        const auto input_end = entry->body->items.size() > 0
                                   ? entry->body->items.size() - 1
                                   : 0;
        for (std::size_t index = first_input; index < input_end; ++index) {
          const auto *declaration =
              dynamic_cast<const ast::VariableDeclaration *>(
                  entry->body->items[index].get());
          if (!declaration) continue;
          const bool is_string_declaration =
              (declaration->type && is_string_type(*declaration->type)) ||
              (!declaration->type && declaration->initializer &&
               probe_analyzer.type_of(*declaration->initializer).kind ==
                   semantics::TypeKind::string_type);
          if (is_string_declaration) {
            diagnostics.emit(
                diagnostics::catalog::compiler::unsupported_lowering,
                declaration->span,
                "o REPL ainda não consegue manter variáveis string entre entradas");
            return false;
          }
          std::optional<std::string> type;
          if (declaration->type)
            type = executable_type(*declaration->type);
          else if (declaration->initializer)
            type = executable_type(
                probe_analyzer.type_of(*declaration->initializer));
          if (!type) {
            diagnostics.emit(diagnostics::catalog::compiler::unsupported_lowering, declaration->span,
                             "o REPL ainda não consegue preservar closures ou objetos entre entradas");
            return false;
          }
          if (type) {
            next_variables.push_back(
                {declaration->name, *type, {},
                 declaration->mutability == ast::Mutability::mutable_,
                 declaration->initializer != nullptr});
          }
        }

        if (input_end == first_input + 1) {
          const auto *statement =
              dynamic_cast<const ast::ExpressionStatement *>(
                  entry->body->items[first_input].get());
          if (statement && !is_print_call(*statement->expression)) {
            const auto type =
                executable_type(probe_analyzer.type_of(*statement->expression));
            echo_expression = type.has_value();
          }
        }
      }
    }
    if (probe_valid)
      for (auto &variable : next_variables)
        variable.initialized = probe_analyzer.is_initialized(variable.name);

    source_file = source::SourceFile{
        .path = "<repl>",
        .content = compose_source(next_functions, variables_, input,
                                  echo_expression, function_definition,
                                  next_variables)};
    lexer::Lexer lexer{source_file, diagnostics};
    parser::Parser parser{lexer.tokenize(), diagnostics};
    auto program = parser.parse_program();
    if (diagnostics.has_errors()) return false;

    semantics::SemanticAnalyzer analyzer{diagnostics};
    if (!analyzer.analyze(program) || diagnostics.has_errors()) return false;
    for (auto &variable : next_variables)
      variable.initialized = analyzer.is_initialized(variable.name);

    codegen::ir::IrGenerator generator{diagnostics, analyzer};
    const auto module = generator.generate(program);
    if (diagnostics.has_errors()) return false;

    std::ostringstream vm_output;
    try {
      vm::VM machine;
      machine.load(module);
      machine.set_output(vm_output);
      (void)machine.run();
    } catch (const std::exception &error) {
      diagnostics.emit(diagnostics::catalog::runtime::execution_failure,
                       program.span, error.what());
      output << vm_output.str();
      return false;
    }

    std::string captured = vm_output.str();
    std::vector<Variable *> snapshot_variables;
    for (auto &variable : next_variables)
      if (variable.initialized) snapshot_variables.push_back(&variable);
    if (!snapshot_variables.empty()) {
      const std::string marker = std::string(state_marker) + '\n';
      const auto marker_position = captured.rfind(marker);
      if (marker_position == std::string::npos) {
        diagnostics.error("ARGOS-R0001",
                          "não consegui guardar os valores desta sessão",
                          program.span);
        return false;
      }
      std::istringstream values{
          captured.substr(marker_position + marker.size())};
      for (auto *variable : snapshot_variables) {
        if (!std::getline(values, variable->value)) {
          diagnostics.error("ARGOS-R0001",
                            "não consegui guardar o valor de " + variable->name,
                            program.span);
          return false;
        }
      }
      captured.resize(marker_position);
    }

    functions_ = std::move(next_functions);
    variables_ = std::move(next_variables);
    output << captured;
    return true;
  }

  void reset() {
    variables_.clear();
    functions_.clear();
  }

  [[nodiscard]] std::vector<std::string> completion_candidates() const {
    std::vector<std::string> result{
        ":help", ":quit", ":q", ":reset", "function", "var", "mut",
        "if", "else", "while", "break", "continue", "return", "print",
        "true", "false", "nil", "int", "float", "double", "bool", "char",
        "string", "void"};
    for (const auto &variable : variables_) result.push_back(variable.name);
    for (const auto &definition : functions_) {
      const auto start = definition.find_first_not_of(" \t");
      const auto name_start = definition.find(' ', start);
      if (name_start == std::string::npos) continue;
      const auto name_end = definition.find('(', name_start + 1);
      if (name_end != std::string::npos)
        result.push_back(
            definition.substr(name_start + 1, name_end - name_start - 1));
    }
    return result;
  }

private:
  std::vector<Variable> variables_;
  std::vector<std::string> functions_;
};

State session;

} // namespace

bool ReplSession::execute(const std::string &input, std::ostream &output,
                          source::SourceFile &source_file,
                          diagnostics::DiagnosticEngine &diagnostics) {
  return session.execute(input, output, source_file, diagnostics);
}

void ReplSession::reset() { session.reset(); }

std::vector<std::string> ReplSession::completion_candidates() const {
  return session.completion_candidates();
}

} // namespace argos::repl
