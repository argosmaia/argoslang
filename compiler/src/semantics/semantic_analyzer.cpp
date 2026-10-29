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
std::string component_alias(const std::string &);
std::string component_field(const std::string &, const std::string &);

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
  auto result = *this;
  --result.nullable_depth;
  return result;
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
  case TypeKind::array_type:
    return "array";
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
  if (type.kind == TypeKind::array_type && !type.signature.empty())
    result = "[" + type_name(type.signature.front()) + "]";
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
  classes_.clear();
  current_class_.clear();
  lambda_scope_boundaries_.clear();
  active_lambdas_.clear();
  lambda_captures_.clear();
  declaration_symbols_.clear();
  resolved_calls_.clear();
  global_overloads_.clear();
  super_initialized_ = true;
  calling_super_ = false;
  resolving_callee_ = false;
  in_constructor_ = false;
  initializing_field_ = false;
  constructor_lambda_context_ = false;
  accessing_member_base_ = false;
  loop_depth_ = 0;

  enter_scope();
  class_order_.clear();
  component_bindings_.clear();
  current_package_ = program.package_name;
  const auto collect = [&](auto &&self,
                           const ast::ClassDeclaration &type) -> void {
    declare(Symbol{.name = type.name,
                   .kind = Symbol::Kind::type,
                   .type = Type{TypeKind::nominal_type, 0, type.name},
                   .initialized = true,
                   .span = type.span});
    ClassInfo info;
    info.declaration = &type;
    classes_.try_emplace(type.name, std::move(info));
    class_order_.push_back(&type);
    for (const auto &member : type.members)
      if (const auto *nested =
              dynamic_cast<const ast::ClassDeclaration *>(member.get()))
        self(self, *nested);
  };
  for (const auto &item : program.items)
    if (const auto *type =
            dynamic_cast<const ast::ClassDeclaration *>(item.get()))
      collect(collect, *type);
  for (const auto *type : class_order_) {
    auto &info = classes_.at(type->name);
    for (const auto &name : type->bases)
      info.bases.push_back(
          resolve_nominal(name, type->name, type->package_name));
    for (const auto &name : type->interfaces)
      info.interfaces.push_back(
          resolve_nominal(name, type->name, type->package_name));
    for (const auto &name : type->permited)
      info.permissions.push_back(
          resolve_nominal(name, type->name, type->package_name));
    for (const auto &name : type->components)
      info.components.push_back(
          resolve_nominal(name, type->name, type->package_name));
  }
  for (const auto *type : class_order_)
    build_class(type->name);
  for (const auto *type : class_order_) {
    if (type->enclosing_type.empty() &&
        (type->modifiers.visibility == ast::Visibility::private_ ||
         type->modifiers.visibility == ast::Visibility::protected_))
      report("ARGOS-S0629", type->span,
             "private/protected class só é válida como classe aninhada");
    if (type->modifiers.sealed_ && !type->is_data) {
      const auto &info = classes_.at(type->name);
      if (info.permissions.empty())
        report("ARGOS-S0624", type->span,
               "sealed exige uma lista permited explícita e não vazia");
      const auto saved_class = current_class_;
      const auto saved_package = current_package_;
      current_class_ = type->name;
      current_package_ = type->package_name;
      for (const auto &name : info.permissions)
        if (classes_.contains(name))
          accessible_type(name, type->span);
      current_class_ = saved_class;
      current_package_ = saved_package;
      for (const auto &name : info.permissions)
        if (classes_.contains(name) &&
            std::find(classes_.at(name).bases.begin(),
                      classes_.at(name).bases.end(),
                      type->name) == classes_.at(name).bases.end())
          report("ARGOS-S0624", type->span,
                 "entidade permited deve herdar/implementar diretamente o tipo "
                 "sealed: " +
                     name);
    }
  }

  // Install function signatures before analyzing calls and executable code.
  for (const auto &item : program.items) {
    if (item->kind == ast::NodeKind::function_declaration) {
      current_package_ =
          static_cast<const ast::Declaration *>(item.get())->package_name;
      analyze_declaration(*static_cast<const ast::Declaration *>(item.get()));
    }
  }

  for (const auto *type : class_order_)
    analyze_class(*type);
  // Variables and statements retain source order for definite initialization.
  for (const auto &item : program.items) {
    current_package_ = item->package_name;
    switch (item->kind) {
    case ast::NodeKind::function_declaration:
      analyze_function_declaration(
          static_cast<const ast::FunctionDeclaration &>(*item));
      break;
    case ast::NodeKind::class_declaration:
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

void SemanticAnalyzer::restore_initialization(
    const std::vector<Scope> &before) {
  for (std::size_t i = 0; i < before.size(); ++i)
    for (const auto &[name, previous] : before[i].symbols) {
      auto &symbol = scopes_[i].symbols.at(name);
      symbol.initialized = previous.initialized;
      if (symbol.id < symbol_records_.size())
        symbol_records_[symbol.id].initialized = symbol.initialized;
    }
}

void SemanticAnalyzer::leave_scope() {
  if (!scopes_.empty()) {
    scopes_.pop_back();
  }
}

Symbol *SemanticAnalyzer::lookup(std::string_view name) {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    if (&*it == &scopes_.front() && !current_package_.empty()) {
      const auto scoped =
          it->symbols.find(current_package_ + "." + std::string{name});
      if (scoped != it->symbols.end())
        return &scoped->second;
    }
    const auto found = it->symbols.find(std::string(name));
    if (found != it->symbols.end()) {
      return &found->second;
    }
  }
  return nullptr;
}

const Symbol *SemanticAnalyzer::lookup(std::string_view name) const {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    if (&*it == &scopes_.front() && !current_package_.empty()) {
      const auto scoped =
          it->symbols.find(current_package_ + "." + std::string{name});
      if (scoped != it->symbols.end())
        return &scoped->second;
    }
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
          function.modifiers.virtual_ || function.modifiers.final_ ||
          function.modifiers.static_ || function.modifiers.sealed_)
        report("ARGOS-S0600", function.span, "modificador de membro fora de um tipo");
      auto symbol = function_symbol(function);
      auto &overloads = global_overloads_[symbol.name];
      for (const auto &prior : overloads)
        if (dispatch_key(prior) == dispatch_key(symbol))
          report("ARGOS-S0001", function.span,
                 "assinatura duplicada: " + function.name);
      if (!overloads.empty() && symbol.name == "main")
        report("ARGOS-S0620", function.span,
               "main deve possuir uma única assinatura de entrada");
      if (!overloads.empty() && !symbol.modifiers.overload_)
        report("ARGOS-S0620", function.span,
               "sobrecarga adicional exige overload");
      declaration_symbols_[&function] = symbol;
      if (overloads.empty())
        declare(symbol);
      overloads.push_back(symbol);
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
  if (declaration.is_property)
    report("ARGOS-S0607", declaration.span, "propriedade só pode ser declarada como membro de um tipo");
  if (!current_class_.empty())
    for (const auto &component : classes_.at(current_class_).components)
      if (declaration.name == component_alias(component))
        report("ARGOS-S0626", declaration.span,
               "variável não pode ocultar qualificador with: " +
                   declaration.name);
  if (!current_class_.empty() && declaration.name == "this")
    report("ARGOS-S0600", declaration.span, "variável não pode ocultar this");
  if (declaration.modifiers.final_ || declaration.modifiers.abstract_ ||
      declaration.modifiers.override_ || declaration.modifiers.overload_ ||
      declaration.modifiers.virtual_ || declaration.modifiers.static_ ||
      declaration.modifiers.sealed_)
    report("ARGOS-S0600", declaration.span, "modificador incompatível com variável local ou global");
  Type declared_type = declaration.type ? resolve_type(*declaration.type) : unknown_type();

  if (declared_type.kind == TypeKind::void_type ||
      declared_type.kind == TypeKind::never_type) {
    report("ARGOS-S0100", declaration.type->span,
           "uma variável não pode ter tipo void ou never");
  }

  if (declaration.initializer) {
    if (declared_type.kind == TypeKind::array_type &&
        dynamic_cast<const ast::ArrayExpression *>(
            declaration.initializer.get()))
      expression_coercions_[declaration.initializer.get()] = declared_type;
    const Type initializer_type =
        analyze_expression(*declaration.initializer, declared_type);
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
  if (declaration.builtin != ast::FunctionDeclaration::Builtin::none)
    return;
  const auto declared = declaration_symbols_.find(&declaration);
  const Symbol *function = declared != declaration_symbols_.end()
                               ? &declared->second
                               : lookup(declaration.name);
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
                          ? analyze_expression(*statement.expression, expected)
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

Type SemanticAnalyzer::analyze_expression(const ast::Expression &expression,
                                          std::optional<Type> expected) {
  if (expected && expected->kind == TypeKind::array_type &&
      dynamic_cast<const ast::ArrayExpression *>(&expression))
    expression_coercions_[&expression] = *expected;
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
      resolve_overload(*creation.call, found->second.methods.at("construct"));
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
        *static_cast<const ast::GroupingExpression &>(expression).expression,
        expected);
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
  case ast::NodeKind::array_expression: {
    const auto &array = static_cast<const ast::ArrayExpression &>(expression);
    const auto context = coercion_of(expression);
    Type element = context && context->kind == TypeKind::array_type &&
                           !context->signature.empty()
                       ? context->signature.front()
                       : unknown_type();
    for (const auto &item : array.elements) {
      const auto type = analyze_expression(*item);
      if (element.is_unknown() && !type.is_null())
        element = type;
    }
    if (!element.is_unknown() && !element.is_optional())
      for (const auto &item : array.elements)
        if (type_of(*item).is_null() && !context) {
          ++element.nullable_depth;
          break;
        }
    if (element.is_unknown() || element.kind == TypeKind::void_type ||
        element.kind == TypeKind::array_type) {
      report("ARGOS-S0377", expression.span,
             "array exige elementos homogêneos habitáveis; array vazio ou "
             "somente nil exige contexto tipado");
      result = error_type();
      break;
    }
    for (const auto &item : array.elements) {
      const auto type = type_of(*item);
      if (!is_assignable_expression(type, element, *item))
        report("ARGOS-S0378", item->span,
               "elemento incompatível com tipo do array");
      else
        record_coercion(*item, type, element);
    }
    result = Type{TypeKind::array_type};
    result.signature.push_back(element);
    break;
  }
  case ast::NodeKind::index_expression: {
    const auto &index = static_cast<const ast::IndexExpression &>(expression);
    const bool target_context = analyzing_assignment_target_;
    analyzing_assignment_target_ = false;
    const Type base = analyze_expression(*index.base);
    const auto before_index = scopes_;
    const Type offset = analyze_expression(*index.index);
    if (index.safe)
      restore_initialization(before_index);
    analyzing_assignment_target_ = target_context;
    if (base.kind != TypeKind::array_type || base.signature.empty() ||
        base.nullable_depth != (index.safe ? 1u : 0u)) {
      report("ARGOS-S0375", expression.span,
             "indexação exige array; '?[' exige array opcional e '[' exige "
             "array não opcional");
      result = error_type();
    } else if (offset.kind != TypeKind::int_type || offset.is_optional()) {
      report("ARGOS-S0376", expression.span, "índice exige int não opcional");
      result = error_type();
    } else {
      result = base.signature.front();
      if (index.safe && !result.is_optional())
        ++result.nullable_depth;
    }
    break;
  }
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

  if (result.nullable_depth > 4095) {
    report("ARGOS-S0401", expression.span,
           "profundidade opcional excede 4095 camadas");
    result = error_type();
  }
  expression_types_[&expression] = result;
  return result;
}

Type SemanticAnalyzer::analyze_identifier(
    const ast::IdentifierExpression &expression) {
  if (expression.name == "super") {
    if (!active_lambdas_.empty())
      report("ARGOS-S0622", expression.span,
             "super só pode ser acessado diretamente no corpo do método");
    const auto base = superclass(current_class_);
    if (base.empty()) {
      report("ARGOS-S0622", expression.span, "super exige superclasse");
      return error_type();
    }
    if (!accessing_member_base_)
      report("ARGOS-S0622", expression.span,
             "super só pode ser usado como chamada ou acesso a membro");
    return Type{TypeKind::nominal_type, 0, base};
  }
  if (expression.name.starts_with("$with.")) {
    report("ARGOS-S0631", expression.span,
           "armazenamento interno de componente não pertence à API");
    return error_type();
  }
  if (!current_class_.empty())
    for (const auto &component : classes_.at(current_class_).components) {
      if (expression.name != component_alias(component) &&
          expression.name != component)
        continue;
      if (expression.name != component &&
          std::count_if(classes_.at(current_class_).components.begin(),
                        classes_.at(current_class_).components.end(),
                        [&](const std::string &candidate) {
                          return component_alias(candidate) == expression.name;
                        }) > 1)
        report("ARGOS-S0626", expression.span,
               "qualificador de componente ambíguo; use o nome completo");
      if (calling_super_)
        report("ARGOS-S0622", expression.span,
               "argumentos de super não podem acessar componentes de this");
      if (in_constructor_ && !analyzing_assignment_target_)
        for (const auto &pending : classes_.at(current_class_).fields)
          if (const auto *field = lookup_field(pending->name);
              field && !field->initialized)
            report(
                "ARGOS-S0628", expression.span,
                "inicialize todos os componentes antes de chamadas delegadas");
      const auto field = component_field(current_class_, component);
      component_bindings_[&expression] = {current_class_, field};
      if (!accessing_member_base_ && !analyzing_assignment_target_)
        report("ARGOS-S0631", expression.span,
               "componente with só pode ser usado como receptor qualificado ou "
               "alvo de inicialização");
      if (!analyzing_assignment_target_)
        if (const auto *stored = lookup_field(field);
            stored && !stored->initialized)
          report("ARGOS-S0628", expression.span,
                 "componente usado antes da inicialização: " + component);
      capture_receiver(expression.span);
      return Type{TypeKind::nominal_type, 0, component};
    }
  const Symbol *symbol = lookup(expression.name);
  if (symbol == nullptr) {
    report("ARGOS-S0300", expression.span,
           "identificador " + quoted(expression.name) + " não foi declarado");
    return error_type();
  }
  if ((!symbol->owner.empty() || symbol->kind == Symbol::Kind::function) &&
      !(resolving_callee_ && symbol->kind == Symbol::Kind::function) &&
      !accessible(*symbol, expression.span))
    return error_type();
  if (calling_super_ && (symbol->name == "this" || !symbol->owner.empty()))
    report("ARGOS-S0622", expression.span,
           "argumentos de super não podem observar this ou seus membros");
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
  if (symbol->kind == Symbol::Kind::function && !symbol->owner.empty() &&
      !active_lambdas_.empty()) {
    if (constructor_lambda_context_)
      report("ARGOS-S0613", expression.span,
             "closure não pode capturar instância em construção");
    if (const auto *receiver = lookup("this"))
      for (const auto *lambda : active_lambdas_) {
        auto &captures = lambda_captures_[lambda];
        if (std::none_of(captures.begin(), captures.end(),
                         [&](const Symbol &capture) {
                           return capture.id == receiver->id;
                         }))
          captures.push_back(*receiver);
      }
  }
  if (!resolving_callee_ && symbol->kind == Symbol::Kind::function) {
    const auto count =
        symbol->owner.empty()
            ? global_overloads_[symbol->name].size()
            : classes_.at(current_class_).methods[symbol->name].size();
    if (count > 1)
      report("ARGOS-S0621", expression.span,
             "referência a função sobrecarregada exige chamada explícita ou "
             "wrapper com assinatura única");
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
  const auto before_right = scopes_;
  const Type right = analyze_expression(
      *expression.right, expression.op == ast::BinaryOperator::null_coalescing
                             ? std::optional<Type>{left.unwrapped()}
                             : std::nullopt);
  if (expression.op == ast::BinaryOperator::null_coalescing ||
      expression.op == ast::BinaryOperator::logical_and ||
      expression.op == ast::BinaryOperator::logical_or)
    restore_initialization(before_right);
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
  case ast::BinaryOperator::lifted_equal: {
    if (!left.is_optional() && !left.is_null() && !right.is_optional() &&
        !right.is_null()) {
      report("ARGOS-S0323", expression.span,
             "'?==' exige valores opcionais ou nil");
      return error_type();
    }
    auto l = left;
    auto r = right;
    if (!l.is_null() && !r.is_null() && l.nullable_depth != r.nullable_depth) {
      if (l.nullable_depth + 1 == r.nullable_depth) {
        l.nullable_depth = r.nullable_depth;
        record_coercion(*expression.left, left, l);
      } else if (r.nullable_depth + 1 == l.nullable_depth) {
        r.nullable_depth = l.nullable_depth;
        record_coercion(*expression.right, right, r);
      }
    }
    if (!is_equality_comparable(l, r)) {
      report("ARGOS-S0329", expression.span,
             "'?==' exige tipos-base compatíveis");
      return error_type();
    }
    return Type{TypeKind::bool_type, 0};
  }
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
  const auto before_value = scopes_;
  const Type value = analyze_expression(*expression.value, target);
  if (expression.op == ast::AssignmentOperator::null_coalescing_assign)
    restore_initialization(before_value);
  const ast::Expression *fresh = expression.value.get();
  while (const auto *group =
             dynamic_cast<const ast::GroupingExpression *>(fresh))
    fresh = group->expression.get();
  if (component_binding(*expression.target) &&
      (expression.op != ast::AssignmentOperator::assign ||
       !dynamic_cast<const ast::NewExpression *>(fresh)))
    report("ARGOS-S0628", expression.span,
           "componente exclusivo deve ser inicializado uma vez com new dentro "
           "de construct");
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
    return component_binding(*expression.target) ? Type{TypeKind::void_type}
                                                 : target;
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
  case ast::AssignmentOperator::force_unwrap_assign:
    if (!value.is_optional()) {
      report("ARGOS-S0338", expression.span, "'!!=' exige fonte opcional");
      return error_type();
    }
    if (!is_assignable(value.unwrapped(), target)) {
      report("ARGOS-S0339", expression.span,
             "valor desembrulhado de '!!=' incompatível com alvo");
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

Type SemanticAnalyzer::resolve_overload(const ast::CallExpression &call,
                                        const std::vector<Symbol> &candidates) {
  if (candidates.empty())
    return error_type();
  const auto selected = [&](const Symbol &method, bool analyzed) {
    accessible(method, call.span);
    resolved_calls_[&call] = method;
    auto callee_type = method.type;
    if (const auto *member = dynamic_cast<const ast::MemberAccessExpression *>(
            call.callee.get());
        member && member->safe)
      ++callee_type.nullable_depth;
    expression_types_[call.callee.get()] = callee_type;
    return check_call(call, method, analyzed);
  };
  if (candidates.size() == 1)
    return selected(candidates.front(), false);

  struct Candidate {
    const Symbol *method;
    std::vector<std::size_t> parameters;
  };
  std::vector<Candidate> shaped;
  for (const auto &candidate : candidates) {
    if (candidate.parameter_types.size() != call.arguments.size())
      continue;
    Candidate binding{&candidate, {}};
    std::vector<bool> used(call.arguments.size());
    std::size_t next = 0;
    for (std::size_t i = 0; i < call.arguments.size(); ++i) {
      const auto label =
          i < call.argument_labels.size() ? call.argument_labels[i] : "";
      std::size_t at = used.size();
      if (label.empty()) {
        while (next < used.size() && used[next])
          ++next;
        if (next < used.size() && candidate.parameter_labels[next].empty())
          at = next++;
      } else
        for (std::size_t j = 0; j < used.size(); ++j)
          if (candidate.parameter_labels[j] == label) {
            at = j;
            break;
          }
      if (at == used.size() || used[at])
        break;
      used[at] = true;
      binding.parameters.push_back(at);
    }
    if (binding.parameters.size() == call.arguments.size())
      shaped.push_back(std::move(binding));
  }
  // Unique arity/labels supply context for nil, empty arrays and numeric
  // literals.
  if (shaped.size() == 1)
    return selected(*shaped.front().method, false);
  for (const auto &arg : call.arguments)
    analyze_expression(*arg);
  std::vector<Candidate> viable;
  for (const auto &candidate : shaped) {
    bool valid = true;
    for (std::size_t i = 0; i < call.arguments.size(); ++i)
      if (!is_assignable_expression(
              type_of(*call.arguments[i]),
              candidate.method->parameter_types[candidate.parameters[i]],
              *call.arguments[i])) {
        valid = false;
        break;
      }
    if (valid)
      viable.push_back(candidate);
  }
  const auto better = [&](const Candidate &a, const Candidate &b) {
    bool strict = false;
    for (std::size_t i = 0; i < call.arguments.size(); ++i) {
      const auto &left = a.method->parameter_types[a.parameters[i]];
      const auto &right = b.method->parameter_types[b.parameters[i]];
      if (left == right)
        continue;
      const auto actual = type_of(*call.arguments[i]);
      if (actual == left) {
        strict = true;
        continue;
      }
      if (actual == right)
        return false;
      if (is_assignable(left, right) && !is_assignable(right, left))
        strict = true;
      else
        return false;
    }
    return strict;
  };
  const Candidate *best = nullptr;
  bool ambiguous = false;
  for (const auto &candidate : viable) {
    const bool dominated =
        std::any_of(viable.begin(), viable.end(), [&](const Candidate &other) {
          return better(other, candidate);
        });
    if (dominated)
      continue;
    if (best)
      ambiguous = true;
    else
      best = &candidate;
  }
  if (!best || ambiguous) {
    report("ARGOS-S0621", call.span,
           best ? "chamada ambígua entre sobrecargas"
                : "nenhuma sobrecarga aceita os tipos e rótulos fornecidos");
    return error_type();
  }
  return selected(*best->method, true);
}

Type SemanticAnalyzer::analyze_call(const ast::CallExpression &expression) {
  struct CalleeContext {
    bool &flag;
    bool previous;
    ~CalleeContext() { flag = previous; }
  } context{resolving_callee_, resolving_callee_};
  resolving_callee_ = false;
  const auto* identifier = dynamic_cast<const ast::IdentifierExpression*>(expression.callee.get());
  if (identifier && identifier->name == "print") {
    for (const auto& arg : expression.arguments) analyze_expression(*arg);
    return Type{TypeKind::void_type};
  }
  if (identifier && identifier->name == "super") {
    const auto base = superclass(current_class_);
    if (!in_constructor_ || base.empty() || super_initialized_ ||
        calling_super_) {
      report("ARGOS-S0622", expression.span,
             "super() exige primeira instrução do construtor e uma única "
             "chamada à superclasse");
      for (const auto &arg : expression.arguments)
        analyze_expression(*arg);
      return error_type();
    }
    auto candidates = classes_.at(base).methods["construct"];
    if (candidates.empty()) {
      Symbol implicit;
      implicit.name = "construct";
      implicit.owner = base;
      implicit.kind = Symbol::Kind::function;
      implicit.return_type = Type{TypeKind::void_type};
      candidates.push_back(implicit);
    }
    calling_super_ = true;
    const auto returned = resolve_overload(expression, candidates);
    calling_super_ = false;
    super_initialized_ = true;
    return returned;
  }
  const auto qualified = [&](auto &&self,
                             const ast::Expression &node) -> std::string {
    if (const auto *id = dynamic_cast<const ast::IdentifierExpression *>(&node))
      return id->name;
    if (const auto *member =
            dynamic_cast<const ast::MemberAccessExpression *>(&node);
        member && !member->safe) {
      const auto base = self(self, *member->base);
      return base.empty() ? "" : base + "." + member->member;
    }
    return "";
  };
  if (!identifier) {
    const auto name = qualified(qualified, *expression.callee);
    const auto found = global_overloads_.find(name);
    if (found != global_overloads_.end())
      return resolve_overload(expression, found->second);
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
    callable = *symbol;
    expression_symbols_[&expression] = symbol->id;
    expression_symbols_[expression.callee.get()] = symbol->id;
  }
  resolving_callee_ =
      identifier || dynamic_cast<const ast::MemberAccessExpression *>(
                        expression.callee.get());
  Type type = analyze_expression(*expression.callee);
  resolving_callee_ = false;
  const auto *safe_member = dynamic_cast<const ast::MemberAccessExpression *>(
      expression.callee.get());
  const bool safe_call = safe_member && safe_member->safe;
  if (safe_call && type.is_optional())
    type = type.unwrapped();
  if (type.kind != TypeKind::function_type || type.is_optional() || type.signature.empty()) {
    report(diagnostics::catalog::semantic::calls::non_named_callee.code, expression.callee->span,
           "chamada exige valor de tipo função não opcional");
    for (const auto& arg : expression.arguments) analyze_expression(*arg);
    return error_type();
  }
  if (const auto* access = dynamic_cast<const ast::MemberAccessExpression*>(expression.callee.get())) {
    const auto base_type = type_of(*access->base);
    if (classes_.contains(base_type.nominal_name)) {
      const auto &members = classes_.at(base_type.nominal_name).members;
      if (members.contains(access->member))
        callable = members.at(access->member);
    }
  }
  if (callable.kind != Symbol::Kind::function) {
    callable.parameter_types.assign(type.signature.begin(), type.signature.end() - 1);
    callable.parameter_labels.assign(callable.parameter_types.size(), "");
    callable.return_type = type.signature.back();
  }
  const auto before_arguments = scopes_;
  std::vector<Symbol> candidates;
  if (callable.kind == Symbol::Kind::function) {
    if (safe_member) {
      const auto nominal = type_of(*safe_member->base).nominal_name;
      if (classes_.contains(nominal) &&
          classes_.at(nominal).methods.contains(safe_member->member))
        candidates = classes_.at(nominal).methods.at(safe_member->member);
    } else if (identifier && !callable.owner.empty()) {
      if (classes_.at(current_class_).methods.contains(identifier->name))
        candidates = classes_.at(current_class_).methods.at(identifier->name);
    } else if (identifier && global_overloads_.contains(callable.name))
      candidates = global_overloads_.at(callable.name);
  }
  auto returned = candidates.empty() ? check_call(expression, callable)
                                     : resolve_overload(expression, candidates);
  if (in_constructor_)
    if (const auto selected = resolved_calls_.find(&expression);
        selected != resolved_calls_.end() && !selected->second.owner.empty()) {
      const auto *receiver =
          safe_member ? dynamic_cast<const ast::IdentifierExpression *>(
                            safe_member->base.get())
                      : nullptr;
      const bool on_this =
          identifier ||
          (receiver && (receiver->name == "this" || receiver->name == "super"));
      if (on_this && selected->second.component_target.empty())
        report("ARGOS-S0614", expression.span,
               "método exige instância completamente inicializada");
      if (on_this && !selected->second.component_target.empty()) {
        if (calling_super_)
          report("ARGOS-S0622", expression.span,
                 "super não pode observar componentes de this");
        for (const auto &field : classes_.at(current_class_).fields)
          if (const auto *stored = lookup_field(field->name);
              stored && !stored->initialized)
            report("ARGOS-S0628", expression.span,
                   "componente deve ser inicializado antes da delegação");
      }
    }
  if (safe_member) {
    const auto *receiver = dynamic_cast<const ast::IdentifierExpression *>(
        safe_member->base.get());
    const auto selected = resolved_calls_.find(&expression);
    if (receiver && receiver->name == "super" &&
        selected != resolved_calls_.end() &&
        selected->second.modifiers.abstract_)
      report("ARGOS-S0622", expression.span,
             "super não pode invocar método abstrato");
  }
  if (safe_call)
    restore_initialization(before_arguments);
  if (safe_call && returned.kind != TypeKind::void_type &&
      !returned.is_optional())
    ++returned.nullable_depth;
  return returned;
}

Type SemanticAnalyzer::check_call(const ast::CallExpression &expression,
                                  const Symbol &callable, bool analyzed) {
  const Symbol* function = &callable;
  std::vector<bool> bound(function->parameter_types.size(), false);
  std::size_t next_positional = 0;
  for (std::size_t i = 0; i < expression.arguments.size(); ++i) {
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
    const Type actual =
        analyzed
            ? type_of(*expression.arguments[i])
            : analyze_expression(
                  *expression.arguments[i],
                  parameter < bound.size()
                      ? std::optional<Type>{function
                                                ->parameter_types[parameter]}
                      : std::nullopt);
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
  const auto qualified_name = [&](auto &&self,
                                  const ast::Expression &node) -> std::string {
    if (const auto *id = dynamic_cast<const ast::IdentifierExpression *>(&node))
      return id->name;
    if (const auto *member =
            dynamic_cast<const ast::MemberAccessExpression *>(&node);
        member && !member->safe) {
      const auto base = self(self, *member->base);
      return base.empty() ? "" : base + "." + member->member;
    }
    return "";
  };
  if (!current_class_.empty()) {
    const auto name = qualified_name(qualified_name, expression);
    const auto &components = classes_.at(current_class_).components;
    if (std::find(components.begin(), components.end(), name) !=
        components.end()) {
      component_bindings_[&expression] = {
          current_class_, component_field(current_class_, name)};
      if (!accessing_member_base_)
        report("ARGOS-S0631", expression.span,
               "componente só pode ser receptor qualificado");
      if (calling_super_)
        report("ARGOS-S0622", expression.span,
               "super não pode observar componentes de this");
      if (in_constructor_)
        for (const auto &pending : classes_.at(current_class_).fields)
          if (const auto *stored = lookup_field(pending->name);
              stored && !stored->initialized)
            report("ARGOS-S0628", expression.span,
                   "componente ainda não inicializado");
      capture_receiver(expression.span);
      return Type{TypeKind::nominal_type, 0, name};
    }
  }
  const bool previous_member_base = accessing_member_base_;
  const auto* direct_base = dynamic_cast<const ast::IdentifierExpression*>(expression.base.get());
  const auto base_name = qualified_name(qualified_name, *expression.base);
  accessing_member_base_ =
      direct_base != nullptr ||
      (!current_class_.empty() &&
       std::find(classes_.at(current_class_).components.begin(),
                 classes_.at(current_class_).components.end(),
                 base_name) != classes_.at(current_class_).components.end());
  const bool previous_target = analyzing_assignment_target_;
  analyzing_assignment_target_ = false;
  const auto previous_resolving = resolving_callee_;
  resolving_callee_ = false;
  const Type base = analyze_expression(*expression.base);
  resolving_callee_ = previous_resolving;
  analyzing_assignment_target_ = previous_target;
  accessing_member_base_ = previous_member_base;
  if (base.is_error())
    return base;

  if (expression.safe) {
    if (base.nullable_depth != 1) {
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
    if (!accessible_type(base.nominal_name, expression.span))
      return error_type();
    if (expression.member.starts_with("$with.")) {
      report("ARGOS-S0631", expression.span,
             "armazenamento interno de with não é acessível");
      return error_type();
    }
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
    if (in_constructor_ &&
        (member->kind == Symbol::Kind::property ||
         (member->kind == Symbol::Kind::function && !resolving_callee_))) {
      const auto* identifier = dynamic_cast<const ast::IdentifierExpression*>(expression.base.get());
      if (identifier &&
          (identifier->name == "this" || identifier->name == "super") &&
          !instance_ready())
        report("ARGOS-S0614", expression.span, "método exige instância completamente inicializada");
    }
    if (!resolving_callee_ && member->kind == Symbol::Kind::function &&
        classes_.at(base.nominal_name).methods.at(expression.member).size() > 1)
      report("ARGOS-S0621", expression.span,
             "referência a método sobrecarregado exige chamada explícita");
    if (direct_base && direct_base->name == "super" &&
        member->kind != Symbol::Kind::function && member->modifiers.abstract_)
      report("ARGOS-S0622", expression.span,
             "super não pode invocar membro abstrato sem implementação");
    Type result = member->type;
    if (expression.safe && !result.is_optional())
      ++result.nullable_depth;
    return result;
  }
  report("ARGOS-S0353", expression.span, "tipo nominal sem membros declarados");
  return error_type();
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
      report("ARGOS-S0361", expression.span, "'!!' exige um tipo opcional");
      return error_type();
    }
    return operand.unwrapped();
  }
  return error_type();
}

Type SemanticAnalyzer::resolve_type(const ast::TypeSyntax &syntax) {
  if (syntax.nullable_depth > 4095) {
    report("ARGOS-S0401", syntax.span,
           "profundidade opcional excede 4095 camadas");
    return error_type();
  }
  if (syntax.element_type) {
    auto element = resolve_type(*syntax.element_type);
    if (element.kind == TypeKind::void_type ||
        element.kind == TypeKind::never_type ||
        element.kind == TypeKind::array_type) {
      report("ARGOS-S0377", syntax.span,
             "array exige elemento habitável; arrays multidimensionais ainda "
             "não são suportados");
      return error_type();
    }
    Type type{TypeKind::array_type, syntax.nullable_depth};
    type.signature.push_back(element);
    return type;
  }
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
  else if (name == "byte" || name == "uint_8")
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
    const auto canonical =
        resolve_nominal(std::string{name}, current_class_, current_package_);
    if (classes_.contains(canonical)) {
      syntax.resolved_name = canonical;
      accessible_type(canonical, syntax.span);
      return Type{TypeKind::nominal_type, syntax.nullable_depth, canonical};
    }
    report("ARGOS-S0400", syntax.span, "tipo " + quoted(name) + " não foi declarado");
    return error_type();
  }

  if ((kind == TypeKind::void_type || kind == TypeKind::never_type) &&
      syntax.nullable_depth) {
    report("ARGOS-S0401", syntax.span,
           "tipo sem valores não admite opcionalidade");
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
  if ((from.kind == TypeKind::function_type ||
       from.kind == TypeKind::array_type) &&
      from.signature != to.signature)
    return false;

  return from.nullable_depth == to.nullable_depth ||
         from.nullable_depth + 1 == to.nullable_depth;
}

bool SemanticAnalyzer::is_equality_comparable(Type left, Type right) noexcept {
  if (left.is_null())
    return right.is_optional() || right.is_null();
  if (right.is_null())
    return left.is_optional();
  if (left.kind == TypeKind::function_type || right.kind == TypeKind::function_type ||
      left.kind == TypeKind::void_type || right.kind == TypeKind::void_type) return false;
  if (left.is_error() || right.is_error())
    return true;
  if (left.is_unknown() || right.is_unknown()) return false;
  if (left.is_null())
    return right.is_optional() || right.is_null();
  if (right.is_null())
    return left.is_optional();
  return left.kind == right.kind &&
         left.nullable_depth == right.nullable_depth &&
         left.nominal_name == right.nominal_name &&
         left.signature == right.signature;
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
  if (dynamic_cast<const ast::IndexExpression *>(&expression) != nullptr ||
      dynamic_cast<const ast::IdentifierExpression *>(&expression) != nullptr ||
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
  if (const auto *postfix =
          dynamic_cast<const ast::PostfixExpression *>(&expression);
      postfix && postfix->op == ast::PostfixOperator::force_unwrap)
    return is_mutable_lvalue(*postfix->operand);
  if (const auto *index =
          dynamic_cast<const ast::IndexExpression *>(&expression))
    return !index->safe && is_mutable_lvalue(*index->base);
  if (const auto *identifier =
          dynamic_cast<const ast::IdentifierExpression *>(&expression)) {
    if (const auto *component = component_binding(expression)) {
      const auto *field = lookup_field(component->field);
      return in_constructor_ && field && !field->initialized;
    }
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
  // A base constructor cannot know whether the actual object has uninitialized
  // derived fields. Until construction returns, this must not escape or
  // dispatch.
  return !in_constructor_;
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
  if (const auto *identifier =
          dynamic_cast<const ast::IdentifierExpression *>(&expression)) {
    if (const auto *component = component_binding(expression))
      return lookup_field(component->field);
    return lookup(identifier->name);
  }
  if (const auto* access = dynamic_cast<const ast::MemberAccessExpression*>(&expression)) {
    const auto* base = dynamic_cast<const ast::IdentifierExpression*>(access->base.get());
    if (base && base->name == "this") return lookup_field(access->member);
  }
  return nullptr;
}

Symbol SemanticAnalyzer::function_symbol(const ast::FunctionDeclaration& declaration) {
  Symbol result;
  result.declaration = &declaration;
  result.name = current_class_.empty() && !declaration.package_name.empty() &&
                        declaration.name != "main"
                    ? declaration.package_name + "." + declaration.name
                    : declaration.name;
  result.package_name = declaration.package_name;
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
    if (!current_class_.empty())
      for (const auto &component : classes_.at(current_class_).components)
        if (parameter->name == component_alias(component))
          report("ARGOS-S0626", parameter->span,
                 "parâmetro não pode ocultar qualificador with: " +
                     parameter->name);
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
      for (const auto &parent : found->second.bases)
        pending.push_back(parent);
  }
  return false;
}

namespace {
// Diagnostic spelling is not a signature encoding: an optional function and a
// function returning an optional may otherwise have the same printed name.
std::string type_key(const Type &type) {
  std::string key = std::to_string(static_cast<unsigned>(type.kind)) + "?" +
                    std::to_string(type.nullable_depth) + "N" +
                    std::to_string(type.nominal_name.size()) + ":" +
                    type.nominal_name;
  key += "S" + std::to_string(type.signature.size()) + "[";
  for (const auto &part : type.signature)
    key += type_key(part) + ";";
  return key + "]";
}
} // namespace

std::string SemanticAnalyzer::dispatch_key(const Symbol &symbol) {
  std::string key =
      std::to_string(symbol.name.size()) + ":" + symbol.name + "(";
  for (const auto &type : symbol.parameter_types)
    key += type_key(type) + ";";
  return key + ")";
}
std::string SemanticAnalyzer::virtual_key(const Symbol &symbol) {
  return dispatch_key(symbol) + ":" + type_key(symbol.return_type);
}
std::string SemanticAnalyzer::function_key(const Symbol &symbol) {
  if (symbol.name == "main" && symbol.owner.empty())
    return "main";
  return (symbol.owner.empty() ? ""
                               : std::to_string(symbol.owner.size()) + ":" +
                                     symbol.owner + ".") +
         dispatch_key(symbol);
}
const Symbol *
SemanticAnalyzer::resolved_call(const ast::CallExpression &call) const {
  const auto it = resolved_calls_.find(&call);
  return it == resolved_calls_.end() ? nullptr : &it->second;
}
const Symbol *SemanticAnalyzer::declaration_symbol(
    const ast::FunctionDeclaration &decl) const {
  const auto it = declaration_symbols_.find(&decl);
  return it == declaration_symbols_.end() ? nullptr : &it->second;
}
std::vector<Symbol>
SemanticAnalyzer::methods_of(const std::string &name) const {
  std::vector<Symbol> result;
  if (const auto it = classes_.find(name); it != classes_.end())
    for (const auto &[key, methods] : it->second.methods)
      for (const auto &method : methods)
        result.push_back(method);
  return result;
}
std::string SemanticAnalyzer::superclass(const std::string &name) const {
  const auto found = classes_.find(name);
  if (found != classes_.end())
    for (const auto &base : found->second.bases)
      if (classes_.contains(base) &&
          !classes_.at(base).declaration->is_interface)
        return base;
  return {};
}
std::vector<const ast::ClassDeclaration *>
SemanticAnalyzer::class_declarations() const {
  return class_order_;
}
const std::vector<std::string> &
SemanticAnalyzer::bases_of(const std::string &name) const {
  return classes_.at(name).bases;
}
const std::vector<std::unique_ptr<ast::VariableDeclaration>> &
SemanticAnalyzer::generated_fields(const std::string &name) const {
  return classes_.at(name).fields;
}
const std::vector<std::unique_ptr<ast::FunctionDeclaration>> &
SemanticAnalyzer::generated_methods(const std::string &name) const {
  return classes_.at(name).generated;
}
const SemanticAnalyzer::ComponentBinding *
SemanticAnalyzer::component_binding(const ast::Expression &expression) const {
  const auto it = component_bindings_.find(&expression);
  return it == component_bindings_.end() ? nullptr : &it->second;
}
std::string
SemanticAnalyzer::resolve_nominal(const std::string &name,
                                  const std::string &owner,
                                  const std::string &package) const {
  if (name.find('.') != std::string::npos && classes_.contains(name))
    return name;
  auto prefix = owner.empty() ? package : owner;
  while (!prefix.empty()) {
    const auto candidate = prefix + "." + name;
    if (classes_.contains(candidate))
      return candidate;
    const auto dot = prefix.rfind('.');
    prefix = dot == std::string::npos ? "" : prefix.substr(0, dot);
  }
  std::vector<std::string> pending;
  std::unordered_set<std::string> visited;
  if (classes_.contains(owner))
    pending = classes_.at(owner).bases;
  while (!pending.empty()) {
    const auto base = pending.back();
    pending.pop_back();
    if (!visited.insert(base).second || !classes_.contains(base))
      continue;
    if (classes_.contains(base + "." + name))
      return base + "." + name;
    for (const auto &ancestor : classes_.at(base).bases)
      pending.push_back(ancestor);
  }
  if (!package.empty() && classes_.contains(package + "." + name))
    return package + "." + name;
  return name;
}
bool SemanticAnalyzer::accessible_type(const std::string &name,
                                       const source::SourceSpan &span) {
  const auto it = classes_.find(name);
  if (it == classes_.end())
    return false;
  const auto &type = *it->second.declaration;
  if (!type.enclosing_type.empty() &&
      !accessible_type(type.enclosing_type, span))
    return false;
  bool valid = true;
  if (type.modifiers.visibility == ast::Visibility::package_)
    valid = current_package_ == type.package_name;
  if (type.modifiers.visibility == ast::Visibility::private_)
    valid = current_class_ == type.enclosing_type ||
            current_class_.starts_with(type.enclosing_type + ".");
  if (type.modifiers.visibility == ast::Visibility::protected_)
    valid = current_class_ == type.enclosing_type ||
            current_class_.starts_with(type.enclosing_type + ".") ||
            is_subtype(current_class_, type.enclosing_type);
  if (!valid)
    report("ARGOS-S0630", span,
           "tipo inacessível neste pacote ou escopo: " + name);
  return valid;
}
bool SemanticAnalyzer::accessible(const Symbol &symbol,
                                  const source::SourceSpan &span) {
  if (symbol.modifiers.sealed_) {
    const auto owner = classes_.find(symbol.owner);
    const bool authorized =
        current_class_ == symbol.owner ||
        (owner != classes_.end() &&
         std::find(owner->second.permissions.begin(),
                   owner->second.permissions.end(),
                   current_class_) != owner->second.permissions.end());
    if (!authorized)
      report("ARGOS-S0624", span,
             "acesso a dado sealed exige entidade listada em permited de " +
                 symbol.owner);
    return authorized;
  }
  if (symbol.owner.empty() && symbol.kind == Symbol::Kind::function) {
    const bool valid =
        (symbol.modifiers.visibility != ast::Visibility::private_ &&
         symbol.modifiers.visibility != ast::Visibility::package_) ||
        symbol.package_name == current_package_;
    if (!valid)
      report("ARGOS-S0611", span,
             "função inacessível neste pacote: " + symbol.name);
    return valid;
  }
  if (symbol.modifiers.visibility == ast::Visibility::package_ &&
      classes_.contains(symbol.owner) &&
      classes_.at(symbol.owner).declaration->package_name != current_package_) {
    report("ARGOS-S0611", span,
           "membro default inacessível neste pacote: " + symbol.name);
    return false;
  }
  const auto nest = [&](std::string type) {
    while (classes_.contains(type) &&
           !classes_.at(type).declaration->enclosing_type.empty())
      type = classes_.at(type).declaration->enclosing_type;
    return type;
  };
  const bool nestmate =
      !current_class_.empty() && nest(current_class_) == nest(symbol.owner);
  if ((symbol.modifiers.visibility == ast::Visibility::private_ &&
       current_class_ != symbol.owner && !nestmate) ||
      (symbol.modifiers.visibility == ast::Visibility::protected_ &&
       (current_class_.empty() || !is_subtype(current_class_, symbol.owner)))) {
    report("ARGOS-S0611", span, "membro inacessível: " + symbol.name);
    return false;
  }
  return true;
}
bool SemanticAnalyzer::immutable_data_type(
    const Type &type, std::vector<std::string> &visiting) const {
  if (type.kind == TypeKind::nominal_type) {
    const auto it = classes_.find(type.nominal_name);
    if (it == classes_.end() || !it->second.declaration->is_data ||
        std::find(visiting.begin(), visiting.end(), type.nominal_name) !=
            visiting.end())
      return false;
    visiting.push_back(type.nominal_name);
    for (const auto &member : it->second.declaration->members)
      if (const auto *field =
              dynamic_cast<const ast::VariableDeclaration *>(member.get())) {
        if (field->mutability == ast::Mutability::mutable_ ||
            field->is_property)
          return false;
        // Resolve nominal links from syntax without mutating the analyzer.
        if (!field->type || field->type->element_type ||
            field->type->result_type)
          return false;
        if (classes_.contains(
                resolve_nominal(field->type->name, type.nominal_name,
                                it->second.declaration->package_name)) &&
            !immutable_data_type(
                Type{TypeKind::nominal_type, 0,
                     resolve_nominal(field->type->name, type.nominal_name,
                                     it->second.declaration->package_name)},
                visiting))
          return false;
      }
    visiting.pop_back();
    return true;
  }
  return type.kind != TypeKind::array_type &&
         type.kind != TypeKind::function_type &&
         type.kind != TypeKind::void_type && type.kind != TypeKind::never_type;
}
namespace {
ast::TypeSyntaxPtr clone_syntax(const ast::TypeSyntax *source) {
  if (!source)
    return nullptr;
  auto result = std::make_unique<ast::TypeSyntax>(
      source->name, source->nullable_depth, source->span);
  result->resolved_name = source->resolved_name;
  result->element_type = clone_syntax(source->element_type.get());
  result->result_type = clone_syntax(source->result_type.get());
  for (const auto &parameter : source->parameter_types)
    result->parameter_types.push_back(clone_syntax(parameter.get()));
  return result;
}
std::string component_alias(const std::string &type) {
  const auto dot = type.rfind('.');
  return dot == std::string::npos ? type : type.substr(dot + 1);
}
std::string component_field(const std::string &owner,
                            const std::string &component) {
  return "$with." + owner + "." + component;
}
} // namespace
bool SemanticAnalyzer::explicitly_initializes(
    const ast::FunctionDeclaration &constructor,
    const std::string &field) const {
  const auto symbol = declaration_symbols_.find(&constructor);
  if (symbol == declaration_symbols_.end() ||
      !classes_.contains(symbol->second.owner))
    return false;
  const auto &map = classes_.at(symbol->second.owner).explicit_components;
  const auto entry = map.find(&constructor);
  return entry != map.end() && entry->second.contains(field);
}
void SemanticAnalyzer::capture_receiver(const source::SourceSpan &span) {
  if (active_lambdas_.empty())
    return;
  if (constructor_lambda_context_)
    report("ARGOS-S0613", span,
           "closure não pode capturar instância em construção");
  if (const auto *receiver = lookup("this"))
    for (const auto *lambda : active_lambdas_) {
      auto &captures = lambda_captures_[lambda];
      if (std::none_of(captures.begin(), captures.end(),
                       [&](const Symbol &capture) {
                         return capture.id == receiver->id;
                       }))
        captures.push_back(*receiver);
    }
}
void SemanticAnalyzer::build_components(const std::string &name) {
  auto &info = classes_.at(name);
  const auto &owner = *info.declaration;
  if ((owner.is_interface || owner.is_data) && !info.components.empty()) {
    report("ARGOS-S0626", owner.span,
           "with é composição de class, não de interface ou data");
    return;
  }
  std::unordered_set<std::string> seen;
  std::unordered_map<std::string, std::vector<std::pair<std::string, Symbol>>>
      providers;
  for (const auto &component : info.components) {
    const auto target = classes_.find(component);
    if (target == classes_.end() || !seen.insert(component).second) {
      report("ARGOS-S0626", owner.span,
             "componente inexistente, repetido ou qualificador ambíguo: " +
                 component);
      continue;
    }
    if (target->second.state == 1) {
      report("ARGOS-S0626", owner.span, "ciclo de composição: " + component);
      continue;
    }
    build_class(component);
    accessible_type(component, owner.span);
    if (target->second.declaration->is_interface ||
        target->second.declaration->modifiers.abstract_) {
      report("ARGOS-S0626", owner.span,
             "componente with deve ser concreto: " + component);
      continue;
    }
    if (info.members.contains(component_alias(component)))
      report("ARGOS-S0626", owner.span,
             "nome do componente conflita com membro: " +
                 component_alias(component));
    const auto span = owner.span;
    auto type = std::make_unique<ast::TypeSyntax>(component, 0, span);
    type->resolved_name = component;
    ast::ExpressionPtr initializer;
    const auto constructors = target->second.methods.find("construct");
    bool automatic = constructors == target->second.methods.end();
    const auto constructor_accessible = [&](const Symbol &method) {
      const auto nest = [&](std::string candidate) {
        while (classes_.contains(candidate) &&
               !classes_.at(candidate).declaration->enclosing_type.empty())
          candidate = classes_.at(candidate).declaration->enclosing_type;
        return candidate;
      };
      if (method.modifiers.visibility == ast::Visibility::private_)
        return !current_class_.empty() &&
               nest(current_class_) == nest(method.owner);
      if (method.modifiers.visibility == ast::Visibility::protected_)
        return !current_class_.empty() &&
               is_subtype(current_class_, method.owner);
      if (method.modifiers.visibility == ast::Visibility::package_)
        return classes_.at(method.owner).declaration->package_name ==
               current_package_;
      return true;
    };
    if (constructors != target->second.methods.end())
      for (const auto &method : constructors->second)
        if (method.parameter_types.empty() && constructor_accessible(method)) {
          automatic = true;
          break;
        }
    if (automatic) {
      auto creation = std::make_unique<ast::NewExpression>(span);
      creation->type = clone_syntax(type.get());
      creation->call = std::make_unique<ast::CallExpression>(
          std::make_unique<ast::IdentifierExpression>(component, span),
          std::vector<ast::ExpressionPtr>{}, span);
      initializer = std::move(creation);
    }
    auto field = std::make_unique<ast::VariableDeclaration>(
        ast::Mutability::immutable, component_field(name, component),
        std::move(type), std::move(initializer), span);
    field->modifiers.visibility = ast::Visibility::private_;
    field->package_name = owner.package_name;
    Symbol storage;
    storage.name = field->name;
    storage.owner = name;
    storage.kind = Symbol::Kind::variable;
    storage.type = Type{TypeKind::nominal_type, 0, component};
    storage.modifiers = field->modifiers;
    storage.initialized = automatic;
    storage.span = span;
    info.members[storage.name] = storage;
    info.fields.push_back(std::move(field));
    if (!automatic && !info.methods.contains("construct"))
      report("ARGOS-S0628", span,
             "componente sem construtor padrão acessível exige construct e "
             "inicialização explícita: " +
                 component_alias(component) + " = new " + component + "(...)");
    for (const auto &method : methods_of(component)) {
      if (method.name == "construct" || method.name.starts_with('$') ||
          method.modifiers.visibility != ast::Visibility::public_ ||
          method.modifiers.static_ || method.modifiers.abstract_)
        continue;
      providers[dispatch_key(method)].push_back({component, method});
    }
  }
  for (const auto &member : owner.members)
    if (const auto *constructor =
            dynamic_cast<const ast::FunctionDeclaration *>(member.get());
        constructor && constructor->name == "construct" && constructor->body) {
      const auto visit = [&](auto &&self, const ast::Node &node) -> void {
        if (const auto *assignment =
                dynamic_cast<const ast::AssignmentExpression *>(&node)) {
          if (const auto *id = dynamic_cast<const ast::IdentifierExpression *>(
                  assignment->target.get()))
            for (const auto &component : info.components)
              if (id->name == component ||
                  id->name == component_alias(component))
                info.explicit_components[constructor].insert(
                    component_field(name, component));
        } else if (const auto *expression =
                       dynamic_cast<const ast::ExpressionStatement *>(&node))
          self(self, *expression->expression);
        else if (const auto *block =
                     dynamic_cast<const ast::BlockStatement *>(&node))
          for (const auto &item : block->items)
            self(self, *item);
        else if (const auto *branch =
                     dynamic_cast<const ast::IfStatement *>(&node)) {
          self(self, *branch->then_branch);
          if (branch->else_branch)
            self(self, *branch->else_branch);
        } else if (const auto *loop =
                       dynamic_cast<const ast::WhileStatement *>(&node))
          self(self, *loop->body);
      };
      visit(visit, *constructor->body);
    }
  for (const auto &[key, candidates] : providers) {
    const auto &method = candidates.front().second;
    auto &overloads = info.methods[method.name];
    const auto same = std::find_if(
        overloads.begin(), overloads.end(),
        [&](const Symbol &existing) { return dispatch_key(existing) == key; });
    if (same != overloads.end() && same->owner == name)
      continue;
    if (same != overloads.end() || candidates.size() != 1) {
      report("ARGOS-S0627", owner.span,
             "assinatura herdada/delegada ambígua; declare uma implementação "
             "explícita: " +
                 method.name);
      continue;
    }
    const auto &source = *method.declaration;
    std::vector<ast::ParameterPtr> parameters;
    std::vector<ast::ExpressionPtr> arguments;
    std::vector<std::string> labels;
    for (const auto &parameter : source.parameters) {
      auto copy = std::make_unique<ast::Parameter>(
          parameter->name, clone_syntax(parameter->type.get()), owner.span);
      copy->external_label = parameter->external_label;
      labels.push_back(copy->external_label);
      arguments.push_back(
          std::make_unique<ast::IdentifierExpression>(copy->name, owner.span));
      parameters.push_back(std::move(copy));
    }
    auto access = std::make_unique<ast::MemberAccessExpression>(
        std::make_unique<ast::IdentifierExpression>(candidates.front().first,
                                                    owner.span),
        method.name, false, owner.span);
    auto call = std::make_unique<ast::CallExpression>(
        std::move(access), std::move(arguments), owner.span);
    call->argument_labels = std::move(labels);
    std::vector<ast::BlockItemPtr> statements;
    if (method.return_type.kind == TypeKind::void_type)
      statements.push_back(std::make_unique<ast::ExpressionStatement>(
          std::move(call), owner.span));
    else
      statements.push_back(
          std::make_unique<ast::ReturnStatement>(std::move(call), owner.span));
    auto generated = std::make_unique<ast::FunctionDeclaration>(
        method.name, std::move(parameters),
        clone_syntax(source.return_type.get()),
        std::make_unique<ast::BlockStatement>(std::move(statements),
                                              owner.span),
        owner.span);
    generated->modifiers.visibility = ast::Visibility::public_;
    generated->modifiers.overload_ = !overloads.empty();
    generated->package_name = owner.package_name;
    auto symbol = method;
    symbol.owner = name;
    symbol.modifiers = generated->modifiers;
    symbol.declaration = generated.get();
    symbol.component_target = component_field(name, candidates.front().first);
    symbol.package_name = owner.package_name;
    overloads.push_back(symbol);
    info.members[method.name] = overloads.front();
    declaration_symbols_[generated.get()] = symbol;
    info.generated.push_back(std::move(generated));
  }
}

void SemanticAnalyzer::build_class(const std::string& name) {
  auto& info = classes_.at(name);
  const auto& declaration = *info.declaration;
  if (info.state == 2) return;
  if (info.state == 1) {
    report("ARGOS-S0603", declaration.span, "ciclo de herança: " + name);
    return;
  }
  info.state = 1;
  if (declaration.modifiers.override_ || declaration.modifiers.overload_ ||
      declaration.modifiers.static_ || declaration.modifiers.virtual_ ||
      (declaration.modifiers.final_ &&
       (declaration.modifiers.abstract_ || declaration.is_interface)))
    report("ARGOS-S0600", declaration.span,
           "modificadores incompatíveis no tipo");
  if (declaration.modifiers.sealed_ && declaration.modifiers.final_)
    report("ARGOS-S0600", declaration.span, "sealed e final são incompatíveis");
  if (!declaration.permited.empty() && !declaration.modifiers.sealed_ &&
      !declaration.is_data)
    report("ARGOS-S0624", declaration.span,
           "permited exige sealed class/interface ou data");
  std::unordered_set<std::string> granted;
  for (const auto &entity : info.permissions) {
    if (!granted.insert(entity).second || entity == declaration.name)
      report("ARGOS-S0624", declaration.span,
             "entidade permited repetida ou igual ao próprio tipo: " + entity);
    if (!classes_.contains(entity))
      report("ARGOS-S0624", declaration.span,
             "entidade permited não declarada: " + entity);
  }
  unsigned class_count = 0;
  std::unordered_set<std::string> seen_bases;
  for (const auto &base : info.bases) {
    const auto it = classes_.find(base);
    if (it == classes_.end()) {
      report("ARGOS-S0400", declaration.span, "base não declarada: " + base);
      continue;
    }
    if (!seen_bases.insert(base).second) {
      report("ARGOS-S0604", declaration.span, "base repetida");
      continue;
    }
    build_class(base);
    {
      const auto saved_class = current_class_;
      const auto saved_package = current_package_;
      current_class_ = declaration.name;
      current_package_ = declaration.package_name;
      accessible_type(base, declaration.span);
      current_class_ = saved_class;
      current_package_ = saved_package;
    }
    const auto &parent = it->second;
    if (parent.declaration->modifiers.sealed_ &&
        std::find(parent.permissions.begin(), parent.permissions.end(), name) ==
            parent.permissions.end())
      report("ARGOS-S0624", declaration.span,
             "entidade não autorizada em permited de " + base + ": " + name);
    const bool implementation =
        std::find(info.interfaces.begin(), info.interfaces.end(), base) !=
        info.interfaces.end();
    if (implementation &&
        (!parent.declaration->is_interface || declaration.is_interface))
      report("ARGOS-S0625", declaration.span,
             "by implementa interfaces; não admite superclasses nem declaração "
             "interface");
    if (!implementation && parent.declaration->is_interface &&
        !declaration.is_interface)
      report("ARGOS-S0625", declaration.span,
             "use by para implementar interfaces; ':' é herança de classe");
    if (!parent.declaration->is_interface) {
      if (++class_count > 1 || declaration.is_interface || declaration.is_data)
        report("ARGOS-S0604", declaration.span,
               "somente uma superclasse é permitida; data não estende classes");
      if (parent.declaration->modifiers.final_ || parent.declaration->is_data)
        report("ARGOS-S0605", declaration.span,
               "classe final não pode ser estendida");
    }
    for (const auto &[key, member] : parent.members) {
      if (key == "construct" || member.kind == Symbol::Kind::function)
        continue;
      const auto old = info.members.find(key);
      if (old == info.members.end())
        info.members[key] = member;
      else if (old->second.kind != member.kind ||
               old->second.type != member.type)
        report("ARGOS-S0606", declaration.span,
               "membros herdados incompatíveis: " + key);
      else if (member.kind == Symbol::Kind::property) {
        if (member.mutability == ast::Mutability::mutable_)
          old->second.mutability = ast::Mutability::mutable_;
        if (old->second.modifiers.abstract_ && !member.modifiers.abstract_)
          old->second = member;
        else if (!old->second.modifiers.abstract_ &&
                 member.modifiers.abstract_ &&
                 member.mutability == ast::Mutability::mutable_ &&
                 old->second.mutability != member.mutability)
          report("ARGOS-S0606", declaration.span,
                 "setter herdado não atende contrato");
      }
    }
    for (const auto &[key, methods] : parent.methods)
      for (const auto &method : methods) {
        if (key == "construct" ||
            (method.modifiers.visibility == ast::Visibility::private_ &&
             !parent.declaration->is_interface))
          continue;
        auto &target = info.methods[key];
        auto old = std::find_if(
            target.begin(), target.end(), [&](const Symbol &other) {
              return dispatch_key(other) == dispatch_key(method);
            });
        if (old == target.end())
          target.push_back(method);
        else if (old->type != method.type ||
                 old->parameter_labels != method.parameter_labels)
          report("ARGOS-S0606", declaration.span,
                 "assinaturas herdadas incompatíveis: " + key);
        else {
          const Symbol *contract =
              parent.declaration->is_interface                    ? &method
              : classes_.at(old->owner).declaration->is_interface ? &*old
                                                                  : nullptr;
          const Symbol *implementation_member =
              old->modifiers.abstract_ ? &method : &*old;
          if (contract && !implementation_member->modifiers.abstract_ &&
              contract->modifiers.visibility == ast::Visibility::public_ &&
              implementation_member->modifiers.visibility !=
                  ast::Visibility::public_)
            report(
                "ARGOS-S0606", declaration.span,
                "implementação reduz a visibilidade do contrato de interface");
          if (old->modifiers.abstract_ && !method.modifiers.abstract_)
            *old = method;
        }
      }
  }
  const auto previous_class = current_class_;
  const auto previous_package = current_package_;
  current_package_ = declaration.package_name;
  current_class_ = name;
  std::unordered_set<std::string> local_names, local_signatures;
  const auto override_check = [&](const Symbol &parent, const Symbol &child,
                                  const ast::Node &node) {
    const bool requirement =
        classes_.at(parent.owner).declaration->is_interface;
    if (!child.modifiers.override_ && !requirement)
      report("ARGOS-S0608", node.span, "sobrescrita exige override");
    if (parent.modifiers.final_ ||
        (!requirement && !parent.modifiers.virtual_ &&
         !parent.modifiers.abstract_ && !parent.modifiers.override_))
      report("ARGOS-S0608", node.span,
             "membro herdado não permite sobrescrita");
    if (parent.type != child.type ||
        parent.parameter_labels != child.parameter_labels)
      report("ARGOS-S0606", node.span,
             "assinatura ou rótulos incompatíveis na sobrescrita");
    if ((parent.modifiers.visibility == ast::Visibility::public_ &&
         child.modifiers.visibility != ast::Visibility::public_) ||
        (parent.modifiers.visibility == ast::Visibility::protected_ &&
         child.modifiers.visibility != ast::Visibility::public_ &&
         child.modifiers.visibility != ast::Visibility::protected_))
      report("ARGOS-S0608", node.span,
             "sobrescrita não pode reduzir visibilidade");
  };
  for (const auto& member : declaration.members) {
    const auto* field = dynamic_cast<const ast::VariableDeclaration*>(member.get());
    const auto* method = dynamic_cast<const ast::FunctionDeclaration*>(member.get());
    if (field) {
      Symbol symbol;
      symbol.name = field->name;
      symbol.owner = name;
      symbol.span = field->span;
      symbol.kind =
          field->is_property ? Symbol::Kind::property : Symbol::Kind::variable;
      symbol.type = field->type ? resolve_type(*field->type) : error_type();
      symbol.modifiers = field->modifiers;
      if (declaration.is_data)
        symbol.modifiers.sealed_ = true;
      symbol.mutability = field->is_property && !field->setter
                              ? ast::Mutability::immutable
                              : field->mutability;
      if (!field->type || symbol.type.kind == TypeKind::void_type ||
          symbol.type.kind == TypeKind::never_type)
        report("ARGOS-S0607", field->span,
               "campo exige tipo explícito habitável");
      if (field->name == "this" || field->name == "super" ||
          field->name == "construct")
        report("ARGOS-S0600", field->span, "nome de campo reservado");
      if (field->name == "this" || field->name == "super" ||
          field->name == "construct" ||
          !local_names.insert(field->name).second ||
          info.methods.contains(field->name))
        report("ARGOS-S0001", field->span,
               "nome de campo/propriedade inválido ou duplicado");
      if (declaration.is_interface && !field->is_property)
        report("ARGOS-S0607", field->span, "interface não armazena estado");
      if (!field->is_property &&
          (field->modifiers.override_ || field->modifiers.virtual_ ||
           field->modifiers.abstract_ || field->modifiers.final_))
        report("ARGOS-S0600", field->span,
               "campo não admite modificadores de despacho");
      if (declaration.is_data) {
        std::vector<std::string> visiting{name};
        if (field->mutability == ast::Mutability::mutable_ ||
            field->is_property || !immutable_data_type(symbol.type, visiting))
          report("ARGOS-S0623", field->span,
                 "data exige campos transitivamente imutáveis; arrays, "
                 "closures e objetos mutáveis são proibidos");
      }
      if (field->is_property) {
        if (!field->getter || field->initializer ||
            (field->setter && field->mutability != ast::Mutability::mutable_))
          report("ARGOS-S0607", field->span,
                 "propriedade exige getter; setter exige var mut e não há "
                 "inicializador");
        if (declaration.is_interface)
          symbol.modifiers.abstract_ = true;
        if ((symbol.modifiers.virtual_ || symbol.modifiers.abstract_) &&
            symbol.modifiers.visibility == ast::Visibility::private_ &&
            !declaration.is_interface)
          report("ARGOS-S0600", field->span,
                 "propriedade privada não pode ser virtual/abstract");
        for (const auto* accessor : {field->getter.get(), field->setter.get()})
          if (accessor && symbol.modifiers.abstract_ && accessor->body)
            report("ARGOS-S0607", accessor->span,
                   "acessor abstrato não pode ter corpo");
      }
      if (const auto old = info.members.find(field->name);
          old != info.members.end()) {
        if (!field->is_property || old->second.kind != Symbol::Kind::property)
          report("ARGOS-S0606", field->span,
                 "campo herdado não pode ser ocultado");
        else {
          override_check(old->second, symbol, *field);
          if (old->second.mutability == ast::Mutability::mutable_ &&
              symbol.mutability != ast::Mutability::mutable_)
            report("ARGOS-S0606", field->span,
                   "setter do contrato deve ser preservado");
        }
      } else if (symbol.modifiers.override_)
        report("ARGOS-S0608", field->span, "override sem propriedade herdada");
      info.members[field->name] = symbol;
      if (field->is_property)
        for (const auto *accessor : {field->getter.get(), field->setter.get()})
          if (accessor) {
            auto helper = function_symbol(*accessor);
            helper.modifiers = symbol.modifiers;
            declaration_symbols_[accessor] = helper;
            info.members[helper.name] = helper;
            info.methods[helper.name] = {helper};
          }
    } else if (method) {
      auto symbol = function_symbol(*method);
      if (method->name == "init" || method->name == "this" ||
          method->name == "super")
        report("ARGOS-S0600", method->span,
               "nome de método inválido; use construct");
      if (declaration.is_interface) {
        if (method->body || method->name == "construct")
          report("ARGOS-S0607", method->span,
                 "interface contém somente contratos sem corpo");
        symbol.modifiers.abstract_ = true;
      }
      if (symbol.modifiers.sealed_ || symbol.modifiers.static_ ||
          (symbol.modifiers.abstract_ &&
           (method->body || symbol.modifiers.final_)) ||
          ((symbol.modifiers.virtual_ || symbol.modifiers.abstract_) &&
           symbol.modifiers.visibility == ast::Visibility::private_ &&
           !declaration.is_interface) ||
          (symbol.modifiers.virtual_ && symbol.modifiers.final_))
        report("ARGOS-S0600", method->span,
               "modificadores de método incompatíveis");
      if (method->name == "construct" &&
          (method->return_type || !method->body || symbol.modifiers.override_ ||
           symbol.modifiers.virtual_ || symbol.modifiers.abstract_))
        report("ARGOS-S0600", method->span,
               "construct exige corpo e não admite retorno ou despacho");
      const auto key = dispatch_key(symbol);
      if (!local_signatures.insert(key).second)
        report("ARGOS-S0001", method->span, "assinatura duplicada");
      if (local_names.contains(method->name) ||
          (info.members.contains(method->name) &&
           info.members.at(method->name).kind != Symbol::Kind::function))
        report("ARGOS-S0606", method->span,
               "método conflita com campo/propriedade");
      auto &methods = info.methods[method->name];
      auto old = std::find_if(
          methods.begin(), methods.end(),
          [&](const Symbol &other) { return dispatch_key(other) == key; });
      if (old != methods.end()) {
        if (old->owner != name)
          override_check(*old, symbol, *method);
        *old = symbol;
      } else {
        if (symbol.modifiers.override_)
          report("ARGOS-S0608", method->span,
                 "override sem assinatura herdada correspondente");
        if (!methods.empty() && !symbol.modifiers.overload_)
          report("ARGOS-S0620", method->span,
                 "sobrecarga adicional exige overload");
        methods.push_back(symbol);
      }
      info.members[method->name] = methods.front();
      declaration_symbols_[method] = symbol;
    }
    if (field && field->name.starts_with("$with."))
      report("ARGOS-S0631", field->span,
             "nome reservado ao armazenamento de composição");
    if (member->modifiers.static_)
      report("ARGOS-S0600", member->span, "static não pertence a esta etapa");
  }
  if (declaration.is_data)
    for (const auto builtin : {ast::FunctionDeclaration::Builtin::data_hash,
                               ast::FunctionDeclaration::Builtin::data_text}) {
      const auto method_name =
          builtin == ast::FunctionDeclaration::Builtin::data_hash ? "hashCode"
                                                                  : "toString";
      const auto existing = info.members.find(method_name);
      if (existing != info.members.end() &&
          (existing->second.owner == name || existing->second.kind != Symbol::Kind::function)) {
        report("ARGOS-S0632", declaration.span,
               "método reservado à geração de data: " +
                   std::string{method_name});
        continue;
      }
      auto method = std::make_unique<ast::FunctionDeclaration>(
          method_name, std::vector<ast::ParameterPtr>{},
          std::make_unique<ast::TypeSyntax>(
              builtin == ast::FunctionDeclaration::Builtin::data_hash
                  ? "int"
                  : "string",
              0, declaration.span),
          nullptr, declaration.span);
      method->modifiers.visibility = ast::Visibility::public_;
      method->builtin = builtin;
      method->package_name = declaration.package_name;
      auto symbol = function_symbol(*method);
      declaration_symbols_[method.get()] = symbol;
      auto& methods = info.methods[method_name];
      const auto inherited = std::find_if(methods.begin(), methods.end(), [&](const Symbol& candidate) {
        return dispatch_key(candidate) == dispatch_key(symbol);
      });
      if (inherited != methods.end()) {
        override_check(*inherited, symbol, *method);
        *inherited = symbol;
      } else {
        methods.push_back(symbol);
      }
      info.members[method_name] = methods.front();
      info.generated.push_back(std::move(method));
    }
  build_components(name);
  for (const auto &[key, methods] : info.methods) {
    if (!methods.empty())
      info.members[key] = methods.front();
    if (!declaration.is_interface && !declaration.modifiers.abstract_)
      for (const auto &method : methods)
        if (method.modifiers.abstract_)
          report("ARGOS-S0609", declaration.span,
                 "classe concreta não implementa " + dispatch_key(method));
  }
  if (!declaration.is_interface && !declaration.modifiers.abstract_)
    for (const auto &[key, member] : info.members)
      if (member.kind == Symbol::Kind::property && member.modifiers.abstract_)
        report("ARGOS-S0609", declaration.span,
               "propriedade abstrata sem implementação");
  if (declaration.is_data && declaration.modifiers.abstract_)
    report("ARGOS-S0623", declaration.span, "data é final e concreta");
  current_class_ = previous_class;
  current_package_ = previous_package;
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
  if (!(resolving_callee_ && symbol.kind == Symbol::Kind::function) &&
      !accessible(symbol, span))
    return nullptr;
  return &symbol;
}

void SemanticAnalyzer::analyze_class(const ast::ClassDeclaration& declaration) {
  const auto previous_class = current_class_;
  current_class_ = declaration.name;
  const auto previous_package = current_package_;
  current_package_ = declaration.package_name;
  enter_scope();
  declare(Symbol{.name = "this", .kind = Symbol::Kind::parameter,
                 .type = Type{TypeKind::nominal_type, 0, declaration.name}, .initialized = true});
  for (const auto& [name, member] : classes_.at(declaration.name).members) {
    auto symbol = member;
    symbol.initialized = true;
    declare(std::move(symbol));
  }
  for (const auto &field : classes_.at(declaration.name).fields)
    if (field->initializer) {
      initializing_field_ = true;
      const auto actual =
          analyze_expression(*field->initializer, Type{TypeKind::nominal_type,
                                                       0, field->type->name});
      (void)actual;
      initializing_field_ = false;
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
        const auto expected = classes_.at(declaration.name).members.at(field->name).type;
        const auto actual = analyze_expression(*field->initializer, expected);
        initializing_field_ = false;
        if (!is_assignable_expression(actual, expected, *field->initializer))
          report("ARGOS-S0101", field->span, "inicializador de campo incompatível");
        else record_coercion(*field->initializer, actual, expected);
      }
    } else if (const auto* method = dynamic_cast<const ast::FunctionDeclaration*>(member.get())) {
      const auto before_method = scopes_;
      // Initialization facts are local to each constructor or method.
      if (method->name == "construct") {
        for (const auto &generated : classes_.at(declaration.name).fields)
          if (auto *symbol = lookup_field(generated->name))
            symbol->initialized =
                generated->initializer != nullptr &&
                !explicitly_initializes(*method, generated->name);
        for (const auto& field_member : declaration.members)
          if (const auto* constructor_field = dynamic_cast<const ast::VariableDeclaration*>(field_member.get());
              constructor_field && !constructor_field->is_property)
            if (auto* symbol = lookup_field(constructor_field->name)) symbol->initialized = constructor_field->initializer != nullptr;
      }
      const bool previous_constructor = in_constructor_;
      in_constructor_ = method->name == "construct";
      const auto previous_super = super_initialized_;
      super_initialized_ = true;
      if (in_constructor_) {
        const auto base = superclass(declaration.name);
        if (!base.empty()) {
          const auto *first =
              method->body && !method->body->items.empty()
                  ? dynamic_cast<const ast::ExpressionStatement *>(
                        method->body->items.front().get())
                  : nullptr;
          const auto *call = first ? dynamic_cast<const ast::CallExpression *>(
                                         first->expression.get())
                                   : nullptr;
          const auto *target =
              call ? dynamic_cast<const ast::IdentifierExpression *>(
                         call->callee.get())
                   : nullptr;
          const bool explicit_super = target && target->name == "super";
          super_initialized_ = !explicit_super;
          if (!explicit_super) {
            const auto &constructors = classes_.at(base).methods;
            if (constructors.contains("construct")) {
              const auto &options = constructors.at("construct");
              const auto zero = std::find_if(
                  options.begin(), options.end(),
                  [](const Symbol &m) { return m.parameter_types.empty(); });
              if (zero == options.end())
                report("ARGOS-S0622", method->span,
                       "superclasse exige super(argumentos) como primeira "
                       "instrução");
              else
                accessible(*zero, method->span);
            }
          }
        }
      }
      analyze_function_declaration(*method);
      super_initialized_ = previous_super;
      in_constructor_ = previous_constructor;
      // Constructor bodies use the regular definite-initialization analysis.
      scopes_ = before_method;
    }
  }
  for (const auto &generated : classes_.at(declaration.name).generated)
    analyze_function_declaration(*generated);
  if (!classes_.at(declaration.name).methods.contains("construct")) {
    const auto base = superclass(declaration.name);
    if (!base.empty() && classes_.at(base).methods.contains("construct")) {
      const auto &options = classes_.at(base).methods.at("construct");
      const auto zero =
          std::find_if(options.begin(), options.end(), [](const Symbol &m) {
            return m.parameter_types.empty();
          });
      if (zero == options.end())
        report(
            "ARGOS-S0622", declaration.span,
            "classe exige construct com chamada explícita super(argumentos)");
      else
        accessible(*zero, declaration.span);
    }
  }
  leave_scope();
  current_class_ = previous_class;
  current_package_ = previous_package;
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
