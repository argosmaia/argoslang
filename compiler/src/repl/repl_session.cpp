#include "argos/repl/repl_session.hpp"

#include "argos/ast/ast.hpp"
#include "argos/lexer/lexer.hpp"
#include "argos/parser/parser.hpp"
#include "argos/semantics/semantic_analyzer.hpp"

#include <charconv>
#include <cmath>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace argos::repl {
namespace {

struct Value final {
  using Data = std::variant<std::monostate, std::int64_t, double, bool, std::string>;
  Data data{};
  [[nodiscard]] bool is_nil() const { return std::holds_alternative<std::monostate>(data); }
  [[nodiscard]] bool truthy() const {
    if (const auto* value = std::get_if<bool>(&data)) return *value;
    return !is_nil();
  }
  [[nodiscard]] std::string text() const {
    if (is_nil()) return "nil";
    if (const auto* value = std::get_if<std::int64_t>(&data)) return std::to_string(*value);
    if (const auto* value = std::get_if<double>(&data)) { std::ostringstream out; out << *value; return out.str(); }
    if (const auto* value = std::get_if<bool>(&data)) return *value ? "true" : "false";
    return std::get<std::string>(data);
  }
};

struct Binding final { Value value; bool mutable_{false}; };

class Interpreter final {
public:
  Interpreter(std::unordered_map<std::string, Binding>& globals, std::ostream& output)
      : globals_(globals), output_(output) {}

  void functions(const ast::Program& program) {
    functions_.clear();
    for (const auto& item : program.items)
      if (const auto* function = dynamic_cast<const ast::FunctionDeclaration*>(item.get()))
        functions_.insert_or_assign(function->name, function);
  }

  void declare(const ast::VariableDeclaration& declaration, bool only_if_missing = false) {
    if (only_if_missing && globals_.contains(declaration.name)) return;
    Value value{};
    if (declaration.initializer) value = expression(*declaration.initializer);
    globals_.insert_or_assign(declaration.name, Binding{std::move(value), declaration.mutability == ast::Mutability::mutable_});
  }

  void execute_item(const ast::BlockItem& item) {
    if (const auto* declaration = dynamic_cast<const ast::VariableDeclaration*>(&item)) { declare(*declaration); return; }
    if (const auto* statement = dynamic_cast<const ast::ExpressionStatement*>(&item)) { (void)expression(*statement->expression); return; }
    if (const auto* block = dynamic_cast<const ast::BlockStatement*>(&item)) { execute_block(*block); return; }
    if (const auto* conditional = dynamic_cast<const ast::IfStatement*>(&item)) {
      if (expression(*conditional->condition).truthy()) execute_block(*conditional->then_branch);
      else if (conditional->else_branch) execute_item(*conditional->else_branch);
      return;
    }
    if (const auto* loop = dynamic_cast<const ast::WhileStatement*>(&item)) {
      // A semântica já validou a condição; o limite protege o REPL de travar por acidente.
      for (std::size_t iterations = 0; expression(*loop->condition).truthy() && iterations < 1'000'000; ++iterations)
        execute_block(*loop->body);
    }
  }

  [[nodiscard]] Value expression(const ast::Expression& node) {
    if (const auto* integer = dynamic_cast<const ast::IntegerLiteralExpression*>(&node)) {
      std::string digits;
      for (const char character : integer->spelling) if (character != '_') digits += character;
      std::int64_t result{}; std::from_chars(digits.data(), digits.data() + digits.size(), result);
      return Value{result};
    }
    if (const auto* floating = dynamic_cast<const ast::FloatLiteralExpression*>(&node)) return Value{std::stod(floating->spelling)};
    if (const auto* string = dynamic_cast<const ast::StringLiteralExpression*>(&node)) return Value{unquote(string->spelling)};
    if (const auto* character = dynamic_cast<const ast::CharacterLiteralExpression*>(&node)) return Value{unquote(character->spelling)};
    if (const auto* boolean = dynamic_cast<const ast::BooleanLiteralExpression*>(&node)) return Value{boolean->value};
    if (dynamic_cast<const ast::NullLiteralExpression*>(&node)) return Value{};
    if (const auto* group = dynamic_cast<const ast::GroupingExpression*>(&node)) return expression(*group->expression);
    if (const auto* identifier = dynamic_cast<const ast::IdentifierExpression*>(&node)) {
      const auto found = globals_.find(identifier->name);
      return found == globals_.end() ? Value{} : found->second.value;
    }
    if (const auto* unary = dynamic_cast<const ast::UnaryExpression*>(&node)) {
      const Value operand = expression(*unary->operand);
      if (unary->op == ast::UnaryOperator::not_) return Value{!operand.truthy()};
      if (const auto* integer = std::get_if<std::int64_t>(&operand.data)) return Value{unary->op == ast::UnaryOperator::minus ? -*integer : *integer};
      if (const auto* floating = std::get_if<double>(&operand.data)) return Value{unary->op == ast::UnaryOperator::minus ? -*floating : *floating};
      return Value{};
    }
    if (const auto* assignment = dynamic_cast<const ast::AssignmentExpression*>(&node)) {
      const auto* target = dynamic_cast<const ast::IdentifierExpression*>(assignment->target.get());
      Value value = expression(*assignment->value);
      if (target) {
        const auto found = globals_.find(target->name);
        if (found != globals_.end() && found->second.mutable_) found->second.value = value;
      }
      return value;
    }
    if (const auto* binary = dynamic_cast<const ast::BinaryExpression*>(&node)) return binary_expression(*binary);
    if (const auto* call = dynamic_cast<const ast::CallExpression*>(&node)) return call_expression(*call);
    return Value{};
  }

private:
  void execute_block(const ast::BlockStatement& block) {
    for (const auto& item : block.items) execute_item(*item);
  }
  static std::string unquote(const std::string& spelling) {
    if (spelling.size() < 2) return spelling;
    return spelling.substr(1, spelling.size() - 2);
  }
  static bool equal(const Value& left, const Value& right) { return left.data == right.data; }
  static std::optional<double> number(const Value& value) {
    if (const auto* integer = std::get_if<std::int64_t>(&value.data)) return static_cast<double>(*integer);
    if (const auto* floating = std::get_if<double>(&value.data)) return *floating;
    return std::nullopt;
  }
  static Value numeric(double value, bool floating) { return floating ? Value{value} : Value{static_cast<std::int64_t>(value)}; }
  Value binary_expression(const ast::BinaryExpression& expression) {
    if (expression.op == ast::BinaryOperator::logical_and) { const Value left = this->expression(*expression.left); return Value{left.truthy() && this->expression(*expression.right).truthy()}; }
    if (expression.op == ast::BinaryOperator::logical_or) { const Value left = this->expression(*expression.left); return Value{left.truthy() || this->expression(*expression.right).truthy()}; }
    const Value left = this->expression(*expression.left);
    if (expression.op == ast::BinaryOperator::null_coalescing) return left.is_nil() ? this->expression(*expression.right) : left;
    const Value right = this->expression(*expression.right);
    if (expression.op == ast::BinaryOperator::equal || expression.op == ast::BinaryOperator::lifted_equal) return Value{equal(left, right)};
    if (expression.op == ast::BinaryOperator::not_equal) return Value{!equal(left, right)};
    const auto l = number(left); const auto r = number(right);
    if (!l || !r) return Value{};
    const bool floating = std::holds_alternative<double>(left.data) || std::holds_alternative<double>(right.data);
    switch (expression.op) {
    case ast::BinaryOperator::add: return numeric(*l + *r, floating);
    case ast::BinaryOperator::subtract: return numeric(*l - *r, floating);
    case ast::BinaryOperator::multiply: return numeric(*l * *r, floating);
    case ast::BinaryOperator::divide: return numeric(*l / *r, true);
    case ast::BinaryOperator::modulo: return numeric(std::fmod(*l, *r), floating);
    case ast::BinaryOperator::less_than: return Value{*l < *r};
    case ast::BinaryOperator::less_than_or_equal: return Value{*l <= *r};
    case ast::BinaryOperator::greater_than: return Value{*l > *r};
    case ast::BinaryOperator::greater_than_or_equal: return Value{*l >= *r};
    default: return Value{};
    }
  }
  Value call_expression(const ast::CallExpression& call) {
    const auto* callee = dynamic_cast<const ast::IdentifierExpression*>(call.callee.get());
    if (!callee) return Value{};
    if (callee->name == "print") {
      for (const auto& argument : call.arguments) output_ << expression(*argument).text();
      output_ << '\n'; return Value{};
    }
    // Funções do REPL são interpretadas em um escopo simples de parâmetros.
    const auto found = functions_.find(callee->name);
    if (found == functions_.end()) return Value{};
    std::unordered_map<std::string, Binding> saved = globals_;
    for (std::size_t i = 0; i < found->second->parameters.size() && i < call.arguments.size(); ++i)
      globals_.insert_or_assign(found->second->parameters[i]->name, Binding{expression(*call.arguments[i]), true});
    Value result{};
    for (const auto& item : found->second->body->items) {
      if (const auto* returned = dynamic_cast<const ast::ReturnStatement*>(item.get())) { if (returned->expression) result = expression(*returned->expression); break; }
      if (const auto* declaration = dynamic_cast<const ast::VariableDeclaration*>(item.get())) declare(*declaration);
      if (const auto* statement = dynamic_cast<const ast::ExpressionStatement*>(item.get())) (void)expression(*statement->expression);
    }
    globals_ = std::move(saved);
    return result;
  }
  std::unordered_map<std::string, Binding>& globals_;
  std::unordered_map<std::string, const ast::FunctionDeclaration*> functions_;
  std::ostream& output_;
};

class State final {
public:
  bool execute(const std::string& input, std::ostream& output, source::SourceFile& source,
               diagnostics::DiagnosticEngine& diagnostics) {
    const auto first = input.find_first_not_of(" \t");
    const std::string_view trimmed = first == std::string::npos ? std::string_view{} : std::string_view{input}.substr(first);
    const bool definition = trimmed.starts_with("function ") || trimmed.starts_with("func ");
    std::string content;
    for (const auto& function : functions_) content += function + '\n';
    if (definition) content += input + "\nfunction main() -> void {}";
    else {
      content += "function main() -> void {\n";
      for (const auto& variable : variables_) content += variable + '\n';
      content += input + "\n}";
    }
    source = source::SourceFile{.path = "<repl>", .content = std::move(content)};
    lexer::Lexer lexer{source, diagnostics};
    parser::Parser parser{lexer.tokenize(), diagnostics};
    const auto program = parser.parse_program();
    semantics::SemanticAnalyzer analyzer{diagnostics};
    if (diagnostics.has_errors() || !analyzer.analyze(program) || diagnostics.has_errors()) return false;
    Interpreter interpreter{bindings_, output};
    interpreter.functions(program);
    if (definition) { functions_.push_back(input); return true; }
    const ast::FunctionDeclaration* main = nullptr;
    for (const auto& item : program.items)
      if (const auto* function = dynamic_cast<const ast::FunctionDeclaration*>(item.get()); function && function->name == "main") main = function;
    if (!main || main->body->items.empty()) return true;
    const std::size_t previous_variables = variables_.size();
    for (std::size_t i = 0; i < previous_variables && i < main->body->items.size(); ++i)
      if (const auto* variable = dynamic_cast<const ast::VariableDeclaration*>(main->body->items[i].get())) interpreter.declare(*variable, true);
    const auto& item = main->body->items.back();
    if (const auto* variable = dynamic_cast<const ast::VariableDeclaration*>(item.get())) {
      interpreter.declare(*variable); variables_.push_back(input); return true;
    }
    if (const auto* statement = dynamic_cast<const ast::ExpressionStatement*>(item.get())) {
      const Value value = interpreter.expression(*statement->expression);
      const auto* call = dynamic_cast<const ast::CallExpression*>(statement->expression.get());
      const auto* id = call ? dynamic_cast<const ast::IdentifierExpression*>(call->callee.get()) : nullptr;
      if (!id || id->name != "print") output << value.text() << '\n';
    } else {
      interpreter.execute_item(*item);
    }
    return true;
  }
  void reset() { bindings_.clear(); variables_.clear(); functions_.clear(); }
private:
  std::unordered_map<std::string, Binding> bindings_;
  std::vector<std::string> variables_;
  std::vector<std::string> functions_;
};
} // namespace

static State session;
bool ReplSession::execute(const std::string& input, std::ostream& output, source::SourceFile& source, diagnostics::DiagnosticEngine& diagnostics) { return session.execute(input, output, source, diagnostics); }
void ReplSession::reset() { session.reset(); }
} // namespace argos::repl
