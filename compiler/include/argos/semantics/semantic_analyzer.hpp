#pragma once

#include "argos/ast/ast.hpp"
#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/source/source_span.hpp"
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace argos::semantics {
using SymbolId = std::uint32_t;
enum class TypeKind : std::uint8_t {
  error,
  void_type,
  bool_type,
  byte,
  sbyte,
  short_type,
  ushort_type,
  int_type,
  uint_type,
  long_type,
  ulong_type,
  usize,
  isize,
  float_type,
  double_type,
  char_type,
  string_type,
  nominal_type,
  function_type,
  array_type,
  never_type,
  null_type,
  unknown,
};

struct Type final {
  TypeKind kind{TypeKind::error};
  std::size_t nullable_depth{0};
  std::string nominal_name;
  // Function parameters followed by the return type; structural and invariant.
  std::vector<Type> signature;

  Type() = default;
  Type(TypeKind kind_value, std::size_t depth = 0,
       std::string name = {})
      : kind(kind_value), nullable_depth(depth),
        nominal_name(std::move(name)) {}

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
  [[nodiscard]] Type unwrapped() const;

  friend bool operator==(const Type &, const Type &) = default;
};

[[nodiscard]] std::string_view type_kind_name(TypeKind kind) noexcept;
[[nodiscard]] std::string type_name(const Type &type);

struct Symbol final {
  SymbolId id{0};
  enum class Kind : std::uint8_t {
    variable,
    property,
    parameter,
    function,
    type,
  };
  std::string name;
  Kind kind{Kind::variable};
  Type type{};
  ast::Mutability mutability{ast::Mutability::immutable};
  bool initialized{false};
  std::vector<Type> parameter_types{};
  std::vector<std::string> parameter_labels{};
  Type return_type{TypeKind::void_type, 0};
  source::SourceSpan span{};
  ast::Modifiers modifiers{};
  std::string owner{};
  const ast::FunctionDeclaration *declaration{nullptr};
  std::string package_name{};
  std::string component_target{};
};

class SemanticAnalyzer final {
public:
  SemanticAnalyzer(diagnostics::DiagnosticEngine &diagnostics);
  [[nodiscard]] bool analyze(const ast::Program &program);
  [[nodiscard]] Type type_of(const ast::Expression &expression) const;
  [[nodiscard]] std::optional<Type>
  coercion_of(const ast::Expression &expression) const;
  [[nodiscard]] std::optional<SymbolId>
  symbol_id_of(const ast::Expression &expression) const;
  [[nodiscard]] const Symbol *symbol_by_id(SymbolId id) const noexcept;
  [[nodiscard]] const std::vector<Symbol>& captures_of(const ast::LambdaExpression& expression) const;
  [[nodiscard]] const Symbol *
  resolved_call(const ast::CallExpression &call) const;
  [[nodiscard]] const Symbol *
  declaration_symbol(const ast::FunctionDeclaration &declaration) const;
  [[nodiscard]] std::vector<Symbol> methods_of(const std::string &name) const;
  [[nodiscard]] static std::string dispatch_key(const Symbol &symbol);
  [[nodiscard]] static std::string virtual_key(const Symbol &symbol);
  [[nodiscard]] static std::string function_key(const Symbol &symbol);
  [[nodiscard]] bool is_initialized(std::string_view name) const noexcept;

  [[nodiscard]] std::vector<const ast::ClassDeclaration *>
  class_declarations() const;
  [[nodiscard]] const std::vector<std::string> &
  bases_of(const std::string &) const;
  [[nodiscard]] const std::vector<std::unique_ptr<ast::VariableDeclaration>> &
  generated_fields(const std::string &) const;
  [[nodiscard]] const std::vector<std::unique_ptr<ast::FunctionDeclaration>> &
  generated_methods(const std::string &) const;
  [[nodiscard]] bool explicitly_initializes(const ast::FunctionDeclaration &,
                                            const std::string &) const;
  struct ComponentBinding {
    std::string receiver;
    std::string field;
  };
  [[nodiscard]] const ComponentBinding *
  component_binding(const ast::Expression &) const;

private:
  std::string current_package_;
  std::vector<const ast::ClassDeclaration *> class_order_;
  std::string resolve_nominal(const std::string &, const std::string &,
                              const std::string &) const;
  bool accessible_type(const std::string &, const source::SourceSpan &);
  void build_components(const std::string &);
  void capture_receiver(const source::SourceSpan &);
  std::unordered_map<const ast::Expression *, ComponentBinding>
      component_bindings_;
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
  void restore_initialization(const std::vector<Scope> &before);

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
  struct ClassInfo {
    const ast::ClassDeclaration* declaration{};
    std::unordered_map<std::string, Symbol> members;
    unsigned state{0};
    std::vector<std::string> bases, interfaces, permissions, components;
    std::vector<std::unique_ptr<ast::VariableDeclaration>> fields;
    std::vector<std::unique_ptr<ast::FunctionDeclaration>> generated;
    std::unordered_map<const ast::FunctionDeclaration *,
                       std::unordered_set<std::string>>
        explicit_components;
    std::unordered_map<std::string, std::vector<Symbol>> methods;
  };
  Type resolve_overload(const ast::CallExpression &,
                        const std::vector<Symbol> &);
  bool accessible(const Symbol &, const source::SourceSpan &);
  bool immutable_data_type(const Type &, std::vector<std::string> &) const;
  std::unordered_map<const ast::FunctionDeclaration *, Symbol>
      declaration_symbols_;
  std::unordered_map<const ast::CallExpression *, Symbol> resolved_calls_;
  std::unordered_map<std::string, std::vector<Symbol>> global_overloads_;
  bool super_initialized_{true};
  bool calling_super_{};
  bool resolving_callee_{};
  std::string superclass(const std::string &) const;
  void build_class(const std::string& name);
  void analyze_class(const ast::ClassDeclaration& declaration);
  Symbol function_symbol(const ast::FunctionDeclaration& declaration);
  Type analyze_lambda(const ast::LambdaExpression& expression);
  Type check_call(const ast::CallExpression &expression, const Symbol &function,
                  bool analyzed = false);
  const Symbol* find_member(const Type& type, const std::string& name,
                            const source::SourceSpan& span);
  bool is_subtype(const std::string& derived, const std::string& base) const;
  std::unordered_map<std::string, ClassInfo> classes_;
  std::string current_class_;
  std::vector<std::size_t> lambda_scope_boundaries_;
  std::vector<const ast::LambdaExpression*> active_lambdas_;
  std::unordered_map<const ast::LambdaExpression*, std::vector<Symbol>> lambda_captures_;
  Symbol* lookup_field(const std::string& name);
  const Symbol* lookup_field(const std::string& name) const;
  Symbol* assignment_symbol(const ast::Expression& expression);
  bool in_constructor_{false};
  bool initializing_field_{false};
  bool constructor_lambda_context_{false};
  bool accessing_member_base_{false};
  bool instance_ready() const;
  void record_coercion(const ast::Expression &expression, Type from, Type to);

  void analyze_statement(const ast::Statement &statement);
  void analyze_block(const ast::BlockStatement &block);
  void analyze_return(const ast::ReturnStatement &statement);
  void analyze_if(const ast::IfStatement &statement);
  void analyze_expression_statement(const ast::ExpressionStatement &statement);

  Type analyze_expression(const ast::Expression &expression,
                          std::optional<Type> expected = std::nullopt);
  Type analyze_identifier(const ast::IdentifierExpression &expression);
  Type analyze_unary(const ast::UnaryExpression &expression);
  Type analyze_binary(const ast::BinaryExpression &expression);
  Type analyze_assignment(const ast::AssignmentExpression &expression);
  Type analyze_call(const ast::CallExpression &expression);
  Type analyze_member_access(const ast::MemberAccessExpression &expression);
  Type analyze_postfix(const ast::PostfixExpression &expression);

  [[nodiscard]] Type resolve_type(const ast::TypeSyntax &syntax);

  [[nodiscard]] static bool is_numeric(Type type) noexcept;
  [[nodiscard]] static bool is_integral(Type type) noexcept;
  [[nodiscard]] static bool is_boolean(Type type) noexcept;
  [[nodiscard]] bool is_assignable(Type from, Type to) const;
  [[nodiscard]] bool is_assignable_expression(
      Type from, Type to, const ast::Expression &expression) const;
  [[nodiscard]] static bool is_equality_comparable(Type left,
                                                   Type right) noexcept;
  [[nodiscard]] static Type common_numeric_type(Type left, Type right) noexcept;

  [[nodiscard]] bool require_boolean(Type type, const source::SourceSpan &span,
                                     std::string_view context);
  [[nodiscard]] bool require_lvalue(const ast::Expression &expression,
                                    std::string_view context);
  [[nodiscard]] bool is_mutable_lvalue(const ast::Expression &expression) const;

  void check_function_returns(const ast::FunctionDeclaration &declaration);
  [[nodiscard]] bool statement_guarantees_return(const ast::Statement &statement) const;
  [[nodiscard]] bool block_guarantees_return(const ast::BlockStatement &block) const;

  diagnostics::DiagnosticEngine &diagnostics_;
  std::vector<Scope> scopes_;
  std::vector<FunctionContext> functions_;
  std::size_t loop_depth_{0};
  std::unordered_map<const ast::Expression *, Type> expression_types_;
  std::unordered_map<const ast::Expression *, Type> expression_coercions_;
  std::unordered_map<const ast::Expression *, SymbolId> expression_symbols_;
  std::vector<Symbol> symbol_records_;
  bool analyzing_assignment_target_{false};
};

}; // namespace argos::semantics
