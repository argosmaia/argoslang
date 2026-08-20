#pragma once

#include "argos/ast/ast.hpp"
#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/source/source_span.hpp"
#include <string_view>
#include <unordered_map>
#include <vector>

namespace argos::semantics {
enum class TypeKind : std::uint8_t {
  error,
  void_type,
  bool_type,
  byte,
  short_type,
  int_type,
  long_type,
  usize,
  isize,
  float_type,
  double_type,
  char_type,
  null_type,
  unknown,
};

struct Type final {
  TypeKind kind{TypeKind::error};
  std::size_t nullable_depth{0};

  [[nodiscard]] bool is_error() const noexcept {
    return kind == TypeKind::error;
  };

  [[nodiscard]] bool is_unknown() const noexcept {
    return kind == TypeKind::unknown;
  }
  [[nodiscard]] bool is_null() const noexcept {
    return kind == TypeKind::null_type;
  }
  [[nodiscard]] bool is_optional() const noexcept { return nullable_depth > 0; }
  [[nodiscard]] Type unwrapped() const noexcept;

  friend bool operator==(const Type &, const Type &) = default;
};

[[nodiscard]] std::string_view type_kind_name(TypeKind kind) noexcept;
[[nodiscard]] std::string type_name(const Type &type);

struct Symbol final {
  enum class Kind : std::uint8_t {
    variable,
    parameter,
    function,
  };
  std::string name;
  Kind kind{Kind::variable};
  Type type{};
  ast::Mutability mutability{ast::Mutability::immutable};
  bool initialized{false};
  std::vector<Type> parameter_types;
  Type return_type{TypeKind::void_type, 0};
  source::SourceSpan span{};
};

class SemanticAnalyzer final {
public:
  SemanticAnalyzer(diagnostics::DiagnosticEngine &diagnostics);
  [[nodiscard]] bool analyze(const ast::Program &program);
  [[nodiscard]] Type type_of(const ast::Expression &expression) const;

private:
  struct Scope final {
    std::unordered_map<std::string, Symbol> symbols;
  };

  struct FunctionContext final {
    std::string name;
    Type return_type{TypeKind::void_type, 0};
    bool saw_return{false};
  };

  void enter_scope();
  void leave_scope();

  [[nodiscard]] Symbol *lookup(std::string_view name);
  [[nodiscard]] const Symbol *lookup(std::string_view name) const;

  bool declare(Symbol symbol);
  void report(std::string_view code, const source::SourceSpan &span,
              std::string message);

  void analyze_block_item(const ast::BlockItem &item);
  void analyze_declaration(const ast::Declaration &declaration);
  void
  analyze_variable_declaration(const ast::VariableDeclaration &declaration);
  void
  analyze_function_declaration(const ast::FunctionDeclaration &declaration);

  void analyze_statement(const ast::Statement &statement);
  void analyze_block(const ast::BlockStatement &block);
  void analyze_return(const ast::ReturnStatement &statement);
  void analyze_if(const ast::IfStatement &statement);
  void analyze_expression_statement(const ast::ExpressionStatement &statement);

  Type analyze_expression(const ast::Expression &expression);
  Type analyze_identifier(const ast::IdentifierExpression &expression);
  Type analyze_unary(const ast::UnaryExpression &expression);
  Type analyze_binary(const ast::BinaryExpression &expression);
  Type analyze_assignment(const ast::AssignmentExpression &expression);
  Type analyze_call(const ast::CallExpression &expression);
  Type analyze_member_access(const ast::MemberAccessExpression &expression);
  Type analyze_postfix(const ast::PostfixExpression &expression);

  [[nodiscard]] Type resolve_type(const ast::TypeSyntax &syntax) const;

  [[nodiscard]] static bool is_numeric(Type type) noexcept;
  [[nodiscard]] static bool is_integral(Type type) noexcept;
  [[nodiscard]] static bool is_boolean(Type type) noexcept;
  [[nodiscard]] static bool is_assignable(Type from, Type to) noexcept;
  [[nodiscard]] static bool is_equality_comparable(Type left,
                                                   Type right) noexcept;
  [[nodiscard]] static Type common_numeric_type(Type left, Type right) noexcept;

  [[nodiscard]] bool require_boolean(Type type, const source::SourceSpan &span,
                                     std::string_view context);
  [[nodiscard]] bool require_lvalue(const ast::Expression &expression,
                                    std::string_view context);
  [[nodiscard]] bool is_mutable_lvalue(const ast::Expression &expression) const;

  void check_function_returns(const ast::FunctionDeclaration &declaration);

  diagnostics::DiagnosticEngine &diagnostics_;
  std::vector<Scope> scopes_;
  std::vector<FunctionContext> functions_;
  std::unordered_map<const ast::Expression *, Type> expression_types_;
};

}; // namespace argos::semantics