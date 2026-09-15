#include "argos/semantics/semantic_analyzer.hpp"

#include <utility>
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_set>

namespace argos::semantics {
namespace {

Type error_type() { return Type{TypeKind::error, 0}; }
Type unknown_type() { return Type{TypeKind::unknown, 0}; }

std::string quoted(std::string_view value) {
  return "'" + std::string(value) + "'";
}

std::optional<std::uint64_t> integer_literal_value(const ast::Expression &expression) {
  const auto *literal = dynamic_cast<const ast::IntegerLiteralExpression *>(&expression);
  if (!literal) return std::nullopt;
  std::string digits = literal->spelling;
  int base = 10;
  if (digits.size() > 2 && digits[0] == '0') {
    switch (digits[1]) {
    case 'x': case 'X': base = 16; break;
    case 'b': case 'B': base = 2; break;
    case 'o': case 'O': base = 8; break;
    default: break;
    }
    if (base != 10) digits.erase(0, 2);
  }
  digits.erase(std::remove(digits.begin(), digits.end(), '_'), digits.end());
  std::uint64_t result = 0;
  const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), result, base);
  if (error != std::errc{} || end != digits.data() + digits.size()) return std::nullopt;
  return result;
}

std::optional<std::uint64_t> integer_max(TypeKind kind) noexcept {
  using std::numeric_limits;
  switch (kind) {
  case TypeKind::byte: return numeric_limits<std::uint8_t>::max();
  case TypeKind::sbyte: return numeric_limits<std::int8_t>::max();
  case TypeKind::short_type: return numeric_limits<std::int16_t>::max();
  case TypeKind::ushort_type: return numeric_limits<std::uint16_t>::max();
  case TypeKind::int_type: return numeric_limits<std::int32_t>::max();
  case TypeKind::uint_type: return numeric_limits<std::uint32_t>::max();
  case TypeKind::long_type: return numeric_limits<std::int64_t>::max();
  case TypeKind::ulong_type: return numeric_limits<std::uint64_t>::max();
  case TypeKind::isize: return static_cast<std::uint64_t>(numeric_limits<std::intptr_t>::max());
  case TypeKind::usize: return static_cast<std::uint64_t>(numeric_limits<std::uintptr_t>::max());
  default: return std::nullopt;
  }
}

} // namespace

Type Type::unwrapped() const {
  if (nullable_depth == 0) {
    return *this;
  }
  return Type{kind, nullable_depth - 1, nominal_name};
}

std::string_view type_kind_name(TypeKind kind) noexcept {
  switch (kind) {
  case TypeKind::error:
    return "<error>";
  case TypeKind::void_type:
    return "Void";
  case TypeKind::bool_type:
    return "Bool";
  case TypeKind::byte:
    return "Byte";
  case TypeKind::sbyte: return "SByte";
  case TypeKind::short_type:
    return "Short";
  case TypeKind::ushort_type: return "UShort";
  case TypeKind::int_type:
    return "Int";
  case TypeKind::uint_type: return "UInt";
  case TypeKind::long_type:
    return "Long";
  case TypeKind::ulong_type: return "ULong";
  case TypeKind::usize:
    return "USize";
  case TypeKind::isize:
    return "ISize";
  case TypeKind::float_type:
    return "Float";
  case TypeKind::double_type:
    return "Double";
  case TypeKind::char_type:
    return "Char";
  case TypeKind::string_type: return "String";
  case TypeKind::nominal_type: return "<nominal>";
  case TypeKind::never_type: return "Never";
  case TypeKind::null_type:
    return "null";
  case TypeKind::unknown:
    return "<unknown>";
  }
  return "<unknown>";
}

std::string type_name(const Type &type) {
  std::string result = type.kind == TypeKind::nominal_type
                           ? type.nominal_name : std::string{type_kind_name(type.kind)};
  for (std::size_t i = 0; i < type.nullable_depth; ++i) {
    result += '?';
  }
  return result;
}

SemanticAnalyzer::SemanticAnalyzer(diagnostics::DiagnosticEngine &diagnostics)
    : diagnostics_(diagnostics) {}

bool SemanticAnalyzer::analyze(const ast::Program &program) {
  scopes_.clear();
  functions_.clear();
  expression_types_.clear();
  expression_coercions_.clear();
  expression_symbols_.clear();
  symbol_records_.clear();
  nominal_fields_.clear();

  enter_scope();

  for (const auto &item : program.items) {
    if (item->kind == ast::NodeKind::protocol_declaration)
      analyze_declaration(*static_cast<const ast::Declaration *>(item.get()));
  }

  // Install function signatures before analyzing calls and executable code.
  for (const auto &item : program.items) {
    if (item->kind == ast::NodeKind::function_declaration) {
      analyze_declaration(*static_cast<const ast::Declaration *>(item.get()));
    }
  }

  for (const auto &item : program.items) {
    if (item->kind == ast::NodeKind::protocol_declaration)
      analyze_protocol_declaration(
          static_cast<const ast::ProtocolDeclaration &>(*item));
  }

  // Variables and statements retain source order for definite initialization.
  for (const auto &item : program.items) {
    switch (item->kind) {
    case ast::NodeKind::function_declaration:
      analyze_function_declaration(
          static_cast<const ast::FunctionDeclaration &>(*item));
      break;
    case ast::NodeKind::protocol_declaration:
      break;
    case ast::NodeKind::variable_declaration:
      analyze_variable_declaration(
          static_cast<const ast::VariableDeclaration &>(*item));
      break;
    case ast::NodeKind::block_statement:
    case ast::NodeKind::expression_statement:
    case ast::NodeKind::return_statement:
    case ast::NodeKind::if_statement:
    case ast::NodeKind::while_statement:
    case ast::NodeKind::unsafe_block_statement:
      analyze_statement(static_cast<const ast::Statement &>(*item));
      break;
    default:
      report("ARGOS-S0002", item->span,
             "nó de AST inválido em posição de programa");
      break;
    }
  }

  leave_scope();
  return !diagnostics_.has_errors();
}

void SemanticAnalyzer::enter_scope() { scopes_.push_back(Scope{}); }

void SemanticAnalyzer::leave_scope() {
  if (!scopes_.empty()) {
    scopes_.pop_back();
  }
}

Symbol *SemanticAnalyzer::lookup(std::string_view name) {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    const auto found = it->symbols.find(std::string(name));
    if (found != it->symbols.end()) {
      return &found->second;
    }
  }
  return nullptr;
}

const Symbol *SemanticAnalyzer::lookup(std::string_view name) const {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    const auto found = it->symbols.find(std::string(name));
    if (found != it->symbols.end()) {
      return &found->second;
    }
  }
  return nullptr;
}

bool SemanticAnalyzer::declare(Symbol symbol) {
  if (scopes_.empty()) {
    enter_scope();
  }

  auto &current = scopes_.back().symbols;
  if (current.contains(symbol.name)) {
    report("ARGOS-S0001", symbol.span,
           "identificador " + quoted(symbol.name) +
               " já foi declarado neste escopo");
    return false;
  }

  symbol.id = static_cast<SymbolId>(symbol_records_.size());
  symbol_records_.push_back(symbol);
  current.emplace(symbol.name, std::move(symbol));
  return true;
}

void SemanticAnalyzer::report(std::string_view code,
                              const source::SourceSpan &span,
                              std::string message) {
  diagnostics_.error(std::string(code), std::move(message), span);
}

void SemanticAnalyzer::record_coercion(
    const ast::Expression &expression, Type from, Type to) {
  if (!from.is_error() && !to.is_error() && from != to)
    expression_coercions_[&expression] = std::move(to);
}

void SemanticAnalyzer::analyze_block_item(const ast::BlockItem &item) {
  switch (item.kind) {
  case ast::NodeKind::variable_declaration:
  case ast::NodeKind::function_declaration:
    analyze_declaration(static_cast<const ast::Declaration &>(item));
    break;
  case ast::NodeKind::block_statement:
  case ast::NodeKind::expression_statement:
  case ast::NodeKind::return_statement:
  case ast::NodeKind::if_statement:
  case ast::NodeKind::while_statement:
  case ast::NodeKind::unsafe_block_statement:
    analyze_statement(static_cast<const ast::Statement &>(item));
    break;
  default:
    report("ARGOS-S0002", item.span, "nó de AST inválido em posição de bloco");
    break;
  }
}

void SemanticAnalyzer::analyze_declaration(
    const ast::Declaration &declaration) {
  switch (declaration.kind) {
  case ast::NodeKind::variable_declaration:
    analyze_variable_declaration(
        static_cast<const ast::VariableDeclaration &>(declaration));
    break;
  case ast::NodeKind::function_declaration:
    // Declaration-only pass: register the signature. The body is
    // analyzed after all functions in the enclosing scope exist.
    {
      const auto &function =
          static_cast<const ast::FunctionDeclaration &>(declaration);
      std::vector<Type> parameters;
      std::vector<std::string> labels;
      parameters.reserve(function.parameters.size());
      labels.reserve(function.parameters.size());
      std::unordered_set<std::string> used_labels;
      for (const auto &parameter : function.parameters) {
        parameters.push_back(resolve_type(*parameter->type));
        labels.push_back(parameter->external_label);
        if (!parameter->external_label.empty() &&
            !used_labels.insert(parameter->external_label).second)
          report("ARGOS-S0347", parameter->span,
                 "rótulo de parâmetro " + quoted(parameter->external_label) +
                 " duplicado");
      }

      const Type return_type = function.return_type
                                   ? resolve_type(*function.return_type)
                                   : Type{TypeKind::void_type, 0};

      declare(Symbol{
          .name = function.name,
          .kind = Symbol::Kind::function,
          .type = return_type,
          .mutability = ast::Mutability::immutable,
          .initialized = true,
          .parameter_types = std::move(parameters),
          .parameter_labels = std::move(labels),
          .return_type = return_type,
          .span = function.span,
      });
    }
    break;
  case ast::NodeKind::protocol_declaration: {
    const auto &protocol = static_cast<const ast::ProtocolDeclaration &>(declaration);
    declare(Symbol{
        .name = protocol.name,
        .kind = Symbol::Kind::type,
        .type = Type{TypeKind::nominal_type, 0, protocol.name},
        .initialized = true,
        .parameter_types = {},
        .parameter_labels = {},
        .span = protocol.span,
    });
    break;
  }
  default:
    report("ARGOS-S0003", declaration.span,
           "declaração semântica desconhecida");
    break;
  }
}

void SemanticAnalyzer::analyze_protocol_declaration(
    const ast::ProtocolDeclaration &declaration) {
  auto &fields = nominal_fields_[declaration.name];
  for (const auto &component : declaration.components) {
    if (fields.contains(component->name)) {
      report("ARGOS-S0500", component->span,
             "componente " + quoted(component->name) + " duplicado no protocol");
      continue;
    }
    Type type = resolve_type(*component->type);
    if (type.kind == TypeKind::void_type || type.kind == TypeKind::never_type) {
      report("ARGOS-S0501", component->type->span,
             "componente de protocol exige tipo habitável");
      type = error_type();
    }
    if (component->initializer) {
      const Type value = analyze_expression(*component->initializer);
      const bool assignable = is_assignable_expression(value, type, *component->initializer);
      if (!assignable &&
          !value.is_error() && !type.is_error())
        report("ARGOS-S0502", component->initializer->span,
               "valor padrão do componente tem tipo incompatível");
      if (assignable) record_coercion(*component->initializer, value, type);
    }
    fields.emplace(component->name, type);
  }
}

void SemanticAnalyzer::analyze_variable_declaration(
    const ast::VariableDeclaration &declaration) {
  Type declared_type = declaration.type ? resolve_type(*declaration.type) : unknown_type();

  if (declared_type.kind == TypeKind::void_type ||
      declared_type.kind == TypeKind::never_type) {
    report("ARGOS-S0100", declaration.type->span,
           "uma variável não pode ter tipo Void ou Never");
  }

  if (declaration.initializer) {
    const Type initializer_type = analyze_expression(*declaration.initializer);
    if (!declaration.type) {
      if (initializer_type.is_null() || initializer_type.is_unknown()) {
        report("ARGOS-S0102", declaration.initializer->span,
               "não é possível inferir o tipo desta variável");
        declared_type = error_type();
      } else {
        declared_type = initializer_type;
        if (const auto value = integer_literal_value(*declaration.initializer);
            dynamic_cast<const ast::IntegerLiteralExpression *>(declaration.initializer.get()) &&
            (!value || *value > *integer_max(TypeKind::int_type))) {
          report("ARGOS-S0104", declaration.initializer->span,
                 "literal inteiro não cabe no tipo inferido Int");
          declared_type = error_type();
        }
      }
    }
    const bool assignable = is_assignable_expression(
        initializer_type, declared_type, *declaration.initializer);
    if (!assignable &&
        !initializer_type.is_error() && !declared_type.is_error()) {
      report("ARGOS-S0101", declaration.initializer->span,
             "não é possível inicializar " + type_name(declared_type) +
                 " com uma expressão de tipo " + type_name(initializer_type));
    }
    if (assignable) record_coercion(*declaration.initializer,
                                   initializer_type, declared_type);
  }
  if (!declaration.type && !declaration.initializer) {
    report("ARGOS-S0103", declaration.span, "variável exige tipo ou inicializador");
    declared_type = error_type();
  }

  declare(Symbol{
      .name = declaration.name,
      .kind = Symbol::Kind::variable,
      .type = declared_type,
      .mutability = declaration.mutability,
      .initialized = declaration.initializer != nullptr,
      .parameter_types = {},
      .parameter_labels = {},
      .return_type = {},
      .span = declaration.span,
  });
}

void SemanticAnalyzer::analyze_function_declaration(
    const ast::FunctionDeclaration &declaration) {
  const Symbol *function = lookup(declaration.name);
  if (function == nullptr || function->kind != Symbol::Kind::function) {
    return;
  }

  enter_scope();

  for (std::size_t i = 0; i < declaration.parameters.size(); ++i) {
    const auto &parameter = declaration.parameters[i];
    declare(Symbol{
        .name = parameter->name,
        .kind = Symbol::Kind::parameter,
        .type = i < function->parameter_types.size()
                    ? function->parameter_types[i] : error_type(),
        .mutability = ast::Mutability::immutable,
        .initialized = true,
        .parameter_types = {},
        .parameter_labels = {},
        .return_type = {},
        .span = parameter->span,
    });
  }

  functions_.push_back(FunctionContext{
      .name = declaration.name,
      .return_type = function->return_type,
      .saw_return = false,
  });

  analyze_block(*declaration.body);
  check_function_returns(declaration);

  functions_.pop_back();
  leave_scope();
}

void SemanticAnalyzer::analyze_statement(const ast::Statement &statement) {
  switch (statement.kind) {
  case ast::NodeKind::block_statement:
    analyze_block(static_cast<const ast::BlockStatement &>(statement));
    break;
  case ast::NodeKind::expression_statement:
    analyze_expression_statement(
        static_cast<const ast::ExpressionStatement &>(statement));
    break;
  case ast::NodeKind::return_statement:
    analyze_return(static_cast<const ast::ReturnStatement &>(statement));
    break;
  case ast::NodeKind::if_statement:
    analyze_if(static_cast<const ast::IfStatement &>(statement));
    break;
  case ast::NodeKind::while_statement: {
    const auto &while_stmt =
        static_cast<const ast::WhileStatement &>(statement);
    const Type condition = analyze_expression(*while_stmt.condition);
    (void)require_boolean(condition, while_stmt.condition->span,
                          "condição de 'while'");
    const auto before_loop = scopes_;
    analyze_block(*while_stmt.body);
    scopes_ = before_loop;
    break;
  }
  case ast::NodeKind::unsafe_block_statement: {
    const auto &unsafe_stmt =
        static_cast<const ast::UnsafeBlockStatement &>(statement);
    analyze_block(*unsafe_stmt.block);
    break;
  }
  default:
    report("ARGOS-S0004", statement.span, "statement semântico desconhecido");
    break;
  }
}

void SemanticAnalyzer::analyze_block(const ast::BlockStatement &block) {
  enter_scope();
  for (const auto &item : block.items) {
    analyze_block_item(*item);
  }
  leave_scope();
}

void SemanticAnalyzer::analyze_return(const ast::ReturnStatement &statement) {
  if (functions_.empty()) {
    report("ARGOS-S0200", statement.span,
           "'return' só pode aparecer dentro de uma função");
    return;
  }

  functions_.back().saw_return = true;
  const Type expected = functions_.back().return_type;
  const Type actual = statement.expression
                          ? analyze_expression(*statement.expression)
                          : Type{TypeKind::void_type, 0};

  const bool return_assignable = statement.expression
            ? is_assignable_expression(actual, expected, *statement.expression)
            : is_assignable(actual, expected);
  if (!return_assignable && !actual.is_error() &&
      !expected.is_error()) {
    report("ARGOS-S0201", statement.span,
           "retorno incompatível: função retorna " + type_name(expected) +
               ", mas a expressão possui tipo " + type_name(actual));
  }
  if (return_assignable && statement.expression)
    record_coercion(*statement.expression, actual, expected);
}

void SemanticAnalyzer::analyze_if(const ast::IfStatement &statement) {
  const Type condition = analyze_expression(*statement.condition);
  (void)require_boolean(condition, statement.condition->span,
                        "condição de 'if'");

  const auto before = scopes_;
  analyze_block(*statement.then_branch);
  const auto then_state = scopes_;
  scopes_ = before;
  if (statement.else_branch) {
    analyze_statement(*statement.else_branch);
  }
  const auto else_state = scopes_;
  scopes_ = before;
  for (std::size_t i = 0; i < scopes_.size(); ++i) {
    for (auto &[name, symbol] : scopes_[i].symbols) {
      const bool in_then = then_state[i].symbols.at(name).initialized;
      const bool in_else = else_state[i].symbols.at(name).initialized;
      symbol.initialized = symbol.initialized || (in_then && in_else);
    }
  }
}

void SemanticAnalyzer::analyze_expression_statement(
    const ast::ExpressionStatement &statement) {
  (void)analyze_expression(*statement.expression);
}

Type SemanticAnalyzer::analyze_expression(const ast::Expression &expression) {
  Type result = error_type();

  switch (expression.kind) {
  case ast::NodeKind::error_expression:
    result = error_type();
    break;
  case ast::NodeKind::identifier_expression:
    result = analyze_identifier(
        static_cast<const ast::IdentifierExpression &>(expression));
    break;
  case ast::NodeKind::integer_literal_expression:
    result = Type{TypeKind::int_type, 0};
    break;
  case ast::NodeKind::float_literal_expression:
    result = Type{TypeKind::double_type, 0};
    break;
  case ast::NodeKind::character_literal_expression:
    result = Type{TypeKind::char_type, 0};
    break;
  case ast::NodeKind::string_literal_expression:
    result = Type{TypeKind::string_type, 0};
    break;
  case ast::NodeKind::boolean_literal_expression:
    result = Type{TypeKind::bool_type, 0};
    break;
  case ast::NodeKind::null_literal_expression:
    result = Type{TypeKind::null_type, 0};
    break;
  case ast::NodeKind::grouping_expression:
    result = analyze_expression(
        *static_cast<const ast::GroupingExpression &>(expression).expression);
    break;
  case ast::NodeKind::unary_expression:
    result =
        analyze_unary(static_cast<const ast::UnaryExpression &>(expression));
    break;
  case ast::NodeKind::binary_expression:
    result =
        analyze_binary(static_cast<const ast::BinaryExpression &>(expression));
    break;
  case ast::NodeKind::assignment_expression:
    result = analyze_assignment(
        static_cast<const ast::AssignmentExpression &>(expression));
    break;
  case ast::NodeKind::call_expression:
    result = analyze_call(static_cast<const ast::CallExpression &>(expression));
    break;
  case ast::NodeKind::member_access_expression:
    result = analyze_member_access(
        static_cast<const ast::MemberAccessExpression &>(expression));
    break;
  case ast::NodeKind::postfix_expression:
    result = analyze_postfix(
        static_cast<const ast::PostfixExpression &>(expression));
    break;
  default:
    report("ARGOS-S0005", expression.span, "expressão semântica desconhecida");
    break;
  }

  expression_types_[&expression] = result;
  return result;
}

Type SemanticAnalyzer::analyze_identifier(
    const ast::IdentifierExpression &expression) {
  const Symbol *symbol = lookup(expression.name);
  if (symbol == nullptr) {
    report("ARGOS-S0300", expression.span,
           "identificador " + quoted(expression.name) + " não foi declarado");
    return error_type();
  }
  if (!analyzing_assignment_target_ &&
      symbol->kind != Symbol::Kind::function && !symbol->initialized) {
    report("ARGOS-S0301", expression.span,
           "variável " + quoted(expression.name) + " usada antes da inicialização");
  }
  expression_symbols_[&expression] = symbol->id;
  if (symbol->kind == Symbol::Kind::type) {
    report("ARGOS-S0302", expression.span,
           "nome de tipo não é um valor; use uma construção explícita");
    return error_type();
  }
  return symbol->kind == Symbol::Kind::function ? symbol->return_type
                                                : symbol->type;
}

Type SemanticAnalyzer::analyze_unary(const ast::UnaryExpression &expression) {
  const Type operand = analyze_expression(*expression.operand);
  if (operand.is_error())
    return operand;

  switch (expression.op) {
  case ast::UnaryOperator::plus:
  case ast::UnaryOperator::minus:
    if (!is_numeric(operand.unwrapped()) || operand.is_optional()) {
      report("ARGOS-S0310", expression.span,
             "operador unário aritmético exige um tipo numérico não opcional");
      return error_type();
    }
    return operand;
  case ast::UnaryOperator::not_:
    if (!is_boolean(operand)) {
      report("ARGOS-S0311", expression.span,
             "'!' lógico exige uma expressão Bool");
      return error_type();
    }
    return Type{TypeKind::bool_type, 0};
  }
  return error_type();
}

Type SemanticAnalyzer::analyze_binary(const ast::BinaryExpression &expression) {
  const Type left = analyze_expression(*expression.left);
  const Type right = analyze_expression(*expression.right);
  if (left.is_error() || right.is_error())
    return error_type();

  switch (expression.op) {
  case ast::BinaryOperator::multiply:
  case ast::BinaryOperator::divide:
  case ast::BinaryOperator::add:
  case ast::BinaryOperator::subtract:
  case ast::BinaryOperator::modulo: {
    if (!is_numeric(left) || !is_numeric(right) || left.is_optional() ||
        right.is_optional()) {
      report("ARGOS-S0320", expression.span,
             "operação aritmética exige operandos numéricos não opcionais");
      return error_type();
    }
    if (left.kind == right.kind) return left;
    if (is_assignable_expression(right, left, *expression.right)) {
      record_coercion(*expression.right, right, left);
      return left;
    }
    if (is_assignable_expression(left, right, *expression.left)) {
      record_coercion(*expression.left, left, right);
      return right;
    }
    report("ARGOS-S0321", expression.span,
           "operandos aritméticos devem possuir o mesmo tipo");
    return error_type();
  }
  case ast::BinaryOperator::less_than:
  case ast::BinaryOperator::less_than_or_equal:
  case ast::BinaryOperator::greater_than:
  case ast::BinaryOperator::greater_than_or_equal: {
    if (!is_numeric(left) || !is_numeric(right) || left.is_optional() ||
        right.is_optional()) {
      report("ARGOS-S0326", expression.span,
             "comparação relacional exige operandos numéricos não opcionais");
      return error_type();
    }
    if (left.kind != right.kind) {
      if (is_assignable_expression(right, left, *expression.right))
        record_coercion(*expression.right, right, left);
      else if (is_assignable_expression(left, right, *expression.left))
        record_coercion(*expression.left, left, right);
      else {
        report("ARGOS-S0328", expression.span,
               "comparação relacional exige tipos compatíveis");
        return error_type();
      }
    }
    return Type{TypeKind::bool_type, 0};
  }
  case ast::BinaryOperator::logical_and:
  case ast::BinaryOperator::logical_or: {
    if (!is_boolean(left) || !is_boolean(right)) {
      report("ARGOS-S0327", expression.span,
             "operadores lógicos exigem operandos Bool");
    }
    return Type{TypeKind::bool_type, 0};
  }
  case ast::BinaryOperator::equal:
  case ast::BinaryOperator::not_equal:
    if (!is_equality_comparable(left, right)) {
      report("ARGOS-S0322", expression.span,
             "operandos de igualdade possuem tipos incompatíveis");
    }
    return Type{TypeKind::bool_type, 0};
  case ast::BinaryOperator::lifted_equal:
    if (!left.is_optional() && !left.is_null() && !right.is_optional() &&
        !right.is_null()) {
      report("ARGOS-S0323", expression.span,
             "'?==' exige valores opcionais ou null");
      return error_type();
    }
    if (!is_equality_comparable(left, right)) {
      report("ARGOS-S0329", expression.span,
             "'?==' exige tipos-base compatíveis");
      return error_type();
    }
    return Type{TypeKind::bool_type, 0};
  case ast::BinaryOperator::null_coalescing:
    if (!left.is_optional()) {
      report("ARGOS-S0324", expression.span,
             "lado esquerdo de '?"
             "?' deve ser opcional");
      return error_type();
    }
    if (!is_assignable_expression(right, left.unwrapped(), *expression.right)) {
      report("ARGOS-S0325", expression.span,
             "valor padrão de '?"
             "?' não é compatível com o tipo desembrulhado");
      return error_type();
    }
    record_coercion(*expression.right, right, left.unwrapped());
    return left.unwrapped();
  }
  return error_type();
}

Type SemanticAnalyzer::analyze_assignment(
    const ast::AssignmentExpression &expression) {
  analyzing_assignment_target_ = true;
  const Type target = analyze_expression(*expression.target);
  analyzing_assignment_target_ = false;
  const Type value = analyze_expression(*expression.value);
  if (target.is_error() || value.is_error())
    return error_type();

  if (!require_lvalue(*expression.target, "lado esquerdo da atribuição")) {
    return error_type();
  }

  Symbol *target_symbol = nullptr;
  if (const auto *identifier =
          dynamic_cast<const ast::IdentifierExpression *>(expression.target.get()))
    target_symbol = lookup(identifier->name);
  if (expression.op != ast::AssignmentOperator::assign && target_symbol &&
      !target_symbol->initialized) {
    report("ARGOS-S0337", expression.target->span,
           "atribuição composta exige alvo já inicializado");
    return error_type();
  }

  if (!is_mutable_lvalue(*expression.target)) {
    report("ARGOS-S0330", expression.target->span,
           "não é possível atribuir a uma variável imutável");
    return error_type();
  }

  const auto mark_initialized = [&]() {
    if (target_symbol) target_symbol->initialized = true;
    return target;
  };

  switch (expression.op) {
  case ast::AssignmentOperator::assign:
    if (!is_assignable_expression(value, target, *expression.value)) {
      report("ARGOS-S0331", expression.span,
             "valor atribuído possui tipo incompatível");
      return error_type();
    }
    record_coercion(*expression.value, value, target);
    return mark_initialized();
  case ast::AssignmentOperator::add_assign:
  case ast::AssignmentOperator::subtract_assign:
  case ast::AssignmentOperator::multiply_assign:
  case ast::AssignmentOperator::divide_assign:
  case ast::AssignmentOperator::modulo_assign:
    if (!is_numeric(target) || !is_numeric(value) || target.is_optional() ||
        value.is_optional()) {
      report("ARGOS-S0336", expression.span,
             "atribuição aritmética exige operandos numéricos não opcionais");
      return error_type();
    }
    return mark_initialized();
  case ast::AssignmentOperator::conditional_assign:
    if (!value.is_optional()) {
      report("ARGOS-S0332", expression.span,
             "'?=' exige um valor opcional no lado direito");
      return error_type();
    }
    if (!is_assignable(value.unwrapped(), target)) {
      report("ARGOS-S0333", expression.span,
             "valor desembrulhado de '?=' não é compatível com o alvo");
      return error_type();
    }
    return mark_initialized();
  case ast::AssignmentOperator::null_coalescing_assign:
    if (!target.is_optional()) {
      report("ARGOS-S0334", expression.span,
             "'?"
             "?=' exige um alvo opcional");
      return error_type();
    }
    if (!is_assignable_expression(value, target.unwrapped(), *expression.value) &&
        !is_assignable(value, target)) {
      report("ARGOS-S0335", expression.span,
             "valor de '?"
             "?=' não é compatível com o tipo desembrulhado do alvo");
      return error_type();
    }
    record_coercion(*expression.value, value,
                    is_assignable(value, target) ? target : target.unwrapped());
    return mark_initialized();
  }
  return error_type();
}

Type SemanticAnalyzer::analyze_call(const ast::CallExpression &expression) {
  const auto *identifier =
      dynamic_cast<const ast::IdentifierExpression *>(expression.callee.get());
  if (identifier == nullptr) {
    report("ARGOS-S0340", expression.callee->span,
           "somente funções nomeadas podem ser chamadas nesta versão da "
           "linguagem");
    for (const auto &argument : expression.arguments)
      analyze_expression(*argument);
    return error_type();
  }

  const Symbol *function = lookup(identifier->name);
  if (function == nullptr || function->kind != Symbol::Kind::function) {
    report("ARGOS-S0341", expression.callee->span,
           "'" + identifier->name + "' não é uma função declarada");
    for (const auto &argument : expression.arguments) (void)analyze_expression(*argument);
    return error_type();
  }
  expression_symbols_[&expression] = function->id;
  expression_symbols_[expression.callee.get()] = function->id;
  std::vector<bool> bound(function->parameter_types.size(), false);
  std::size_t next_positional = 0;
  for (std::size_t i = 0; i < expression.arguments.size(); ++i) {
    const Type actual = analyze_expression(*expression.arguments[i]);
    const std::string label = i < expression.argument_labels.size()
                                  ? expression.argument_labels[i] : "";
    std::size_t parameter = function->parameter_types.size();
    if (label.empty()) {
      while (next_positional < bound.size() && bound[next_positional]) ++next_positional;
      if (next_positional < bound.size() &&
          function->parameter_labels[next_positional].empty()) parameter = next_positional++;
      else report("ARGOS-S0344", expression.arguments[i]->span,
                  "argumento posicional não corresponde a parâmetro sem rótulo");
    } else {
      for (std::size_t j = 0; j < function->parameter_labels.size(); ++j) {
        if (function->parameter_labels[j] == label) { parameter = j; break; }
      }
      if (parameter == bound.size())
        report("ARGOS-S0345", expression.arguments[i]->span,
               "rótulo de argumento " + quoted(label) + " inexistente");
    }
    if (parameter < bound.size()) {
      if (bound[parameter])
        report("ARGOS-S0346", expression.arguments[i]->span,
               "parâmetro recebeu argumento mais de uma vez");
      bound[parameter] = true;
      const bool assignable = is_assignable_expression(
          actual, function->parameter_types[parameter], *expression.arguments[i]);
      if (!assignable && !actual.is_error())
        report("ARGOS-S0343", expression.arguments[i]->span,
               "tipo do argumento incompatível com o parâmetro correspondente");
      if (assignable) record_coercion(*expression.arguments[i], actual,
                                     function->parameter_types[parameter]);
    }
  }
  for (std::size_t i = 0; i < bound.size(); ++i)
    if (!bound[i]) report("ARGOS-S0342", expression.span,
                          "faltou argumento para o parâmetro " +
                          quoted(function->parameter_labels[i].empty()
                                     ? std::to_string(i + 1) : function->parameter_labels[i]));

  return function->return_type;
}

Type SemanticAnalyzer::analyze_member_access(
    const ast::MemberAccessExpression &expression) {
  const Type base = analyze_expression(*expression.base);
  if (base.is_error())
    return base;

  if (expression.safe) {
    if (!base.is_optional()) {
      report("ARGOS-S0350", expression.base->span,
             "'?.' exige uma base opcional");
      return error_type();
    }
  } else if (base.is_optional()) {
    report("ARGOS-S0351", expression.base->span,
           "acesso '.' não pode ser feito diretamente sobre um tipo opcional; "
           "use '?.' ou unwrap");
    return error_type();
  }
  if (base.kind != TypeKind::nominal_type) {
    report("ARGOS-S0352", expression.span,
           "acesso a membro exige um tipo nominal com componentes");
    return error_type();
  }
  const auto nominal = nominal_fields_.find(base.nominal_name);
  if (nominal == nominal_fields_.end() ||
      !nominal->second.contains(expression.member)) {
    report("ARGOS-S0353", expression.span,
           "membro " + quoted(expression.member) + " não existe em " +
           type_name(base));
    return error_type();
  }
  Type result = nominal->second.at(expression.member);
  if (expression.safe)
    result.nullable_depth = std::max<std::size_t>(
        1, std::max(base.nullable_depth, result.nullable_depth));
  return result;
}

Type SemanticAnalyzer::analyze_postfix(
    const ast::PostfixExpression &expression) {
  const Type operand = analyze_expression(*expression.operand);
  if (operand.is_error())
    return operand;

  switch (expression.op) {
  case ast::PostfixOperator::increment:
  case ast::PostfixOperator::decrement:
    if (!require_lvalue(*expression.operand, "incremento/decremento") ||
        !is_mutable_lvalue(*expression.operand)) {
      report("ARGOS-S0363", expression.span,
             "incremento/decremento exige variável mutável");
      return error_type();
    }
    if (!is_numeric(operand) || operand.is_optional()) {
      report("ARGOS-S0362", expression.span,
             "incremento/decremento exige um tipo numérico não opcional");
      return error_type();
    }
    return operand;
  case ast::PostfixOperator::propagate:
    report("ARGOS-S0364", expression.span, "'?' postfix não faz parte da gramática atual");
    return error_type();
    if (!operand.is_optional()) {
      report("ARGOS-S0360", expression.span,
             "'?' postfix exige um tipo opcional");
      return error_type();
    }
    return operand.unwrapped();
  case ast::PostfixOperator::checked_unwrap:
    report("ARGOS-S0365", expression.span, "'!' é apenas negação lógica; use '!!'");
    return error_type();
  case ast::PostfixOperator::force_unwrap:
    if (!operand.is_optional()) {
      report("ARGOS-S0361", expression.span,
             "'!'/'!!' exigem um tipo opcional");
      return error_type();
    }
    return operand.unwrapped();
  }
  return error_type();
}

Type SemanticAnalyzer::resolve_type(const ast::TypeSyntax &syntax) {
  TypeKind kind = TypeKind::unknown;
  const std::string_view name = syntax.name;

  if (name == "Bool")
    kind = TypeKind::bool_type;
  else if (name == "Byte")
    kind = TypeKind::byte;
  else if (name == "SByte") kind = TypeKind::sbyte;
  else if (name == "Short")
    kind = TypeKind::short_type;
  else if (name == "UShort") kind = TypeKind::ushort_type;
  else if (name == "Int")
    kind = TypeKind::int_type;
  else if (name == "UInt") kind = TypeKind::uint_type;
  else if (name == "Long")
    kind = TypeKind::long_type;
  else if (name == "ULong") kind = TypeKind::ulong_type;
  else if (name == "USize")
    kind = TypeKind::usize;
  else if (name == "ISize")
    kind = TypeKind::isize;
  else if (name == "Float")
    kind = TypeKind::float_type;
  else if (name == "Double")
    kind = TypeKind::double_type;
  else if (name == "Char")
    kind = TypeKind::char_type;
  else if (name == "String") kind = TypeKind::string_type;
  else if (name == "Void")
    kind = TypeKind::void_type;
  else if (name == "Never") kind = TypeKind::never_type;
  else {
    if (const Symbol *symbol = lookup(name);
        symbol && symbol->kind == Symbol::Kind::type) {
      Type resolved = symbol->type;
      resolved.nullable_depth = syntax.nullable_depth;
      return resolved;
    }
    report("ARGOS-S0400", syntax.span, "tipo " + quoted(name) + " não foi declarado");
    return error_type();
  }

  return Type{kind, syntax.nullable_depth};
}

bool SemanticAnalyzer::is_numeric(Type type) noexcept {
  switch (type.kind) {
  case TypeKind::byte:
  case TypeKind::sbyte:
  case TypeKind::short_type:
  case TypeKind::ushort_type:
  case TypeKind::int_type:
  case TypeKind::uint_type:
  case TypeKind::long_type:
  case TypeKind::ulong_type:
  case TypeKind::usize:
  case TypeKind::isize:
  case TypeKind::float_type:
  case TypeKind::double_type:
    return true;
  default:
    return false;
  }
}

bool SemanticAnalyzer::is_integral(Type type) noexcept {
  switch (type.kind) {
  case TypeKind::byte:
  case TypeKind::sbyte:
  case TypeKind::short_type:
  case TypeKind::ushort_type:
  case TypeKind::int_type:
  case TypeKind::uint_type:
  case TypeKind::long_type:
  case TypeKind::ulong_type:
  case TypeKind::usize:
  case TypeKind::isize:
    return true;
  default:
    return false;
  }
}

bool SemanticAnalyzer::is_boolean(Type type) noexcept {
  return type.kind == TypeKind::bool_type && !type.is_optional();
}

bool SemanticAnalyzer::is_assignable_expression(
    Type from, Type to, const ast::Expression &expression) {
  if (dynamic_cast<const ast::IntegerLiteralExpression *>(&expression)) {
    if (const auto maximum = integer_max(to.kind)) {
      const auto value = integer_literal_value(expression);
      return value && *value <= *maximum && to.nullable_depth <= 1;
    }
  }
  return is_assignable(from, to);
}

bool SemanticAnalyzer::is_assignable(Type from, Type to) noexcept {
  if (from.is_error() || to.is_error()) {
    return true;
  }
  if (from.is_unknown() || to.is_unknown()) return false;

  if (from.is_null()) {
    return to.is_optional();
  }

  if (from.kind != to.kind) {
    return false;
  }
  if (from.kind == TypeKind::nominal_type &&
      from.nominal_name != to.nominal_name) return false;

  return from.nullable_depth == to.nullable_depth ||
         from.nullable_depth + 1 == to.nullable_depth;
}

bool SemanticAnalyzer::is_equality_comparable(Type left, Type right) noexcept {
  if (left.is_error() || right.is_error())
    return true;
  if (left.is_unknown() || right.is_unknown()) return false;
  if (left.is_null())
    return right.is_optional() || right.is_null();
  if (right.is_null())
    return left.is_optional();
  return left.kind == right.kind && left.nullable_depth == right.nullable_depth &&
         left.nominal_name == right.nominal_name;
}

Type SemanticAnalyzer::common_numeric_type(Type left, Type right) noexcept {
  if (left.kind == right.kind)
    return Type{left.kind, 0};
  return error_type();
}

bool SemanticAnalyzer::require_boolean(Type type,
                                       const source::SourceSpan &span,
                                       std::string_view context) {
  if (is_boolean(type) || type.is_error())
    return true;
  report("ARGOS-S0370", span, std::string(context) + " deve possuir tipo Bool");
  return false;
}

bool SemanticAnalyzer::require_lvalue(const ast::Expression &expression,
                                      std::string_view context) {
  if (dynamic_cast<const ast::IdentifierExpression *>(&expression) != nullptr ||
      dynamic_cast<const ast::MemberAccessExpression *>(&expression) !=
          nullptr) {
    return true;
  }
  report("ARGOS-S0380", expression.span,
         std::string(context) + " deve ser um l-value");
  return false;
}

bool SemanticAnalyzer::is_mutable_lvalue(
    const ast::Expression &expression) const {
  if (const auto *identifier =
          dynamic_cast<const ast::IdentifierExpression *>(&expression)) {
    const Symbol *symbol = lookup(identifier->name);
    return symbol != nullptr &&
           (symbol->mutability == ast::Mutability::mutable_ || !symbol->initialized);
  }

  return false;
}

void SemanticAnalyzer::check_function_returns(
    const ast::FunctionDeclaration &declaration) {
  const Type return_type = functions_.back().return_type;
  if (return_type.kind != TypeKind::void_type && !return_type.is_error() &&
      !block_guarantees_return(*declaration.body)) {
    report("ARGOS-S0202", declaration.span,
           "função não-Void deve retornar em todos os caminhos");
  }
}

bool SemanticAnalyzer::statement_guarantees_return(const ast::Statement &statement) const {
  switch (statement.kind) {
  case ast::NodeKind::return_statement: return true;
  case ast::NodeKind::block_statement:
    return block_guarantees_return(static_cast<const ast::BlockStatement &>(statement));
  case ast::NodeKind::if_statement: {
    const auto &branch = static_cast<const ast::IfStatement &>(statement);
    return branch.else_branch && block_guarantees_return(*branch.then_branch) &&
           statement_guarantees_return(*branch.else_branch);
  }
  default: return false;
  }
}

bool SemanticAnalyzer::block_guarantees_return(const ast::BlockStatement &block) const {
  for (const auto &item : block.items) {
    if (const auto *statement = dynamic_cast<const ast::Statement *>(item.get())) {
      if (statement_guarantees_return(*statement)) return true;
    }
  }
  return false;
}

Type SemanticAnalyzer::type_of(const ast::Expression &expression) const {
  const auto found = expression_types_.find(&expression);
  if (found == expression_types_.end())
    return unknown_type();
  return found->second;
}

std::optional<Type>
SemanticAnalyzer::coercion_of(const ast::Expression &expression) const {
  const auto found = expression_coercions_.find(&expression);
  if (found == expression_coercions_.end()) return std::nullopt;
  return found->second;
}

std::optional<SymbolId>
SemanticAnalyzer::symbol_id_of(const ast::Expression &expression) const {
  const auto found = expression_symbols_.find(&expression);
  if (found == expression_symbols_.end()) return std::nullopt;
  return found->second;
}

const Symbol *SemanticAnalyzer::symbol_by_id(SymbolId id) const noexcept {
  return id < symbol_records_.size() ? &symbol_records_[id] : nullptr;
}

} // namespace argos::semantics
