#include "argos/semantics/semantic_analyzer.hpp"

#include <utility>

namespace argos::semantics {
namespace {

Type error_type() { return Type{TypeKind::error, 0}; }
Type unknown_type() { return Type{TypeKind::unknown, 0}; }

std::string quoted(std::string_view value) {
  return "'" + std::string(value) + "'";
}

} // namespace

Type Type::unwrapped() const noexcept {
  if (nullable_depth == 0) {
    return *this;
  }
  return Type{kind, nullable_depth - 1};
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
  case TypeKind::short_type:
    return "Short";
  case TypeKind::int_type:
    return "Int";
  case TypeKind::long_type:
    return "Long";
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
  case TypeKind::null_type:
    return "null";
  case TypeKind::unknown:
    return "<unknown>";
  }
  return "<unknown>";
}

std::string type_name(const Type &type) {
  std::string result{type_kind_name(type.kind)};
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

  enter_scope();

  // Pass 1: install all declarations visible in the program scope.
  for (const auto &item : program.items) {
    if (item->kind == ast::NodeKind::function_declaration ||
        item->kind == ast::NodeKind::variable_declaration) {
      analyze_declaration(*static_cast<const ast::Declaration *>(item.get()));
    }
  }

  // Pass 2: analyze executable code and function bodies after all top-level
  // function signatures are available for calls and mutual references.
  for (const auto &item : program.items) {
    switch (item->kind) {
    case ast::NodeKind::function_declaration:
      analyze_function_declaration(
          static_cast<const ast::FunctionDeclaration &>(*item));
      break;
    case ast::NodeKind::variable_declaration:
      // Initializers were checked in pass 1.
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
  return true;
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

  current.emplace(symbol.name, std::move(symbol));
  return true;
}

void SemanticAnalyzer::report(std::string_view code,
                              const source::SourceSpan &span,
                              std::string message) {
  diagnostics_.error(std::string(code), std::move(message), span);
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
      parameters.reserve(function.parameters.size());
      for (const auto &parameter : function.parameters) {
        parameters.push_back(resolve_type(*parameter->type));
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
          .return_type = return_type,
          .span = function.span,
      });
    }
    break;
  default:
    report("ARGOS-S0003", declaration.span,
           "declaração semântica desconhecida");
    break;
  }
}

void SemanticAnalyzer::analyze_variable_declaration(
    const ast::VariableDeclaration &declaration) {
  const Type declared_type = resolve_type(*declaration.type);

  if (declared_type.kind == TypeKind::void_type) {
    report("ARGOS-S0100", declaration.type->span,
           "uma variável não pode ter tipo Void");
  }

  if (declaration.initializer) {
    const Type initializer_type = analyze_expression(*declaration.initializer);
    if (!is_assignable(initializer_type, declared_type) &&
        !initializer_type.is_error() && !declared_type.is_error()) {
      report("ARGOS-S0101", declaration.initializer->span,
             "não é possível inicializar " + type_name(declared_type) +
                 " com uma expressão de tipo " + type_name(initializer_type));
    }
  }

  declare(Symbol{
      .name = declaration.name,
      .kind = Symbol::Kind::variable,
      .type = declared_type,
      .mutability = declaration.mutability,
      .initialized = declaration.initializer != nullptr,
      .parameter_types = {},
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
        .type = resolve_type(*parameter->type),
        .mutability = ast::Mutability::immutable,
        .initialized = true,
        .parameter_types = {},
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
    analyze_block(*while_stmt.body);
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

  if (!is_assignable(actual, expected) && !actual.is_error() &&
      !expected.is_error()) {
    report("ARGOS-S0201", statement.span,
           "retorno incompatível: função retorna " + type_name(expected) +
               ", mas a expressão possui tipo " + type_name(actual));
  }
}

void SemanticAnalyzer::analyze_if(const ast::IfStatement &statement) {
  const Type condition = analyze_expression(*statement.condition);
  (void)require_boolean(condition, statement.condition->span,
                        "condição de 'if'");

  analyze_block(*statement.then_branch);
  if (statement.else_branch) {
    analyze_statement(*statement.else_branch);
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
  case ast::NodeKind::string_literal_expression:
    result = Type{TypeKind::char_type, 0};
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
    if (left.kind != right.kind) {
      report("ARGOS-S0321", expression.span,
             "operandos aritméticos devem possuir o mesmo tipo");
      return error_type();
    }
    return common_numeric_type(left, right);
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
    }
    return Type{TypeKind::bool_type, 0};
  case ast::BinaryOperator::null_coalescing:
    if (!left.is_optional()) {
      report("ARGOS-S0324", expression.span,
             "lado esquerdo de '?"
             "?' deve ser opcional");
      return error_type();
    }
    if (!is_assignable(right, left.unwrapped())) {
      report("ARGOS-S0325", expression.span,
             "valor padrão de '?"
             "?' não é compatível com o tipo desembrulhado");
      return error_type();
    }
    return left.unwrapped();
  }
  return error_type();
}

Type SemanticAnalyzer::analyze_assignment(
    const ast::AssignmentExpression &expression) {
  const Type target = analyze_expression(*expression.target);
  const Type value = analyze_expression(*expression.value);
  if (target.is_error() || value.is_error())
    return error_type();

  if (!require_lvalue(*expression.target, "lado esquerdo da atribuição")) {
    return error_type();
  }

  if (!is_mutable_lvalue(*expression.target)) {
    report("ARGOS-S0330", expression.target->span,
           "não é possível atribuir a uma variável imutável");
    return error_type();
  }

  switch (expression.op) {
  case ast::AssignmentOperator::assign:
    if (!is_assignable(value, target)) {
      report("ARGOS-S0331", expression.span,
             "valor atribuído possui tipo incompatível");
      return error_type();
    }
    return target;
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
    return target;
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
    return target;
  case ast::AssignmentOperator::null_coalescing_assign:
    if (!target.is_optional()) {
      report("ARGOS-S0334", expression.span,
             "'?"
             "?=' exige um alvo opcional");
      return error_type();
    }
    if (!is_assignable(value, target.unwrapped())) {
      report("ARGOS-S0335", expression.span,
             "valor de '?"
             "?=' não é compatível com o tipo desembrulhado do alvo");
      return error_type();
    }
    return target;
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
    return error_type();
  }

  if (expression.arguments.size() != function->parameter_types.size()) {
    report("ARGOS-S0342", expression.span,
           "quantidade incorreta de argumentos para a função '" +
               identifier->name + "'");
  }

  const std::size_t count =
      std::min(expression.arguments.size(), function->parameter_types.size());
  for (std::size_t i = 0; i < expression.arguments.size(); ++i) {
    const Type actual = analyze_expression(*expression.arguments[i]);
    if (i < count && !is_assignable(actual, function->parameter_types[i]) &&
        !actual.is_error()) {
      report("ARGOS-S0343", expression.arguments[i]->span,
             "tipo do argumento incompatível com o parâmetro correspondente");
    }
  }

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
    }
    return Type{TypeKind::unknown, 1};
  }

  if (base.is_optional()) {
    report("ARGOS-S0351", expression.base->span,
           "acesso '.' não pode ser feito diretamente sobre um tipo opcional; "
           "use '?.' ou unwrap");
  }
  return unknown_type();
}

Type SemanticAnalyzer::analyze_postfix(
    const ast::PostfixExpression &expression) {
  const Type operand = analyze_expression(*expression.operand);
  if (operand.is_error())
    return operand;

  switch (expression.op) {
  case ast::PostfixOperator::increment:
  case ast::PostfixOperator::decrement:
    if (!is_numeric(operand) || operand.is_optional()) {
      report("ARGOS-S0362", expression.span,
             "incremento/decremento exige um tipo numérico não opcional");
      return error_type();
    }
    return operand;
  case ast::PostfixOperator::propagate:
    if (!operand.is_optional()) {
      report("ARGOS-S0360", expression.span,
             "'?' postfix exige um tipo opcional");
      return error_type();
    }
    return operand.unwrapped();
  case ast::PostfixOperator::checked_unwrap:
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

Type SemanticAnalyzer::resolve_type(const ast::TypeSyntax &syntax) const {
  TypeKind kind = TypeKind::unknown;
  const std::string_view name = syntax.name;

  if (name == "Bool")
    kind = TypeKind::bool_type;
  else if (name == "Byte")
    kind = TypeKind::byte;
  else if (name == "Short")
    kind = TypeKind::short_type;
  else if (name == "Int")
    kind = TypeKind::int_type;
  else if (name == "Long")
    kind = TypeKind::long_type;
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
  else if (name == "Void")
    kind = TypeKind::void_type;
  else {
    return Type{TypeKind::unknown, syntax.nullable_depth};
  }

  return Type{kind, syntax.nullable_depth};
}

bool SemanticAnalyzer::is_numeric(Type type) noexcept {
  type = type.unwrapped();
  switch (type.kind) {
  case TypeKind::byte:
  case TypeKind::short_type:
  case TypeKind::int_type:
  case TypeKind::long_type:
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
  type = type.unwrapped();
  switch (type.kind) {
  case TypeKind::byte:
  case TypeKind::short_type:
  case TypeKind::int_type:
  case TypeKind::long_type:
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

bool SemanticAnalyzer::is_assignable(Type from, Type to) noexcept {
  if (from.is_error() || to.is_error() || from.is_unknown() ||
      to.is_unknown()) {
    return true;
  }

  if (from.is_null()) {
    return to.is_optional();
  }

  if (from.kind != to.kind) {
    return false;
  }

  return from.nullable_depth == to.nullable_depth;
}

bool SemanticAnalyzer::is_equality_comparable(Type left, Type right) noexcept {
  if (left.is_error() || right.is_error() || left.is_unknown() ||
      right.is_unknown())
    return true;
  if (left.is_null())
    return right.is_optional() || right.is_null();
  if (right.is_null())
    return left.is_optional();
  return left.kind == right.kind && left.nullable_depth == right.nullable_depth;
}

Type SemanticAnalyzer::common_numeric_type(Type left, Type right) noexcept {
  if (left.kind == right.kind)
    return Type{left.kind, 0};
  return error_type();
}

bool SemanticAnalyzer::require_boolean(Type type,
                                       const source::SourceSpan &span,
                                       std::string_view context) {
  if (is_boolean(type) || type.is_error() || type.is_unknown())
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
    return symbol != nullptr && symbol->mutability == ast::Mutability::mutable_;
  }

  return false;
}

void SemanticAnalyzer::check_function_returns(
    const ast::FunctionDeclaration &declaration) {
  const Type return_type = functions_.back().return_type;
  if (return_type.kind != TypeKind::void_type && !return_type.is_error() &&
      !functions_.back().saw_return) {
    report("ARGOS-S0202", declaration.span,
           "função não-void não possui 'return'");
  }
}

Type SemanticAnalyzer::type_of(const ast::Expression &expression) const {
  const auto found = expression_types_.find(&expression);
  if (found == expression_types_.end())
    return unknown_type();
  return found->second;
}

} // namespace argos::semantics