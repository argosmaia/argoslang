#include "argos/semantics/semantic_analyzer.hpp"
#include "argos/diagnostics/diagnostic_catalog.hpp"

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
    return "void";
  case TypeKind::bool_type:
    return "bool";
  case TypeKind::byte:
    return "byte";
  case TypeKind::sbyte: return "sbyte";
  case TypeKind::short_type:
    return "short";
  case TypeKind::ushort_type: return "ushort";
  case TypeKind::int_type:
    return "int";
  case TypeKind::uint_type: return "uint";
  case TypeKind::long_type:
    return "long";
  case TypeKind::ulong_type: return "ulong";
  case TypeKind::usize:
    return "usize";
  case TypeKind::isize:
    return "isize";
  case TypeKind::float_type:
    return "float";
  case TypeKind::double_type:
    return "double";
  case TypeKind::char_type:
    return "char";
  case TypeKind::string_type: return "string";
  case TypeKind::nominal_type: return "<nominal>";
  case TypeKind::function_type: return "function";
  case TypeKind::never_type: return "never";
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
  if (type.kind == TypeKind::function_type && !type.signature.empty()) {
    result = "(";
    for (std::size_t i = 0; i + 1 < type.signature.size(); ++i) {
      if (i) result += ", ";
      result += type_name(type.signature[i]);
    }
    result += ") -> " + type_name(type.signature.back());
  }
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
  classes_.clear();
  current_class_.clear();
  lambda_scope_boundaries_.clear();
  active_lambdas_.clear();
  lambda_captures_.clear();
  in_constructor_ = false;
  initializing_field_ = false;
  constructor_lambda_context_ = false;
  accessing_member_base_ = false;
  loop_depth_ = 0;

  enter_scope();
  for (const auto& item : program.items) {
    if (const auto* type = dynamic_cast<const ast::ClassDeclaration*>(item.get())) {
      declare(Symbol{.name = type->name, .kind = Symbol::Kind::type,
                     .type = Type{TypeKind::nominal_type, 0, type->name},
                     .initialized = true, .span = type->span});
      classes_.try_emplace(type->name, ClassInfo{type, {}, 0});
    }
  }

  for (const auto &item : program.items) {
    if (item->kind == ast::NodeKind::protocol_declaration)
      analyze_declaration(*static_cast<const ast::Declaration *>(item.get()));
  }

  for (auto& [name, info] : classes_) build_class(name);

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
    case ast::NodeKind::class_declaration:
      analyze_class(static_cast<const ast::ClassDeclaration&>(*item));
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
    case ast::NodeKind::break_statement:
    case ast::NodeKind::continue_statement:
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
  if (const auto *definition =
          diagnostics::catalog::semantic::find_definition(code)) {
    diagnostics_.emit(*definition, span, std::move(message));
    return;
  }
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
  case ast::NodeKind::break_statement:
  case ast::NodeKind::continue_statement:
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
      const auto& function = static_cast<const ast::FunctionDeclaration&>(declaration);
      if (function.modifiers.abstract_ || function.modifiers.override_ ||
          function.modifiers.virtual_ || function.modifiers.final_ || function.modifiers.static_)
        report("ARGOS-S0600", function.span, "modificador de membro fora de um tipo");
      declare(function_symbol(function));
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
  if (declaration.is_property)
    report("ARGOS-S0607", declaration.span, "propriedade só pode ser declarada como membro de um tipo");
  if (!current_class_.empty() && declaration.name == "this")
    report("ARGOS-S0600", declaration.span, "variável não pode ocultar this");
  if (declaration.modifiers.final_ || declaration.modifiers.abstract_ || declaration.modifiers.override_ ||
      declaration.modifiers.virtual_ || declaration.modifiers.static_)
    report("ARGOS-S0600", declaration.span, "modificador incompatível com variável local ou global");
  Type declared_type = declaration.type ? resolve_type(*declaration.type) : unknown_type();

  if (declared_type.kind == TypeKind::void_type ||
      declared_type.kind == TypeKind::never_type) {
    report("ARGOS-S0100", declaration.type->span,
           "uma variável não pode ter tipo void ou never");
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
        if (declared_type.kind == TypeKind::void_type || declared_type.kind == TypeKind::never_type) {
          report("ARGOS-S0100", declaration.span, "variável exige valor de tipo habitável");
          declared_type = error_type();
        }
        if (const auto value = integer_literal_value(*declaration.initializer);
            dynamic_cast<const ast::IntegerLiteralExpression *>(declaration.initializer.get()) &&
            (!value || *value > *integer_max(TypeKind::int_type))) {
          report("ARGOS-S0104", declaration.initializer->span,
                 "literal inteiro não cabe no tipo inferido int");
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

  const auto previous_loop_depth = loop_depth_;
  loop_depth_ = 0;
  if (declaration.body) {
    analyze_block(*declaration.body);
    check_function_returns(declaration);
    if (in_constructor_)
      for (const auto& [name, member] : classes_.at(current_class_).members)
        if (member.kind == Symbol::Kind::variable)
          if (const auto* field = lookup_field(name); field && !field->initialized)
            report("ARGOS-S0614", declaration.span, "construtor não inicializa campo em todos os caminhos: " + name);
  } else if (current_class_.empty() ||
             (!classes_.at(current_class_).declaration->is_interface && !function->modifiers.abstract_)) {
    report("ARGOS-S0601", declaration.span, "função concreta exige corpo");
  }
  loop_depth_ = previous_loop_depth;

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
    ++loop_depth_;
    analyze_block(*while_stmt.body);
    --loop_depth_;
    scopes_ = before_loop;
    break;
  }
  case ast::NodeKind::break_statement:
    if (loop_depth_ == 0) report(diagnostics::catalog::semantic::control::break_outside_loop.code, statement.span, "'break' só pode ser usado dentro de um laço");
    break;
  case ast::NodeKind::continue_statement:
    if (loop_depth_ == 0) report(diagnostics::catalog::semantic::control::continue_outside_loop.code, statement.span, "'continue' só pode ser usado dentro de um laço");
    break;
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
    report(diagnostics::catalog::semantic::control::return_outside_function.code, statement.span,
           "'return' só pode aparecer dentro de uma função");
    return;
  }

  if (in_constructor_)
    for (const auto& [name, member] : classes_.at(current_class_).members)
      if (member.kind == Symbol::Kind::variable)
        if (const auto* field = lookup_field(name); field && !field->initialized)
          report("ARGOS-S0614", statement.span, "return antes da inicialização do campo: " + name);
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
    report(diagnostics::catalog::semantic::control::incompatible_return.code, statement.span,
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
      if (symbol.id < symbol_records_.size())
        symbol_records_[symbol.id].initialized = symbol.initialized;
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
  case ast::NodeKind::lambda_expression:
    result = analyze_lambda(static_cast<const ast::LambdaExpression&>(expression));
    break;
  case ast::NodeKind::new_expression: {
    const auto& creation = static_cast<const ast::NewExpression&>(expression);
    result = resolve_type(*creation.type);
    const auto found = classes_.find(result.nominal_name);
    if (found == classes_.end() || found->second.declaration->is_interface ||
        found->second.declaration->modifiers.abstract_) {
      report("ARGOS-S0602", expression.span, "new exige classe concreta");
      result = error_type();
      for (const auto& arg : creation.call->arguments) analyze_expression(*arg);
      break;
    }
    const auto constructor = found->second.members.find("construct");
    Symbol implicit;
    implicit.return_type = result;
    if (constructor != found->second.members.end()) {
      if (const auto* member = find_member(result, "construct", expression.span))
        check_call(*creation.call, *member);
    } else check_call(*creation.call, implicit);
    break;
  }
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
           "variável " + quoted(expression.name) +
               " usada antes da inicialização");
  }
  if (in_constructor_ && symbol->kind == Symbol::Kind::property && !instance_ready())
    report("ARGOS-S0614", expression.span, "acessor exige instância completamente inicializada");
  if (in_constructor_ && symbol->name == "this" && !accessing_member_base_ && !instance_ready())
    report("ARGOS-S0614", expression.span, "instância não pode escapar antes da inicialização completa");
  if (initializing_field_ && (symbol->name == "this" || !symbol->owner.empty()))
    report("ARGOS-S0614", expression.span, "inicializador de campo não pode acessar a instância; use construct");
  expression_symbols_[&expression] = symbol->id;
  for (std::size_t i = 0; i < lambda_scope_boundaries_.size(); ++i) {
    bool captured = false;
    for (std::size_t scope = 0; scope < lambda_scope_boundaries_[i]; ++scope) {
      const auto found = scopes_[scope].symbols.find(symbol->name);
      if (found != scopes_[scope].symbols.end() && found->second.id == symbol->id) captured = true;
    }
    if (captured && symbol->kind != Symbol::Kind::function && symbol->kind != Symbol::Kind::type) {
      if (symbol->mutability == ast::Mutability::mutable_ || !symbol->owner.empty() ||
          (constructor_lambda_context_ && symbol->name == "this"))
        report("ARGOS-S0613", expression.span, "captura exige binding local imutável; captura mutável ainda não é suportada");
      auto& captures = lambda_captures_[active_lambdas_[i]];
      if (std::none_of(captures.begin(), captures.end(), [&](const Symbol& value) { return value.id == symbol->id; }))
        captures.push_back(*symbol);
    }
  }
  if (symbol->kind == Symbol::Kind::type) {
    report("ARGOS-S0302", expression.span,
           "nome de tipo não é um valor; use uma construção explícita");
    return error_type();
  }
  return symbol->type;
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
             "'!' lógico exige uma expressão bool");
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
    if (expression.op == ast::BinaryOperator::modulo &&
        (!is_integral(left) || !is_integral(right))) {
      report("ARGOS-S0320", expression.span,
             "módulo exige operandos inteiros");
      return error_type();
    }
    if (expression.op == ast::BinaryOperator::divide) {
      const Type double_type{TypeKind::double_type, 0};
      if (is_assignable(left, double_type))
        record_coercion(*expression.left, left, double_type);
      if (is_assignable(right, double_type))
        record_coercion(*expression.right, right, double_type);
      return double_type;
    }
    const Type common = common_numeric_type(left, right);
    if (common.is_error()) {
      report("ARGOS-S0321", expression.span,
             "operandos aritméticos devem possuir tipos numéricos compatíveis");
      return error_type();
    }
    if (left != common) record_coercion(*expression.left, left, common);
    if (right != common) record_coercion(*expression.right, right, common);
    return common;
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
             "operadores lógicos exigem operandos bool");
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
  const auto previous_assignment_target = analyzing_assignment_target_;
  analyzing_assignment_target_ = true;
  const Type target = analyze_expression(*expression.target);
  analyzing_assignment_target_ = previous_assignment_target;
  const Type value = analyze_expression(*expression.value);
  if (target.is_error() || value.is_error())
    return error_type();

  if (!require_lvalue(*expression.target, "lado esquerdo da atribuição")) {
    return error_type();
  }

  Symbol* target_symbol = assignment_symbol(*expression.target);
  if (expression.op != ast::AssignmentOperator::assign && target_symbol &&
      !target_symbol->initialized) {
    report("ARGOS-S0337", expression.target->span,
           "atribuição composta exige alvo já inicializado");
    return error_type();
  }

  if (!lambda_scope_boundaries_.empty() && target_symbol) {
    for (std::size_t i = 0; i < lambda_scope_boundaries_.back(); ++i) {
      const auto found = scopes_[i].symbols.find(target_symbol->name);
      if (found != scopes_[i].symbols.end() && found->second.id == target_symbol->id) {
        report("ARGOS-S0613", expression.span, "lambda não pode alterar binding capturado");
        return error_type();
      }
    }
  }
  if (!is_mutable_lvalue(*expression.target)) {
    report("ARGOS-S0330", expression.target->span,
           "não é possível atribuir a uma variável imutável");
    return error_type();
  }

  const auto mark_initialized = [&]() {
    if (target_symbol) {
      target_symbol->initialized = true;
      if (target_symbol->id < symbol_records_.size())
        symbol_records_[target_symbol->id].initialized = true;
    }
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
    if ((expression.op == ast::AssignmentOperator::divide_assign &&
         target.kind != TypeKind::float_type && target.kind != TypeKind::double_type) ||
        (expression.op == ast::AssignmentOperator::modulo_assign &&
         (!is_integral(target) || !is_integral(value))) ||
        !is_assignable_expression(value, target, *expression.value)) {
      report("ARGOS-S0331", expression.span, "resultado da atribuição composta não cabe no tipo do alvo");
      return error_type();
    }
    record_coercion(*expression.value, value, target);
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
  const auto* identifier = dynamic_cast<const ast::IdentifierExpression*>(expression.callee.get());
  if (identifier && identifier->name == "print") {
    for (const auto& arg : expression.arguments) analyze_expression(*arg);
    return Type{TypeKind::void_type};
  }
  // Copy signatures before analyzing arguments: nested lambdas may grow scopes.
  Symbol callable;
  if (identifier) {
    const auto* symbol = lookup(identifier->name);
    if (!symbol || (symbol->kind != Symbol::Kind::function && symbol->type.kind != TypeKind::function_type)) {
      report(diagnostics::catalog::semantic::calls::undeclared_function.code, expression.callee->span,
             "expressão não é uma função declarada ou valor chamável");
      for (const auto& arg : expression.arguments) analyze_expression(*arg);
      return error_type();
    }
    if (in_constructor_ && symbol->kind == Symbol::Kind::function &&
        !symbol->owner.empty() && !instance_ready())
      report("ARGOS-S0614", expression.span, "método exige instância completamente inicializada");
    callable = *symbol;
    expression_symbols_[&expression] = symbol->id;
    expression_symbols_[expression.callee.get()] = symbol->id;
  }
  const Type type = analyze_expression(*expression.callee);
  if (type.kind != TypeKind::function_type || type.is_optional() || type.signature.empty()) {
    report(diagnostics::catalog::semantic::calls::non_named_callee.code, expression.callee->span,
           "chamada exige valor de tipo função não opcional");
    for (const auto& arg : expression.arguments) analyze_expression(*arg);
    return error_type();
  }
  if (const auto* access = dynamic_cast<const ast::MemberAccessExpression*>(expression.callee.get())) {
    const auto base_type = type_of(*access->base);
    if (const auto* member = find_member(base_type, access->member, access->span)) callable = *member;
  }
  if (callable.kind != Symbol::Kind::function) {
    callable.parameter_types.assign(type.signature.begin(), type.signature.end() - 1);
    callable.parameter_labels.assign(callable.parameter_types.size(), "");
    callable.return_type = type.signature.back();
  }
  return check_call(expression, callable);
}

Type SemanticAnalyzer::check_call(const ast::CallExpression& expression, const Symbol& callable) {
  const Symbol* function = &callable;
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
      else report(diagnostics::catalog::semantic::calls::positional_argument_without_slot.code, expression.arguments[i]->span,
                  "argumento posicional não corresponde a parâmetro sem rótulo");
    } else {
      for (std::size_t j = 0; j < function->parameter_labels.size(); ++j) {
        if (function->parameter_labels[j] == label) { parameter = j; break; }
      }
      if (parameter == bound.size())
        report(diagnostics::catalog::semantic::calls::unknown_argument_label.code, expression.arguments[i]->span,
               "rótulo de argumento " + quoted(label) + " inexistente");
    }
    if (parameter < bound.size()) {
      if (bound[parameter])
        report(diagnostics::catalog::semantic::calls::duplicate_argument.code, expression.arguments[i]->span,
               "parâmetro recebeu argumento mais de uma vez");
      bound[parameter] = true;
      const bool assignable = is_assignable_expression(
          actual, function->parameter_types[parameter], *expression.arguments[i]);
      if (!assignable && !actual.is_error())
        report(diagnostics::catalog::semantic::calls::incompatible_argument.code, expression.arguments[i]->span,
               "tipo do argumento incompatível com o parâmetro correspondente");
      if (assignable) record_coercion(*expression.arguments[i], actual,
                                     function->parameter_types[parameter]);
    }
  }
  for (std::size_t i = 0; i < bound.size(); ++i)
    if (!bound[i]) report(diagnostics::catalog::semantic::calls::missing_argument.code, expression.span,
                          "faltou argumento para o parâmetro " +
                          quoted(function->parameter_labels[i].empty()
                                     ? std::to_string(i + 1) : function->parameter_labels[i]));

  return function->return_type;
}

Type SemanticAnalyzer::analyze_member_access(
    const ast::MemberAccessExpression &expression) {
  const bool previous_member_base = accessing_member_base_;
  const auto* direct_base = dynamic_cast<const ast::IdentifierExpression*>(expression.base.get());
  accessing_member_base_ = direct_base && direct_base->name == "this";
  const bool previous_target = analyzing_assignment_target_;
  analyzing_assignment_target_ = false;
  const Type base = analyze_expression(*expression.base);
  analyzing_assignment_target_ = previous_target;
  accessing_member_base_ = previous_member_base;
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
  if (classes_.contains(base.nominal_name)) {
    if (expression.member == "construct") {
      report("ARGOS-S0600", expression.span, "construct só pode ser invocado pela construção new");
      return error_type();
    }
    if (in_constructor_ && !analyzing_assignment_target_) {
      const auto* identifier = dynamic_cast<const ast::IdentifierExpression*>(expression.base.get());
      if (identifier && identifier->name == "this")
        if (const auto* field = lookup_field(expression.member); field && !field->initialized)
          report("ARGOS-S0301", expression.span, "campo usado antes da inicialização");
    }
    const auto* member = find_member(base, expression.member, expression.span);
    if (!member) return error_type();
    if (in_constructor_ && (member->kind == Symbol::Kind::function || member->kind == Symbol::Kind::property)) {
      const auto* identifier = dynamic_cast<const ast::IdentifierExpression*>(expression.base.get());
      if (identifier && identifier->name == "this" && !instance_ready())
        report("ARGOS-S0614", expression.span, "método exige instância completamente inicializada");
    }
    Type result = member->type;
    if (expression.safe) ++result.nullable_depth;
    return result;
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
  if (syntax.result_type) {
    Type result{TypeKind::function_type, syntax.nullable_depth};
    for (const auto& parameter : syntax.parameter_types) {
      auto type = resolve_type(*parameter);
      if (type.kind == TypeKind::void_type || type.kind == TypeKind::never_type)
        report("ARGOS-S0610", parameter->span, "parâmetro de função exige tipo habitável");
      result.signature.push_back(type);
    }
    result.signature.push_back(resolve_type(*syntax.result_type));
    return result;
  }
  TypeKind kind = TypeKind::unknown;
  const std::string_view name = syntax.name;

  if (name == "bool")
    kind = TypeKind::bool_type;
  else if (name == "byte")
    kind = TypeKind::byte;
  else if (name == "sbyte") kind = TypeKind::sbyte;
  else if (name == "short")
    kind = TypeKind::short_type;
  else if (name == "ushort") kind = TypeKind::ushort_type;
  else if (name == "int")
    kind = TypeKind::int_type;
  else if (name == "uint") kind = TypeKind::uint_type;
  else if (name == "long")
    kind = TypeKind::long_type;
  else if (name == "ulong") kind = TypeKind::ulong_type;
  else if (name == "usize")
    kind = TypeKind::usize;
  else if (name == "isize")
    kind = TypeKind::isize;
  else if (name == "float")
    kind = TypeKind::float_type;
  else if (name == "double")
    kind = TypeKind::double_type;
  else if (name == "char")
    kind = TypeKind::char_type;
  else if (name == "string") kind = TypeKind::string_type;
  else if (name == "void")
    kind = TypeKind::void_type;
  else if (name == "never") kind = TypeKind::never_type;
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
    Type from, Type to, const ast::Expression &expression) const {
  if (dynamic_cast<const ast::IntegerLiteralExpression *>(&expression)) {
    if (const auto maximum = integer_max(to.kind)) {
      const auto value = integer_literal_value(expression);
      return value && *value <= *maximum && to.nullable_depth <= 1;
    }
  }
  return is_assignable(from, to);
}

bool SemanticAnalyzer::is_assignable(Type from, Type to) const {
  if (from.is_error() || to.is_error()) {
    return true;
  }
  if (from.is_unknown() || to.is_unknown()) return false;

  if (from.is_null()) {
    return to.is_optional();
  }

  if (to.nullable_depth == 0 && from.nullable_depth == 0) {
    const auto real_type = [](TypeKind kind) {
      return kind == TypeKind::float_type || kind == TypeKind::double_type;
    };
    const auto integral_type = [](TypeKind kind) {
      return kind == TypeKind::byte || kind == TypeKind::sbyte ||
             kind == TypeKind::short_type || kind == TypeKind::ushort_type ||
             kind == TypeKind::int_type || kind == TypeKind::uint_type ||
             kind == TypeKind::long_type || kind == TypeKind::ulong_type ||
             kind == TypeKind::usize || kind == TypeKind::isize;
    };
    if ((integral_type(from.kind) && real_type(to.kind)) ||
        (real_type(from.kind) && real_type(to.kind)))
      return true;
  }

  if (from.kind != to.kind) {
    return false;
  }
  if (from.kind == TypeKind::nominal_type &&
      !is_subtype(from.nominal_name, to.nominal_name)) return false;
  if (from.kind == TypeKind::function_type && from.signature != to.signature) return false;

  return from.nullable_depth == to.nullable_depth ||
         from.nullable_depth + 1 == to.nullable_depth;
}

bool SemanticAnalyzer::is_equality_comparable(Type left, Type right) noexcept {
  if (left.kind == TypeKind::function_type || right.kind == TypeKind::function_type ||
      left.kind == TypeKind::void_type || right.kind == TypeKind::void_type) return false;
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
  const auto rank = [](TypeKind kind) {
    switch (kind) {
    case TypeKind::float_type: return 1;
    case TypeKind::double_type: return 2;
    default: return 0;
    }
  };
  if (left.kind == right.kind) return Type{left.kind, 0};
  if (!is_numeric(left) || !is_numeric(right) || left.is_optional() ||
      right.is_optional())
    return error_type();
  return rank(left.kind) >= rank(right.kind) ? Type{left.kind, 0}
                                              : Type{right.kind, 0};
}

bool SemanticAnalyzer::require_boolean(Type type,
                                       const source::SourceSpan &span,
                                       std::string_view context) {
  if (is_boolean(type) || type.is_error())
    return true;
  report("ARGOS-S0370", span, std::string(context) + " deve possuir tipo bool");
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

  if (const auto* access = dynamic_cast<const ast::MemberAccessExpression*>(&expression)) {
    if (access->safe) return false;
    const auto type = type_of(*access->base);
    const auto found = classes_.find(type.nominal_name);
    if (found == classes_.end()) return false;
    const auto member = found->second.members.find(access->member);
    if (member == found->second.members.end() ||
        (member->second.kind != Symbol::Kind::variable && member->second.kind != Symbol::Kind::property)) return false;
    if (member->second.mutability == ast::Mutability::mutable_) return true;
    const auto* base = dynamic_cast<const ast::IdentifierExpression*>(access->base.get());
    if (in_constructor_ && base && base->name == "this") {
      const auto* field = lookup_field(access->member);
      return field && !field->initialized;
    }
  }
  return false;
}

void SemanticAnalyzer::check_function_returns(
    const ast::FunctionDeclaration &declaration) {
  const Type return_type = functions_.back().return_type;
  if (return_type.kind != TypeKind::void_type && !return_type.is_error() &&
      declaration.body && !block_guarantees_return(*declaration.body)) {
    report(diagnostics::catalog::semantic::control::missing_return.code, declaration.span,
           "função não-void deve retornar em todos os caminhos");
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

bool SemanticAnalyzer::is_initialized(std::string_view name) const noexcept {
  for (auto symbol = symbol_records_.rbegin(); symbol != symbol_records_.rend();
       ++symbol) {
    if (symbol->kind == Symbol::Kind::variable && symbol->name == name)
      return symbol->initialized;
  }
  return false;
}

const std::vector<Symbol>& SemanticAnalyzer::captures_of(const ast::LambdaExpression& expression) const {
  static const std::vector<Symbol> empty;
  const auto found = lambda_captures_.find(&expression);
  return found == lambda_captures_.end() ? empty : found->second;
}

bool SemanticAnalyzer::instance_ready() const {
  if (current_class_.empty()) return true;
  for (const auto& [name, member] : classes_.at(current_class_).members)
    if (member.kind == Symbol::Kind::variable)
      if (const auto* field = lookup_field(name); field && !field->initialized) return false;
  return true;
}

Symbol* SemanticAnalyzer::lookup_field(const std::string& name) {
  return const_cast<Symbol*>(std::as_const(*this).lookup_field(name));
}
const Symbol* SemanticAnalyzer::lookup_field(const std::string& name) const {
  for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
    const auto found = scope->symbols.find(name);
    if (found != scope->symbols.end() && !found->second.owner.empty() &&
        found->second.kind == Symbol::Kind::variable) return &found->second;
  }
  return nullptr;
}

Symbol* SemanticAnalyzer::assignment_symbol(const ast::Expression& expression) {
  if (const auto* identifier = dynamic_cast<const ast::IdentifierExpression*>(&expression))
    return lookup(identifier->name);
  if (const auto* access = dynamic_cast<const ast::MemberAccessExpression*>(&expression)) {
    const auto* base = dynamic_cast<const ast::IdentifierExpression*>(access->base.get());
    if (base && base->name == "this") return lookup_field(access->member);
  }
  return nullptr;
}

Symbol SemanticAnalyzer::function_symbol(const ast::FunctionDeclaration& declaration) {
  Symbol result;
  result.name = declaration.name;
  result.kind = Symbol::Kind::function;
  result.initialized = true;
  result.span = declaration.span;
  result.modifiers = declaration.modifiers;
  result.owner = current_class_;
  result.type = Type{TypeKind::function_type};
  std::unordered_set<std::string> labels;
  for (const auto& parameter : declaration.parameters) {
    if (!current_class_.empty() && parameter->name == "this")
      report("ARGOS-S0600", parameter->span, "parâmetro não pode ocultar this");
    auto type = parameter->type ? resolve_type(*parameter->type) : error_type();
    if (type.kind == TypeKind::void_type || type.kind == TypeKind::never_type)
      report("ARGOS-S0610", parameter->span, "parâmetro exige tipo habitável");
    result.parameter_types.push_back(type);
    result.parameter_labels.push_back(parameter->external_label);
    result.type.signature.push_back(type);
    if (!parameter->external_label.empty() && !labels.insert(parameter->external_label).second)
      report(diagnostics::catalog::semantic::calls::duplicate_parameter_label.code,
             parameter->span, "rótulo de parâmetro duplicado");
  }
  result.return_type = declaration.return_type ? resolve_type(*declaration.return_type) : Type{TypeKind::void_type};
  result.type.signature.push_back(result.return_type);
  return result;
}

bool SemanticAnalyzer::is_subtype(const std::string& derived, const std::string& base) const {
  std::unordered_set<std::string> visited;
  std::vector<std::string> pending{derived};
  while (!pending.empty()) {
    auto name = pending.back();
    pending.pop_back();
    if (name == base) return true;
    if (!visited.insert(name).second) continue;
    if (const auto found = classes_.find(name); found != classes_.end())
      for (const auto& parent : found->second.declaration->bases) pending.push_back(parent);
  }
  return false;
}

void SemanticAnalyzer::build_class(const std::string& name) {
  auto& info = classes_.at(name);
  const auto& declaration = *info.declaration;
  if (info.state == 2) return;
  if (info.state == 1) {
    report("ARGOS-S0603", declaration.span, "ciclo de herança envolvendo " + name);
    return;
  }
  info.state = 1;
  if (declaration.modifiers.final_ && (declaration.modifiers.abstract_ || declaration.is_interface))
    report("ARGOS-S0600", declaration.span, "final não pode ser combinado com abstract ou interface");
  if (declaration.modifiers.override_ || declaration.modifiers.static_ || declaration.modifiers.virtual_)
    report("ARGOS-S0600", declaration.span, "modificador incompatível com declaração de tipo");
  std::size_t class_bases = 0;
  std::unordered_set<std::string> bases;
  for (const auto& base : declaration.bases) {
    if (!bases.insert(base).second) {
      report("ARGOS-S0604", declaration.span, "tipo base repetido: " + base);
      continue;
    }
    const auto parent = classes_.find(base);
    if (parent == classes_.end()) {
      report("ARGOS-S0400", declaration.span, "tipo base não declarado: " + base);
      continue;
    }
    build_class(base);
    if (!parent->second.declaration->is_interface) {
      if (declaration.is_interface || ++class_bases > 1)
        report("ARGOS-S0604", declaration.span, "herança de implementação deve ser simples; interface só estende interfaces");
      if (parent->second.declaration->modifiers.final_)
        report("ARGOS-S0605", declaration.span, "não é possível estender classe final " + base);
    }
    for (const auto& [member_name, member] : parent->second.members) {
      if (member_name.starts_with("$") || member_name == "construct" || member.modifiers.visibility == ast::Visibility::private_) continue;
      const auto previous = info.members.find(member_name);
      if (previous == info.members.end()) info.members.emplace(member_name, member);
      else if (previous->second.kind != member.kind || previous->second.type != member.type ||
               previous->second.parameter_labels != member.parameter_labels)
        report("ARGOS-S0606", declaration.span, "contratos herdados incompatíveis para " + member_name);
      else {
        const auto& existing = previous->second;
        if ((existing.modifiers.abstract_ && existing.modifiers.visibility == ast::Visibility::public_ &&
             !member.modifiers.abstract_ && member.modifiers.visibility != ast::Visibility::public_) ||
            (member.modifiers.abstract_ && member.modifiers.visibility == ast::Visibility::public_ &&
             !existing.modifiers.abstract_ && existing.modifiers.visibility != ast::Visibility::public_))
          report("ARGOS-S0606", declaration.span, "implementação herdada deve preservar visibilidade pública: " + member_name);
        if (existing.kind == Symbol::Kind::property && existing.mutability != member.mutability) {
          const bool requirement = existing.modifiers.abstract_ && member.modifiers.abstract_;
          if (requirement) previous->second.mutability = ast::Mutability::mutable_;
          else if ((existing.modifiers.abstract_ && existing.mutability == ast::Mutability::mutable_) ||
                   (member.modifiers.abstract_ && member.mutability == ast::Mutability::mutable_))
            report("ARGOS-S0606", declaration.span, "implementação herdada não oferece setter exigido: " + member_name);
        }
        if (existing.modifiers.abstract_ && !member.modifiers.abstract_) previous->second = member;
      }
    }
  }
  const auto previous_class = current_class_;
  current_class_ = name;
  std::unordered_set<std::string> local_names;
  for (const auto& member : declaration.members) {
    const auto* field = dynamic_cast<const ast::VariableDeclaration*>(member.get());
    const auto* method = dynamic_cast<const ast::FunctionDeclaration*>(member.get());
    Symbol symbol;
    if (field) {
      if (field->name == "construct" || field->name == "this")
        report("ARGOS-S0600", field->span, "campo ou propriedade não pode ocultar construct ou this");
      symbol.kind = field->is_property ? Symbol::Kind::property : Symbol::Kind::variable;
      symbol.name = field->name;
      symbol.type = field->type ? resolve_type(*field->type) : error_type();
      symbol.mutability = field->is_property && !field->setter ? ast::Mutability::immutable : field->mutability;
      symbol.modifiers = field->modifiers;
      symbol.span = field->span;
      symbol.owner = name;
      if (!field->type) report("ARGOS-S0607", field->span, "campo exige tipo explícito");
      if (symbol.type.kind == TypeKind::void_type || symbol.type.kind == TypeKind::never_type)
        report("ARGOS-S0501", field->span, "campo exige tipo habitável");
      if (declaration.is_interface && !field->is_property)
        report("ARGOS-S0607", field->span, "interface não pode armazenar estado");
      if (!field->is_property && (field->modifiers.abstract_ || field->modifiers.override_ || field->modifiers.final_ || field->modifiers.virtual_))
        report("ARGOS-S0600", field->span, "modificador de método aplicado a campo");
      if (field->is_property) {
        if (!field->getter || field->initializer)
          report("ARGOS-S0607", field->span, "propriedade exige getter e não armazena inicializador");
        if (field->setter && field->mutability != ast::Mutability::mutable_)
          report("ARGOS-S0607", field->span, "setter exige var mut");
        if (declaration.is_interface) {
          if (symbol.modifiers.visibility == ast::Visibility::private_ || symbol.modifiers.visibility == ast::Visibility::protected_)
            report("ARGOS-S0600", field->span, "propriedade de interface deve ser pública");
          symbol.modifiers.visibility = ast::Visibility::public_;
          symbol.modifiers.abstract_ = true;
        }
        if ((symbol.modifiers.virtual_ || symbol.modifiers.abstract_) && symbol.modifiers.visibility == ast::Visibility::private_)
          report("ARGOS-S0600", field->span, "propriedade private não pode ser virtual ou abstract");
        if (symbol.modifiers.final_ && (symbol.modifiers.virtual_ || symbol.modifiers.abstract_))
          report("ARGOS-S0600", field->span, "modificadores incompatíveis na propriedade");
        for (const auto* accessor : {field->getter.get(), field->setter.get()})
          if (accessor && symbol.modifiers.abstract_ && accessor->body)
            report("ARGOS-S0607", accessor->span, "requisito de acessor não pode ter corpo");
      }
    } else if (method) {
      if (method->name == "init")
        report("ARGOS-S0600", method->span, "init foi removido; declare construct(...) para construir a instância");
      symbol = function_symbol(*method);
      if (declaration.is_interface) {
        if (symbol.modifiers.visibility == ast::Visibility::private_ ||
            symbol.modifiers.visibility == ast::Visibility::protected_ ||
            symbol.modifiers.final_ || symbol.modifiers.override_ || symbol.modifiers.virtual_)
          report("ARGOS-S0600", method->span, "modificador incompatível com requisito de interface");
        symbol.modifiers.abstract_ = true;
        symbol.modifiers.visibility = ast::Visibility::public_;
        if (method->body) report("ARGOS-S0607", method->span, "método de interface deve declarar somente assinatura");
      }
      if ((symbol.modifiers.virtual_ || symbol.modifiers.abstract_) &&
          symbol.modifiers.visibility == ast::Visibility::private_)
        report("ARGOS-S0600", method->span, "método private não pode ser virtual ou abstract");
      if (symbol.modifiers.final_ && symbol.modifiers.virtual_)
        report("ARGOS-S0600", method->span, "final e virtual são incompatíveis");
      if (symbol.modifiers.abstract_ && (symbol.modifiers.final_ || method->body))
        report("ARGOS-S0600", method->span, "método abstract não pode ser final nem ter corpo");
      if (method->name == "construct" && (method->return_type || !method->body || declaration.is_interface ||
          method->modifiers.abstract_ || method->modifiers.override_ || method->modifiers.static_))
        report("ARGOS-S0600", method->span, "construct exige corpo, sem retorno explícito e sem modificadores de despacho");
    } else continue;
    if (symbol.modifiers.static_)
      report("ARGOS-S0600", member->span, "membros static ainda não são suportados");
    if (!local_names.insert(symbol.name).second) {
      report("ARGOS-S0001", member->span, "membro duplicado: " + symbol.name);
      continue;
    }
    if (const auto inherited = info.members.find(symbol.name); inherited != info.members.end()) {
      const auto& parent = inherited->second;
      if ((field && !field->is_property) || parent.kind != symbol.kind ||
          (parent.kind != Symbol::Kind::function && parent.kind != Symbol::Kind::property))
        report("ARGOS-S0606", member->span, "membros herdados incompatíveis; campos não podem ser sobrescritos");
      else {
        const bool requirement = classes_.at(parent.owner).declaration->is_interface;
        if (!symbol.modifiers.override_ && !requirement)
          report("ARGOS-S0608", member->span, "sobrescrita exige override");
        if (parent.modifiers.final_ || (!requirement && !parent.modifiers.virtual_ &&
            !parent.modifiers.abstract_ && !parent.modifiers.override_))
          report("ARGOS-S0608", member->span, "método herdado não permite sobrescrita");
        if (parent.kind == Symbol::Kind::property && parent.mutability == ast::Mutability::mutable_ &&
            symbol.mutability != ast::Mutability::mutable_)
          report("ARGOS-S0606", member->span, "implementação deve preservar setter do contrato");
        if (parent.type != symbol.type || parent.parameter_labels != symbol.parameter_labels)
          report("ARGOS-S0606", member->span, "assinatura ou rótulos incompatíveis na sobrescrita");
        if (parent.modifiers.visibility == ast::Visibility::public_ &&
            symbol.modifiers.visibility != ast::Visibility::public_)
          report("ARGOS-S0608", member->span, "implementação não pode reduzir visibilidade pública");
      }
    } else if (symbol.modifiers.override_)
      report("ARGOS-S0608", member->span, "override sem método herdado correspondente");
    info.members.insert_or_assign(symbol.name, symbol);
    if (field && field->is_property) {
      for (const auto* accessor : {field->getter.get(), field->setter.get()}) {
        if (!accessor) continue;
        auto helper = function_symbol(*accessor);
        helper.modifiers = symbol.modifiers;
        info.members.insert_or_assign(helper.name, std::move(helper));
      }
    }
  }
  if (!declaration.is_interface && !declaration.modifiers.abstract_)
    for (const auto& [member_name, member] : info.members)
      if (!member_name.starts_with("$") && member.modifiers.abstract_)
        report("ARGOS-S0609", declaration.span, "classe concreta não implementa " + member_name);
  current_class_ = previous_class;
  info.state = 2;
}

const Symbol* SemanticAnalyzer::find_member(const Type& type, const std::string& name,
                                           const source::SourceSpan& span) {
  const auto found = classes_.find(type.nominal_name);
  if (found == classes_.end()) return nullptr;
  const auto member = found->second.members.find(name);
  if (member == found->second.members.end()) {
    report("ARGOS-S0353", span, "membro não existe: " + name);
    return nullptr;
  }
  const auto& symbol = member->second;
  if ((symbol.modifiers.visibility == ast::Visibility::private_ && current_class_ != symbol.owner) ||
      (symbol.modifiers.visibility == ast::Visibility::protected_ &&
       (current_class_.empty() || !is_subtype(current_class_, symbol.owner)))) {
    report("ARGOS-S0611", span, "membro inacessível: " + name);
    return nullptr;
  }
  return &symbol;
}

void SemanticAnalyzer::analyze_class(const ast::ClassDeclaration& declaration) {
  const auto previous_class = current_class_;
  current_class_ = declaration.name;
  enter_scope();
  declare(Symbol{.name = "this", .kind = Symbol::Kind::parameter,
                 .type = Type{TypeKind::nominal_type, 0, declaration.name}, .initialized = true});
  for (const auto& [name, member] : classes_.at(declaration.name).members) {
    auto symbol = member;
    symbol.initialized = true;
    declare(std::move(symbol));
  }
  for (const auto& member : declaration.members) {
    if (const auto* field = dynamic_cast<const ast::VariableDeclaration*>(member.get())) {
      if (field->is_property) {
        const auto before_accessor = scopes_;
        for (const auto* accessor : {field->getter.get(), field->setter.get()}) {
          if (accessor) analyze_function_declaration(*accessor);
          scopes_ = before_accessor;
        }
        continue;
      }
      if (!field->initializer && !classes_.at(declaration.name).members.contains("construct"))
        report("ARGOS-S0614", field->span, "campo sem inicializador exige construct");
      if (field->initializer) {
        initializing_field_ = true;
        const auto actual = analyze_expression(*field->initializer);
        initializing_field_ = false;
        const auto expected = classes_.at(declaration.name).members.at(field->name).type;
        if (!is_assignable_expression(actual, expected, *field->initializer))
          report("ARGOS-S0101", field->span, "inicializador de campo incompatível");
        else record_coercion(*field->initializer, actual, expected);
      }
    } else if (const auto* method = dynamic_cast<const ast::FunctionDeclaration*>(member.get())) {
      const auto before_method = scopes_;
      // Initialization facts are local to each constructor or method.
      if (method->name == "construct") {
        for (const auto& field_member : declaration.members)
          if (const auto* constructor_field = dynamic_cast<const ast::VariableDeclaration*>(field_member.get());
              constructor_field && !constructor_field->is_property)
            if (auto* symbol = lookup_field(constructor_field->name)) symbol->initialized = constructor_field->initializer != nullptr;
      }
      const bool previous_constructor = in_constructor_;
      in_constructor_ = method->name == "construct";
      analyze_function_declaration(*method);
      in_constructor_ = previous_constructor;
      // Constructor bodies use the regular definite-initialization analysis.
      scopes_ = before_method;
    }
  }
  leave_scope();
  current_class_ = previous_class;
}

Type SemanticAnalyzer::analyze_lambda(const ast::LambdaExpression& expression) {
  const auto before = scopes_;
  const auto previous_loop_depth = loop_depth_;
  const bool previous_constructor = in_constructor_;
  const bool previous_constructor_lambda = constructor_lambda_context_;
  constructor_lambda_context_ = constructor_lambda_context_ || in_constructor_;
  in_constructor_ = false;
  loop_depth_ = 0;
  lambda_scope_boundaries_.push_back(scopes_.size());
  active_lambdas_.push_back(&expression);
  enter_scope();
  Type result{TypeKind::function_type};
  for (const auto& parameter : expression.parameters) {
    if (!current_class_.empty() && parameter->name == "this")
      report("ARGOS-S0600", parameter->span, "parâmetro não pode ocultar this");
    Type type = parameter->type ? resolve_type(*parameter->type) : error_type();
    if (!parameter->type)
      report("ARGOS-S0612", parameter->span, "parâmetro de lambda exige tipo explícito nesta versão");
    if (type.kind == TypeKind::void_type || type.kind == TypeKind::never_type)
      report("ARGOS-S0610", parameter->span, "parâmetro de lambda exige tipo habitável");
    result.signature.push_back(type);
    declare(Symbol{.name = parameter->name, .kind = Symbol::Kind::parameter,
                   .type = type, .initialized = true, .span = parameter->span});
  }
  Type returned = expression.return_type ? resolve_type(*expression.return_type) : Type{TypeKind::void_type};
  if (expression.expression_body) {
    Type actual = analyze_expression(*expression.expression_body);
    if (!expression.return_type) returned = actual;
    else if (!is_assignable_expression(actual, returned, *expression.expression_body))
      report("ARGOS-S0201", expression.span, "retorno da lambda incompatível");
    else record_coercion(*expression.expression_body, actual, returned);
  } else {
    functions_.push_back(FunctionContext{"<lambda>", returned, false});
    analyze_block(*expression.body);
    if (returned.kind != TypeKind::void_type && !block_guarantees_return(*expression.body))
      report("ARGOS-S0202", expression.span, "lambda não-void deve retornar em todos os caminhos");
    functions_.pop_back();
  }
  if (returned.is_null()) {
    report("ARGOS-S0612", expression.span, "retorno nil da lambda exige tipo opcional explícito");
    returned = error_type();
  }
  result.signature.push_back(returned);
  scopes_ = before;
  loop_depth_ = previous_loop_depth;
  in_constructor_ = previous_constructor;
  constructor_lambda_context_ = previous_constructor_lambda;
  lambda_scope_boundaries_.pop_back();
  active_lambdas_.pop_back();
  return result;
}

} // namespace argos::semantics
