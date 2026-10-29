#include "argos/parser/parser.hpp"
#include "argos/diagnostics/diagnostic_catalog.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace argos::parser {
namespace {

constexpr std::uint8_t assignment_precedence = 10;
constexpr std::uint8_t coalescing_precedence = 20;
constexpr std::uint8_t logical_or_precedence = 25;
constexpr std::uint8_t logical_and_precedence = 30;
constexpr std::uint8_t equality_precedence = 35;
constexpr std::uint8_t relational_precedence = 40;
constexpr std::uint8_t additive_precedence = 50;
constexpr std::uint8_t multiplicative_precedence = 60;
constexpr std::uint8_t prefix_precedence = 70;
constexpr std::uint8_t postfix_precedence = 80;

ast::TypeSyntaxPtr clone_type(const ast::TypeSyntax* type) {
    if (!type) return std::make_unique<ast::TypeSyntax>("<error>", 0, source::SourceSpan{});
    auto result = std::make_unique<ast::TypeSyntax>(type->name, type->nullable_depth, type->span);
    for (const auto& parameter : type->parameter_types)
        result->parameter_types.push_back(clone_type(parameter.get()));
    if (type->result_type) result->result_type = clone_type(type->result_type.get());
    if (type->element_type)
      result->element_type = clone_type(type->element_type.get());
    return result;
}

bool is_assignment_operator(lexer::TokenKind kind) noexcept {
  return kind == lexer::TokenKind::Equal ||
         kind == lexer::TokenKind::PlusEqual ||
         kind == lexer::TokenKind::MinusEqual ||
         kind == lexer::TokenKind::StarEqual ||
         kind == lexer::TokenKind::SlashEqual ||
         kind == lexer::TokenKind::PercentEqual ||
         kind == lexer::TokenKind::ForceUnwrapAssign ||
         kind == lexer::TokenKind::SafeAssign ||
         kind == lexer::TokenKind::NullCoalescingAssign;
}

bool is_postfix_operator(lexer::TokenKind kind) noexcept {
    return kind == lexer::TokenKind::ForceUnwrap
        || kind == lexer::TokenKind::PlusPlus
        || kind == lexer::TokenKind::MinusMinus;
}

bool is_type_token(lexer::TokenKind kind) noexcept {
    using K = lexer::TokenKind;
    switch (kind) {
    case K::Identifier: case K::EscapedIdentifier: case K::KwBool:
    case K::KwByte: case K::KwSByte: case K::KwShort: case K::KwUShort:
    case K::KwInt: case K::KwUInt: case K::KwLong: case K::KwULong:
    case K::KwISize: case K::KwUSize: case K::KwFloat: case K::KwDouble:
    case K::KwChar: case K::KwString: case K::KwVoid: case K::KwNever:
      return true;
    default: return false;
    }
}

bool is_reserved_keyword(lexer::TokenKind kind) noexcept {
    using K = lexer::TokenKind;
    return kind >= K::KwVar && kind <= K::KwNever;
}

} // namespace

Parser::Parser(
    std::vector<lexer::Token> tokens,
    diagnostics::DiagnosticEngine& diagnostics
)
    : tokens_(std::move(tokens)),
      diagnostics_(diagnostics) {
    if (
        tokens_.empty()
        || tokens_.back().kind != lexer::TokenKind::EndOfFile
    ) {
        tokens_.push_back(lexer::Token{
            .kind = lexer::TokenKind::EndOfFile,
            .lexeme = {},
            .span = {},
        });
    }
}

std::string Parser::parse_qualified_name() {
  auto name =
      consume(lexer::TokenKind::Identifier, "esperado nome de tipo ou pacote")
          .lexeme;
  while (match(lexer::TokenKind::Dot))
    name +=
        "." +
        consume(lexer::TokenKind::Identifier, "esperado nome após '.'").lexeme;
  return name;
}

ast::Program Parser::parse_program() {
    std::vector<ast::BlockItemPtr> items;
    if (match(lexer::TokenKind::KwPackage)) {
      package_name_ = parse_qualified_name();
      consume_statement_terminator("package");
    }
    while (!is_at_end()) {
        if (match(lexer::TokenKind::Semicolon)) {
            continue;
        }

        const std::size_t index_before = index_;
        auto item = parse_block_item();

        if (item != nullptr) {
            items.push_back(std::move(item));
        }

        if (index_ == index_before && !is_at_end()) {
            report(current(), "o parser não conseguiu avançar nesta construção");
            synchronize();
        }
    }

    const auto begin = items.empty()
        ? current().span.begin
        : items.front()->span.begin;
    const auto end = current().span.end;

    ast::Program result{std::move(items), span_from(begin, end)};
    result.package_name = package_name_;
    return result;
}

bool Parser::is_at_end() const noexcept {
    return current().kind == lexer::TokenKind::EndOfFile;
}

const lexer::Token& Parser::current() const noexcept {
    return tokens_[std::min(index_, tokens_.size() - 1)];
}

const lexer::Token& Parser::previous() const noexcept {
    if (index_ == 0) {
        return tokens_.front();
    }

    return tokens_[index_ - 1];
}

const lexer::Token& Parser::peek(std::size_t distance) const noexcept {
    return tokens_[std::min(index_ + distance, tokens_.size() - 1)];
}

lexer::Token Parser::advance() {
    const lexer::Token token = current();

    if (!is_at_end()) {
        ++index_;
    }

    return token;
}

bool Parser::check(lexer::TokenKind kind) const noexcept {
    return current().kind == kind;
}

bool Parser::match(lexer::TokenKind kind) {
    if (!check(kind)) {
        return false;
    }

    advance();
    return true;
}

bool Parser::match(std::initializer_list<lexer::TokenKind> kinds) {
    for (const lexer::TokenKind kind : kinds) {
        if (check(kind)) {
            advance();
            return true;
        }
    }

    return false;
}

lexer::Token Parser::consume(
    lexer::TokenKind kind,
    std::string message
) {
    if (check(kind)) {
        return advance();
    }
    if (kind == lexer::TokenKind::Identifier &&
        check(lexer::TokenKind::EscapedIdentifier)) {
        auto token = advance();
        token.kind = lexer::TokenKind::Identifier;
        token.lexeme = token.lexeme.substr(1, token.lexeme.size() - 2);
        return token;
    }

    report(current(), std::move(message));
    return synthetic_token(kind);
}

void Parser::report(
    const lexer::Token& token,
    std::string message
) {
    diagnostics_.emit(diagnostics::catalog::parser::unexpected_syntax,
                      token.span, std::move(message));
}

void Parser::synchronize() {
    advance();
    while (!is_at_end()) {
        if (previous().kind == lexer::TokenKind::Semicolon) {
            return;
        }

        switch (current().kind) {
            case lexer::TokenKind::KwVar:
            case lexer::TokenKind::KwFunction:
            case lexer::TokenKind::KwIf:
            case lexer::TokenKind::KwWhile:
            case lexer::TokenKind::KwUnsafe:
            case lexer::TokenKind::KwReturn:
            case lexer::TokenKind::RightBrace:
                return;
            default:
                advance();
        }
    }
}

void Parser::skip_unsupported_construct() {
    std::size_t braces = 0;
    while (!is_at_end()) {
        if (check(lexer::TokenKind::Semicolon) && braces == 0) {
            advance();
            return;
        }
        if (match(lexer::TokenKind::LeftBrace)) {
            ++braces;
            continue;
        }
        if (match(lexer::TokenKind::RightBrace)) {
            if (braces == 0) return;
            --braces;
            if (braces == 0) return;
            continue;
        }
        advance();
    }
}

void Parser::consume_statement_terminator(std::string context) {
    if (match(lexer::TokenKind::Semicolon)
        || check(lexer::TokenKind::RightBrace)
        || check(lexer::TokenKind::KwElse)
        || is_at_end()
        || has_line_break_before_current()) {
        return;
    }
    report(
        current(),
        "esperado ';' ou newline elegível após " + std::move(context)
    );
}

bool Parser::has_line_break_before_current() const noexcept {
    if (index_ == 0) {
        return false;
    }

    return previous().span.end.line < current().span.begin.line;
}

ast::BlockItemPtr Parser::parse_block_item() {
    if (check(lexer::TokenKind::Identifier) && current().lexeme == "functio") {
        const auto token = advance();
        diagnostics_.emit(diagnostics::catalog::parser::incomplete_keyword,
                          token.span, "'functio' não é uma palavra-chave; você quis dizer 'function'?");
        skip_unsupported_construct();
        return nullptr;
    }
    const auto modifier_start = index_;
    const auto modifiers = parse_modifiers();
    ast::BlockItemPtr declaration;
    if (match(lexer::TokenKind::KwVar))
        declaration = parse_variable_declaration(previous());
    else if (match(lexer::TokenKind::KwFunction))
        declaration = parse_function_declaration(previous());
    else if (match(lexer::TokenKind::KwProtocol)) {
      report(previous(), "protocol foi removido; use interface para contratos "
                         "ou data para dados imutáveis");
      skip_unsupported_construct();
      return nullptr;
    } else if (match({lexer::TokenKind::KwClass, lexer::TokenKind::KwInterface,
                      lexer::TokenKind::KwData}))
      declaration = parse_class_declaration(previous());
    if (declaration) {
        static_cast<ast::Declaration&>(*declaration).modifiers = modifiers;
        static_cast<ast::Declaration &>(*declaration).package_name =
            package_name_;
        if (auto *type =
                dynamic_cast<ast::ClassDeclaration *>(declaration.get())) {
          if (!modifiers.explicit_visibility)
            type->modifiers.visibility = ast::Visibility::public_;
          if (type->is_data)
            type->modifiers.final_ = true;
        }
        if (auto *function =
                dynamic_cast<ast::FunctionDeclaration *>(declaration.get());
            function && !modifiers.explicit_visibility)
          function->modifiers.visibility = ast::Visibility::public_;
        return declaration;
    }

    if (index_ != modifier_start) report(current(), "modificador exige declaração compatível");
    switch (current().kind) {
    case lexer::TokenKind::KwClass:
    case lexer::TokenKind::KwInterface:
    case lexer::TokenKind::KwEnum:
    case lexer::TokenKind::KwModule:
    case lexer::TokenKind::KwImport:
    case lexer::TokenKind::KwType:
    case lexer::TokenKind::KwExtern:
    case lexer::TokenKind::KwThreadLocal:
    case lexer::TokenKind::KwNative:
    case lexer::TokenKind::KwOperator:
    case lexer::TokenKind::KwConstruct:
    case lexer::TokenKind::KwDeinit:
    case lexer::TokenKind::KwWhere:
    case lexer::TokenKind::KwPublic:
    case lexer::TokenKind::KwPrivate:
    case lexer::TokenKind::KwProtected:
    case lexer::TokenKind::KwInternal:
    case lexer::TokenKind::At:
      report(current(), "declaração ainda não suportada pelo frontend");
      skip_unsupported_construct();
      return nullptr;
    default: break;
    }

    auto statement = parse_statement();
    if (statement)
      statement->package_name = package_name_;
    return statement;
}

ast::Modifiers Parser::parse_modifiers() {
    ast::Modifiers result;
    bool visibility = false;
    for (;;) {
        const auto kind = current().kind;
        bool* flag = nullptr;
        using K = lexer::TokenKind;
        switch (kind) {
        case K::KwPublic:
        case K::KwPrivate:
        case K::KwProtected:
        case K::KwInternal:
        case K::KwDefault:
          if (visibility)
            report(current(), "visibilidade repetida ou conflitante");
          visibility = true;
          result.explicit_visibility = true;
          result.visibility =
              kind == K::KwPublic      ? ast::Visibility::public_
              : kind == K::KwPrivate   ? ast::Visibility::private_
              : kind == K::KwProtected ? ast::Visibility::protected_
              : kind == K::KwDefault   ? ast::Visibility::package_
                                       : ast::Visibility::internal_;
          advance();
          continue;
        case K::KwFinal: flag = &result.final_; break;
        case K::KwSealed:
          flag = &result.sealed_;
          break;
        case K::KwAbstract: flag = &result.abstract_; break;
        case K::KwOverride: flag = &result.override_; break;
        case K::KwStatic: flag = &result.static_; break;
        case K::KwVirtual: flag = &result.virtual_; break;
        case K::KwOverload:
          flag = &result.overload_;
          break;
        default: return result;
        }
        if (*flag) report(current(), "modificador repetido");
        *flag = true;
        advance();
    }
}

ast::BlockItemPtr Parser::parse_class_declaration(const lexer::Token &keyword) {
  if (keyword.kind == lexer::TokenKind::KwData)
    match(lexer::TokenKind::KwClass);
  const auto name =
      consume(lexer::TokenKind::Identifier, "esperado nome do tipo");
  auto result = std::make_unique<ast::ClassDeclaration>(
      name.lexeme, keyword.kind == lexer::TokenKind::KwInterface, keyword.span);
  result->is_data = keyword.kind == lexer::TokenKind::KwData;
  std::vector<ast::VariableDeclaration *> bare_components;
  result->simple_name = name.lexeme;
  result->enclosing_type = enclosing_type_;
  result->package_name = package_name_;
  result->name = (enclosing_type_.empty()
                      ? (package_name_.empty() ? "" : package_name_ + ".")
                      : enclosing_type_ + ".") +
                 name.lexeme;
  if (result->is_data && match(lexer::TokenKind::LeftParen)) {
    std::vector<ast::ParameterPtr> parameters;
    std::vector<ast::BlockItemPtr> assignments;
    while (!check(lexer::TokenKind::RightParen) && !is_at_end()) {
      const auto field =
          consume(lexer::TokenKind::Identifier, "esperado nome do campo data");
      consume(lexer::TokenKind::Colon, "esperado ':' no campo data");
      auto type = parse_type();
      parameters.push_back(std::make_unique<ast::Parameter>(
          field.lexeme, clone_type(type.get()), field.span));
      auto stored = std::make_unique<ast::VariableDeclaration>(
          ast::Mutability::immutable, field.lexeme, std::move(type), nullptr,
          field.span);
      stored->modifiers.visibility = ast::Visibility::private_;
      stored->modifiers.sealed_ = true;
      result->members.push_back(std::move(stored));
      auto target = std::make_unique<ast::MemberAccessExpression>(
          std::make_unique<ast::IdentifierExpression>("this", field.span),
          field.lexeme, false, field.span);
      assignments.push_back(std::make_unique<ast::ExpressionStatement>(
          std::make_unique<ast::AssignmentExpression>(
              std::move(target), ast::AssignmentOperator::assign,
              std::make_unique<ast::IdentifierExpression>(field.lexeme,
                                                          field.span),
              field.span),
          field.span));
      if (!match(lexer::TokenKind::Comma))
        break;
    }
    consume(lexer::TokenKind::RightParen, "esperado ')' nos campos data");
    auto constructor = std::make_unique<ast::FunctionDeclaration>(
        "construct", std::move(parameters), nullptr,
        std::make_unique<ast::BlockStatement>(std::move(assignments),
                                              keyword.span),
        keyword.span);
    constructor->modifiers.visibility = ast::Visibility::public_;
    constructor->package_name = package_name_;
    result->members.push_back(std::move(constructor));
  }
  bool seen_base = false, seen_by = false, seen_permited = false,
       seen_with = false;
  for (;;) {
    std::vector<std::string> *names = nullptr;
    if (match(lexer::TokenKind::Colon)) {
      if (seen_base)
        report(previous(), "cláusula de herança repetida");
      seen_base = true;
      names = &result->bases;
    } else if (match(lexer::TokenKind::KwBy)) {
      if (seen_by)
        report(previous(), "cláusula by repetida");
      seen_by = true;
      names = &result->interfaces;
    } else if (match(lexer::TokenKind::KwWith)) {
      if (seen_with)
        report(previous(), "cláusula with repetida");
      seen_with = true;
      names = &result->components;
    } else if (match(lexer::TokenKind::KwPermited)) {
      if (seen_permited)
        report(previous(), "cláusula permited repetida");
      seen_permited = true;
      names = &result->permited;
    } else if (check(lexer::TokenKind::Identifier) &&
               current().lexeme == "permitted") {
      report(advance(), "use permited, não permitted");
      names = &result->permited;
    } else
      break;
    do {
      names->push_back(parse_qualified_name());
    } while (match(lexer::TokenKind::Comma));
  }
  for (const auto &interface : result->interfaces)
    result->bases.push_back(interface);
  if (result->is_data && !check(lexer::TokenKind::Colon) &&
      !check(lexer::TokenKind::LeftBrace)) {
    consume_statement_terminator("data");
    result->span.end = previous().span.end;
    return result;
  }
  consume(lexer::TokenKind::LeftBrace, "esperado '{' após o tipo");
  const bool previous_interface = in_interface_;
  const bool previous_class = in_class_;
  in_class_ = true;
  const auto previous_enclosing = enclosing_type_;
  enclosing_type_ = result->name;
  in_interface_ = result->is_interface;
  while (!check(lexer::TokenKind::RightBrace) && !is_at_end()) {
    if (match(lexer::TokenKind::Semicolon))
      continue;
    const auto before = index_;
    auto modifiers = parse_modifiers();
    ast::BlockItemPtr member;
    if (result->is_data && check(lexer::TokenKind::Identifier) &&
        peek(1).kind == lexer::TokenKind::Colon) {
      const auto component = advance();
      advance();
      auto type = parse_type();
      auto field = std::make_unique<ast::VariableDeclaration>(
          ast::Mutability::immutable, component.lexeme, std::move(type),
          nullptr, component.span);
      if (!modifiers.explicit_visibility)
        modifiers.visibility = ast::Visibility::private_;
      modifiers.sealed_ = true;
      bare_components.push_back(field.get());
      member = std::move(field);
      consume_statement_terminator("componente de data");
    } else if (match(lexer::TokenKind::KwVar))
      member = parse_variable_declaration(previous());
    else if (match(lexer::TokenKind::KwFunction))
      member = parse_function_declaration(previous());
    else if (match({lexer::TokenKind::KwClass, lexer::TokenKind::KwInterface,
                    lexer::TokenKind::KwData}))
      member = parse_class_declaration(previous());
    else if (match(lexer::TokenKind::KwConstruct)) {
      const auto token = previous();
      member = parse_function_declaration(token);
    } else if (check(lexer::TokenKind::Identifier) &&
               current().lexeme == "init" &&
               peek(1).kind == lexer::TokenKind::LeftParen) {
      report(current(),
             "init foi removido; declare o construtor com construct(...)");
      skip_unsupported_construct();
    } else {
      report(current(),
             "esperado campo, propriedade, método ou construct no tipo");
      skip_unsupported_construct();
    }
    if (member) {
      auto *declaration = static_cast<ast::Declaration *>(member.release());
      declaration->modifiers = modifiers;
      declaration->package_name = package_name_;
      if (result->is_data &&
          dynamic_cast<ast::VariableDeclaration *>(declaration))
        declaration->modifiers.sealed_ = true;
      if (!modifiers.explicit_visibility) {
        if (result->is_data &&
            dynamic_cast<ast::VariableDeclaration *>(declaration))
          declaration->modifiers.visibility = ast::Visibility::private_;
        else if (result->is_interface ||
                 dynamic_cast<ast::ClassDeclaration *>(declaration))
          declaration->modifiers.visibility = ast::Visibility::public_;
        else if (const auto *method =
                     dynamic_cast<ast::FunctionDeclaration *>(declaration);
                 method && method->name != "construct")
          declaration->modifiers.visibility = ast::Visibility::private_;
      }
      if (auto *nested = dynamic_cast<ast::ClassDeclaration *>(declaration);
          nested && nested->is_data)
        nested->modifiers.final_ = true;
      result->members.emplace_back(declaration);
    }
    if (index_ == before && !is_at_end())
      advance();
  }
  if (!bare_components.empty()) {
    std::vector<ast::ParameterPtr> parameters;
    std::vector<ast::BlockItemPtr> assignments;
    for (const auto *field : bare_components) {
      parameters.push_back(std::make_unique<ast::Parameter>(
          field->name, clone_type(field->type.get()), field->span));
      auto target = std::make_unique<ast::MemberAccessExpression>(
          std::make_unique<ast::IdentifierExpression>("this", field->span),
          field->name, false, field->span);
      assignments.push_back(std::make_unique<ast::ExpressionStatement>(
          std::make_unique<ast::AssignmentExpression>(
              std::move(target), ast::AssignmentOperator::assign,
              std::make_unique<ast::IdentifierExpression>(field->name,
                                                          field->span),
              field->span),
          field->span));
    }
    auto constructor = std::make_unique<ast::FunctionDeclaration>(
        "construct", std::move(parameters), nullptr,
        std::make_unique<ast::BlockStatement>(std::move(assignments),
                                              result->span),
        result->span);
    constructor->modifiers.visibility = ast::Visibility::public_;
    constructor->package_name = package_name_;
    result->members.push_back(std::move(constructor));
  }
  in_interface_ = previous_interface;
  in_class_ = previous_class;
  enclosing_type_ = previous_enclosing;
  result->span.end =
      consume(lexer::TokenKind::RightBrace, "esperado '}' após membros")
          .span.end;
  return result;
}

bool Parser::starts_lambda() const {
  if (!check(lexer::TokenKind::LeftParen))
    return false;
  std::size_t depth = 0;
  for (std::size_t i = 0; index_ + i < tokens_.size(); ++i) {
    const auto kind = peek(i).kind;
    if (kind == lexer::TokenKind::LeftParen)
      ++depth;
    if (kind == lexer::TokenKind::RightParen && depth > 0 && --depth == 0)
      return peek(i + 1).kind == lexer::TokenKind::FatArrow ||
             peek(i + 1).kind == lexer::TokenKind::Arrow;
    if (kind == lexer::TokenKind::EndOfFile)
      break;
  }
  return false;
}

ast::ExpressionPtr Parser::parse_lambda() {
    const auto opening = advance();
    auto lambda = std::make_unique<ast::LambdaExpression>(opening.span);
    while (!check(lexer::TokenKind::RightParen) && !is_at_end()) {
        const auto name = consume(lexer::TokenKind::Identifier, "esperado parâmetro da lambda");
        ast::TypeSyntaxPtr type;
        if (match(lexer::TokenKind::Colon)) type = parse_type();
        auto parameter = std::make_unique<ast::Parameter>(name.lexeme, std::move(type), name.span);
        parameter->external_label.clear();
        lambda->parameters.push_back(std::move(parameter));
        if (!match(lexer::TokenKind::Comma)) break;
    }
    consume(lexer::TokenKind::RightParen, "esperado ')' após parâmetros da lambda");
    if (match(lexer::TokenKind::Arrow)) lambda->return_type = parse_type();
    consume(lexer::TokenKind::FatArrow, "esperado '=>' após assinatura da lambda");
    if (match(lexer::TokenKind::LeftBrace)) {
        lambda->body = parse_block_statement(previous());
        lambda->span.end = lambda->body->span.end;
    } else {
        lambda->expression_body = parse_expression(assignment_precedence);
        lambda->span.end = lambda->expression_body->span.end;
    }
    return lambda;
}

ast::BlockItemPtr Parser::parse_variable_declaration(
    const lexer::Token& keyword
) {
    const auto mutability = match(lexer::TokenKind::KwMut)
        ? ast::Mutability::mutable_
        : ast::Mutability::immutable;

    lexer::Token name;
    if (check(lexer::TokenKind::IntegerLiteral) || check(lexer::TokenKind::FloatLiteral)) {
        name = advance();
        if (check(lexer::TokenKind::Identifier) &&
            name.span.end.offset == current().span.begin.offset) {
            const auto suffix = advance();
            name.lexeme += suffix.lexeme;
            name.span.end = suffix.span.end;
        }
        diagnostics_.emit(diagnostics::catalog::parser::malformed_identifier,
                          name.span, "identificador não pode começar por número");
    } else if (is_reserved_keyword(current().kind)) {
        name = advance();
        diagnostics_.emit(diagnostics::catalog::parser::reserved_identifier,
                          name.span, "palavra reservada '" + name.lexeme + "' não pode ser nome de variável");
    } else {
        name = consume(lexer::TokenKind::Identifier,
                       "esperado nome após 'var' ou 'var mut'");
    }

    ast::TypeSyntaxPtr type;
    if (match(lexer::TokenKind::Colon)) type = parse_type();
    ast::ExpressionPtr initializer;

    if (match(lexer::TokenKind::Equal)) {
        initializer = parse_expression();
    }
    if (!type && !initializer) report(current(), "declaração sem tipo exige inicializador");

    auto declaration = std::make_unique<ast::VariableDeclaration>(
        mutability, name.lexeme, std::move(type), std::move(initializer),
        span_from(keyword.span.begin, previous().span.end));
    if (check(lexer::TokenKind::LeftBrace)) {
        if (!in_class_) report(current(), "acessores só podem ser declarados em um tipo");
        parse_accessors(*declaration);
    } else {
        consume_statement_terminator("a declaração de variável");
        declaration->span.end = previous().span.end;
    }
    return declaration;
}

void Parser::parse_accessors(ast::VariableDeclaration& property) {
    property.is_property = true;
    advance(); // {
    if (!property.type) report(current(), "propriedade exige tipo explícito");
    if (property.initializer) report(current(), "propriedade computada não aceita inicializador; use um campo privado");
    while (!check(lexer::TokenKind::RightBrace) && !is_at_end()) {
        if (match(lexer::TokenKind::Semicolon)) continue;
        const bool getter = check(lexer::TokenKind::KwGetter);
        if (!match({lexer::TokenKind::KwGetter, lexer::TokenKind::KwSetter})) {
            report(current(), "esperado getter() ou setter() na propriedade");
            skip_unsupported_construct();
            continue;
        }
        const auto keyword = previous();
        consume(lexer::TokenKind::LeftParen, "esperado '(' após acessor");
        std::vector<ast::ParameterPtr> parameters;
        if (!getter) {
            std::string name = "value";
            auto location = keyword.span;
            if (!check(lexer::TokenKind::RightParen)) {
                const auto parameter = consume(lexer::TokenKind::Identifier, "esperado nome do valor recebido");
                name = parameter.lexeme;
                location = parameter.span;
            }
            auto parameter = std::make_unique<ast::Parameter>(name, clone_type(property.type.get()), location);
            parameter->external_label.clear();
            parameters.push_back(std::move(parameter));
        }
        consume(lexer::TokenKind::RightParen, "esperado ')' após parâmetros do acessor");
        std::unique_ptr<ast::BlockStatement> body;
        if (!match(lexer::TokenKind::Semicolon)) body = parse_required_block("esperado corpo do acessor ou ';' em contrato");
        auto returned = getter ? clone_type(property.type.get())
            : std::make_unique<ast::TypeSyntax>("void", 0, keyword.span);
        auto accessor = std::make_unique<ast::FunctionDeclaration>(
            std::string(getter ? "$getter." : "$setter.") + property.name,
            std::move(parameters), std::move(returned), std::move(body),
            span_from(keyword.span.begin, previous().span.end));
        auto& destination = getter ? property.getter : property.setter;
        if (destination) report(keyword, "acessor duplicado na propriedade");
        else destination = std::move(accessor);
    }
    property.span.end = consume(lexer::TokenKind::RightBrace, "esperado '}' após acessores").span.end;
    if (!property.getter) report(previous(), "propriedade exige getter()");
    if (property.setter && property.mutability != ast::Mutability::mutable_)
        report(previous(), "setter exige propriedade var mut");
}


ast::BlockItemPtr Parser::parse_function_declaration(
    const lexer::Token& keyword
) {
    const lexer::Token name = keyword.kind == lexer::TokenKind::KwConstruct
        ? lexer::Token{.kind = lexer::TokenKind::Identifier, .lexeme = "construct", .span = keyword.span}
        : consume(lexer::TokenKind::Identifier, "esperado nome após 'function'");

    consume(
        lexer::TokenKind::LeftParen,
        "esperado '(' após o nome da função"
    );

    std::vector<ast::ParameterPtr> parameters;

    if (!check(lexer::TokenKind::RightParen)) {
        while (!is_at_end()) {
            const lexer::Token first_name = consume(
                lexer::TokenKind::Identifier,
                "esperado nome do parâmetro"
            );
            lexer::Token parameter_name = first_name;
            std::string external_label = first_name.lexeme == "_" ? "" : first_name.lexeme;
            if ((check(lexer::TokenKind::Identifier) ||
                 check(lexer::TokenKind::EscapedIdentifier)) &&
                peek(1).kind == lexer::TokenKind::Colon) {
                parameter_name = consume(lexer::TokenKind::Identifier, "esperado nome do parâmetro");
                external_label = first_name.lexeme == "_" ? "" : first_name.lexeme;
            }

            consume(
                lexer::TokenKind::Colon,
                "esperado ':' após o nome do parâmetro"
            );

            auto parameter_type = parse_type();
            const auto parameter_span = span_from(
                parameter_name.span.begin,
                parameter_type->span.end
            );

            auto parameter = std::make_unique<ast::Parameter>(
                parameter_name.lexeme,
                std::move(parameter_type),
                parameter_span
            );
            parameter->external_label = std::move(external_label);
            parameters.push_back(std::move(parameter));

            if (!match(lexer::TokenKind::Comma)) {
                break;
            }

            if (check(lexer::TokenKind::RightParen)) {
                break;
            }
        }
    }

    consume(
        lexer::TokenKind::RightParen,
        "esperado ')' após os parâmetros"
    );

    ast::TypeSyntaxPtr return_type;

    if (match(lexer::TokenKind::Arrow)) {
        return_type = parse_type();
    }

    std::unique_ptr<ast::BlockStatement> body;
    if (check(lexer::TokenKind::Semicolon)) advance();
    else if (in_interface_ && check(lexer::TokenKind::RightBrace)) {}
    else body = parse_simple_body("esperado corpo após a assinatura da função");

    const auto end = body ? body->span.end : previous().span.end;

    return std::make_unique<ast::FunctionDeclaration>(
        name.lexeme,
        std::move(parameters),
        std::move(return_type),
        std::move(body),
        span_from(keyword.span.begin, end)
    );
}

ast::StatementPtr Parser::parse_statement() {
    switch (current().kind) {
    case lexer::TokenKind::KwFor:
    case lexer::TokenKind::KwDo:
    case lexer::TokenKind::KwMatch:
    case lexer::TokenKind::KwDefer:
    case lexer::TokenKind::KwThrow:
    case lexer::TokenKind::KwTry:
      report(current(), "instrução ainda não suportada pelo frontend");
      skip_unsupported_construct();
      return nullptr;
    default: break;
    }
    if (match(lexer::TokenKind::LeftBrace)) {
        return parse_block_statement(previous());
    }

    if (match(lexer::TokenKind::KwReturn)) {
        return parse_return_statement(previous());
    }

    if (match(lexer::TokenKind::KwIf)) {
        return parse_if_statement(previous());
    }

    if (match(lexer::TokenKind::KwWhile)) {
        return parse_while_statement(previous());
    }

    if (match(lexer::TokenKind::KwBreak)) {
        return parse_break_statement(previous());
    }
    if (match(lexer::TokenKind::KwContinue)) return parse_continue_statement(previous());

    if (match(lexer::TokenKind::KwUnsafe)) {
        return parse_unsafe_statement(previous());
    }

    return parse_expression_statement();
}

ast::StatementPtr Parser::parse_break_statement(const lexer::Token& keyword) {
    consume_statement_terminator("'break'");
    return std::make_unique<ast::BreakStatement>(keyword.span);
}
ast::StatementPtr Parser::parse_continue_statement(const lexer::Token& keyword) {
    consume_statement_terminator("'continue'");
    return std::make_unique<ast::ContinueStatement>(keyword.span);
}

std::unique_ptr<ast::BlockStatement> Parser::parse_required_block(
    std::string message
) {
    if (match(lexer::TokenKind::LeftBrace)) {
        return parse_block_statement(previous());
    }

    report(current(), std::move(message));

    const auto location = current().span.begin;
    return std::make_unique<ast::BlockStatement>(
        std::vector<ast::BlockItemPtr>{},
        span_from(location, location)
    );
}

std::unique_ptr<ast::BlockStatement> Parser::parse_simple_body(
    std::string message
) {
    match(lexer::TokenKind::Semicolon);
    if (match(lexer::TokenKind::LeftBrace)) {
        return parse_block_statement(previous());
    }

    const auto begin = current().span.begin;
    if (is_at_end() || check(lexer::TokenKind::RightBrace)
        || check(lexer::TokenKind::KwElse)) {
        report(current(), std::move(message));
        return std::make_unique<ast::BlockStatement>(
            std::vector<ast::BlockItemPtr>{}, span_from(begin, begin)
        );
    }

    std::vector<ast::BlockItemPtr> items;
    auto item = parse_block_item();
    if (item != nullptr) {
        items.push_back(std::move(item));
    }
    return std::make_unique<ast::BlockStatement>(
        std::move(items), span_from(begin, previous().span.end)
    );
}

std::unique_ptr<ast::BlockStatement> Parser::parse_block_statement(
    const lexer::Token& left_brace
) {
    std::vector<ast::BlockItemPtr> items;

    while (
        !check(lexer::TokenKind::RightBrace)
        && !is_at_end()
    ) {
        if (match(lexer::TokenKind::Semicolon)) {
            continue;
        }

        const std::size_t index_before = index_;
        auto item = parse_block_item();

        if (item != nullptr) {
            items.push_back(std::move(item));
        }

        if (index_ == index_before && !is_at_end()) {
            report(current(), "construção inválida dentro do bloco");
            synchronize();
        }
    }

    source::SourceLocation end = current().span.begin;

    if (match(lexer::TokenKind::RightBrace)) {
        end = previous().span.end;
    } else {
        report(current(), "esperado '}' para encerrar o bloco");
    }

    return std::make_unique<ast::BlockStatement>(
        std::move(items),
        span_from(left_brace.span.begin, end)
    );
}

ast::StatementPtr Parser::parse_return_statement(
    const lexer::Token& keyword
) {
    ast::ExpressionPtr value;

    if (
        !check(lexer::TokenKind::Semicolon)
        && !check(lexer::TokenKind::RightBrace)
        && !is_at_end()
        && !has_line_break_before_current()
    ) {
        value = parse_expression();
    }

    consume_statement_terminator("'return'");

    return std::make_unique<ast::ReturnStatement>(
        std::move(value),
        span_from(keyword.span.begin, previous().span.end)
    );
}

ast::StatementPtr Parser::parse_if_statement(
    const lexer::Token& keyword
) {
    auto condition = parse_expression();
    auto then_branch = parse_simple_body(
        "esperado corpo após a condição do 'if'"
    );

    ast::StatementPtr else_branch;

    if (match(lexer::TokenKind::KwElse)) {
        if (match(lexer::TokenKind::KwIf)) {
            else_branch = parse_if_statement(previous());
        } else {
            else_branch = parse_simple_body(
                "esperado corpo ou 'if' após 'else'"
            );
        }
    }

    const auto end = else_branch != nullptr
        ? else_branch->span.end
        : then_branch->span.end;

    return std::make_unique<ast::IfStatement>(
        std::move(condition),
        std::move(then_branch),
        std::move(else_branch),
        span_from(keyword.span.begin, end)
    );
}

ast::StatementPtr Parser::parse_while_statement(
    const lexer::Token& keyword
) {
    auto condition = parse_expression();
    auto body = parse_simple_body(
        "esperado corpo após a condição do 'while'"
    );

    const auto end = body->span.end;

    return std::make_unique<ast::WhileStatement>(
        std::move(condition),
        std::move(body),
        span_from(keyword.span.begin, end)
    );
}

ast::StatementPtr Parser::parse_unsafe_statement(
    const lexer::Token& keyword
) {
    auto block = parse_required_block(
        "esperado '{' após 'unsafe'"
    );

    const auto end = block->span.end;

    return std::make_unique<ast::UnsafeBlockStatement>(
        std::move(block),
        span_from(keyword.span.begin, end)
    );
}

ast::StatementPtr Parser::parse_expression_statement() {
    auto expression = parse_expression();
    const auto begin = expression->span.begin;

    consume_statement_terminator("a expressão");

    return std::make_unique<ast::ExpressionStatement>(
        std::move(expression),
        span_from(begin, previous().span.end)
    );
}

ast::TypeSyntaxPtr Parser::parse_type() {
  if (match(lexer::TokenKind::LeftBracket)) {
    const auto begin = previous().span.begin;
    auto type = std::make_unique<ast::TypeSyntax>("array", 0, previous().span);
    type->element_type = parse_type();
    consume(lexer::TokenKind::RightBracket, "esperado ']' no tipo de array");
    while (check(lexer::TokenKind::Question) ||
           check(lexer::TokenKind::NullCoalescing)) {
      type->nullable_depth +=
          advance().kind == lexer::TokenKind::NullCoalescing ? 2 : 1;
    }
    type->span = span_from(begin, previous().span.end);
    return type;
  }
    if (match(lexer::TokenKind::LeftParen)) {
        const auto begin = previous().span.begin;
        auto type = std::make_unique<ast::TypeSyntax>("function", 0, previous().span);
        while (!check(lexer::TokenKind::RightParen) && !is_at_end()) {
            const auto before = index_;
            type->parameter_types.push_back(parse_type());
            if (index_ == before || !match(lexer::TokenKind::Comma)) break;
        }
        consume(lexer::TokenKind::RightParen, "esperado ')' no tipo de função");
        if (!check(lexer::TokenKind::Arrow) &&
            type->parameter_types.size() == 1) {
          auto grouped = std::move(type->parameter_types.front());
          while (check(lexer::TokenKind::Question) ||
                 check(lexer::TokenKind::NullCoalescing))
            grouped->nullable_depth +=
                advance().kind == lexer::TokenKind::NullCoalescing ? 2 : 1;
          grouped->span = span_from(begin, previous().span.end);
          return grouped;
        }
        consume(lexer::TokenKind::Arrow, "esperado '->' no tipo de função");
        type->result_type = parse_type();
        type->span = span_from(begin, type->result_type->span.end);
        return type;
    }
    lexer::Token name = synthetic_token(lexer::TokenKind::Identifier);
    if (is_type_token(current().kind)) {
        name = current().kind == lexer::TokenKind::EscapedIdentifier
                   ? consume(lexer::TokenKind::Identifier, "esperado nome de tipo")
                   : advance();
    }
    else report(current(), "esperado nome de tipo");

    while (match(lexer::TokenKind::Dot)) {
      name.lexeme += "." + consume(lexer::TokenKind::Identifier,
                                   "esperado nome após '.' no tipo")
                               .lexeme;
      name.span.end = previous().span.end;
    }
    std::size_t nullable_depth = 0;
    source::SourceLocation end = name.span.end;

    while (check(lexer::TokenKind::Question) || check(lexer::TokenKind::NullCoalescing)) {
        nullable_depth += check(lexer::TokenKind::NullCoalescing) ? 2 : 1;
        advance();
        end = previous().span.end;
    }

    const std::string type_name = name.lexeme.empty()
        ? "<error>"
        : name.lexeme;

    return std::make_unique<ast::TypeSyntax>(
        type_name,
        nullable_depth,
        span_from(name.span.begin, end)
    );
}

ast::ExpressionPtr Parser::parse_expression(
    std::uint8_t minimum_binding_power
) {
    auto left = parse_prefix_expression();

    while (!is_at_end()) {
      if ((check(lexer::TokenKind::LeftBracket) ||
           check(lexer::TokenKind::QuestionLeftBracket)) &&
          postfix_precedence >= minimum_binding_power) {
        const bool safe =
            advance().kind == lexer::TokenKind::QuestionLeftBracket;
        auto index = parse_expression();
        const auto end =
            consume(lexer::TokenKind::RightBracket, "esperado ']' após índice");
        const auto span = span_from(left->span.begin, end.span.end);
        left = std::make_unique<ast::IndexExpression>(
            std::move(left), std::move(index), safe, span);
        continue;
      }
        if (
            check(lexer::TokenKind::LeftParen)
            && postfix_precedence >= minimum_binding_power
        ) {
            advance();
            left = finish_call(std::move(left));
            continue;
        }

        if (
            (check(lexer::TokenKind::Dot)
             || check(lexer::TokenKind::QuestionDot))
            && postfix_precedence >= minimum_binding_power
        ) {
            const lexer::Token operation = advance();
            left = finish_member_access(
                std::move(left),
                operation
            );
            continue;
        }

        if (
            is_postfix_operator(current().kind)
            && postfix_precedence >= minimum_binding_power
        ) {
            const lexer::Token operation = advance();
            left = finish_postfix(std::move(left), operation);
            continue;
        }

        const auto binding = infix_binding_power(current().kind);

        if (
            !binding.has_value()
            || binding->left < minimum_binding_power
        ) {
            break;
        }

        const lexer::Token operation = advance();
        auto right = parse_expression(binding->right);
        const auto expression_span = span_from(
            left->span.begin,
            right->span.end
        );

        if (is_assignment_operator(operation.kind)) {
            left = std::make_unique<ast::AssignmentExpression>(
                std::move(left),
                assignment_operator(operation.kind),
                std::move(right),
                expression_span
            );
        } else {
            left = std::make_unique<ast::BinaryExpression>(
                std::move(left),
                binary_operator(operation.kind),
                std::move(right),
                expression_span
            );
        }
    }

    return left;
}

ast::ExpressionPtr Parser::parse_prefix_expression() {
    if (
        match({
            lexer::TokenKind::Plus,
            lexer::TokenKind::Minus,
            lexer::TokenKind::Bang,
        })
    ) {
        const lexer::Token operation = previous();
        auto operand = parse_expression(prefix_precedence);
        const auto span = span_from(
            operation.span.begin,
            operand->span.end
        );

        return std::make_unique<ast::UnaryExpression>(
            unary_operator(operation.kind),
            std::move(operand),
            span
        );
    }

    return parse_primary_expression();
}

ast::ExpressionPtr Parser::parse_primary_expression() {
    if (starts_lambda()) return parse_lambda();
    if (match(lexer::TokenKind::LeftBracket)) {
      const auto begin = previous().span.begin;
      std::vector<ast::ExpressionPtr> elements;
      while (!check(lexer::TokenKind::RightBracket) && !is_at_end()) {
        elements.push_back(parse_expression());
        if (!match(lexer::TokenKind::Comma))
          break;
      }
      const auto end =
          consume(lexer::TokenKind::RightBracket, "esperado ']' após array");
      return std::make_unique<ast::ArrayExpression>(
          std::move(elements), span_from(begin, end.span.end));
    }
    if (match(lexer::TokenKind::KwNew)) {
        auto node = std::make_unique<ast::NewExpression>(previous().span);
        node->type = parse_type();
        consume(lexer::TokenKind::LeftParen, "esperado '(' após o tipo em 'new'");
        auto call = finish_call(std::make_unique<ast::IdentifierExpression>(
            node->type->name, node->type->span));
        node->span.end = call->span.end;
        node->call.reset(static_cast<ast::CallExpression*>(call.release()));
        return node;
    }
    if (match({lexer::TokenKind::KwThis, lexer::TokenKind::KwSuper})) {
      const lexer::Token token = previous();
      return std::make_unique<ast::IdentifierExpression>(
          token.kind == lexer::TokenKind::KwSuper ? "super" : "this",
          token.span);
    }

    if (match({lexer::TokenKind::Identifier, lexer::TokenKind::EscapedIdentifier})) {
        const lexer::Token token = previous();
        return std::make_unique<ast::IdentifierExpression>(
            token.kind == lexer::TokenKind::EscapedIdentifier
                ? token.lexeme.substr(1, token.lexeme.size() - 2) : token.lexeme,
            token.span
        );
    }

    if (match(lexer::TokenKind::IntegerLiteral)) {
        const lexer::Token token = previous();
        return std::make_unique<ast::IntegerLiteralExpression>(
            token.lexeme,
            token.span
        );
    }

    if (match(lexer::TokenKind::FloatLiteral)) {
        const lexer::Token token = previous();
        return std::make_unique<ast::FloatLiteralExpression>(token.lexeme, token.span);
    }

    if (match(lexer::TokenKind::CharacterLiteral)) {
        const lexer::Token token = previous();
        return std::make_unique<ast::CharacterLiteralExpression>(token.lexeme, token.span);
    }

    if (match({lexer::TokenKind::StringLiteral, lexer::TokenKind::MultilineStringLiteral})) {
        const lexer::Token token = previous();
        return std::make_unique<ast::StringLiteralExpression>(
            token.lexeme,
            token.span
        );
    }

    if (match(lexer::TokenKind::KwTrue)) {
        const lexer::Token token = previous();
        return std::make_unique<ast::BooleanLiteralExpression>(
            true,
            token.span
        );
    }

    if (match(lexer::TokenKind::KwFalse)) {
        const lexer::Token token = previous();
        return std::make_unique<ast::BooleanLiteralExpression>(
            false,
            token.span
        );
    }

    if (match(lexer::TokenKind::KwNull)) {
        if (previous().lexeme == "null") {
            diagnostics_.emit(diagnostics::catalog::warning::legacy_null_keyword,
                              previous().span);
        }
        return std::make_unique<ast::NullLiteralExpression>(
            previous().span
        );
    }

    if (match(lexer::TokenKind::LeftParen)) {
        const lexer::Token left_parenthesis = previous();
        auto expression = parse_expression();
        const lexer::Token right_parenthesis = consume(
            lexer::TokenKind::RightParen,
            "esperado ')' após a expressão"
        );

        return std::make_unique<ast::GroupingExpression>(
            std::move(expression),
            span_from(
                left_parenthesis.span.begin,
                right_parenthesis.span.end
            )
        );
    }

    const lexer::Token token = current();
    report(token, "esperada uma expressão");

    if (!is_at_end()) {
        advance();
    }

    return std::make_unique<ast::ErrorExpression>(token.span);
}

ast::ExpressionPtr Parser::finish_call(ast::ExpressionPtr callee) {
    std::vector<ast::ExpressionPtr> arguments;
    std::vector<std::string> labels;
    bool saw_named = false;

    if (!check(lexer::TokenKind::RightParen)) {
        while (!is_at_end()) {
            std::string label;
            if ((check(lexer::TokenKind::Identifier) ||
                 check(lexer::TokenKind::EscapedIdentifier)) &&
                peek(1).kind == lexer::TokenKind::Colon) {
                label = consume(lexer::TokenKind::Identifier, "esperado rótulo").lexeme;
                advance();
                saw_named = true;
            } else if (saw_named) {
                report(current(), "argumentos posicionais devem preceder argumentos nomeados");
            }
            labels.push_back(std::move(label));
            arguments.push_back(parse_expression());

            if (!match(lexer::TokenKind::Comma)) {
                break;
            }

            if (check(lexer::TokenKind::RightParen)) {
                break;
            }
        }
    }

    const lexer::Token right_parenthesis = consume(
        lexer::TokenKind::RightParen,
        "esperado ')' após os argumentos"
    );

    const auto span = span_from(
        callee->span.begin,
        right_parenthesis.span.end
    );

    auto call = std::make_unique<ast::CallExpression>(
        std::move(callee),
        std::move(arguments),
        span
    );
    call->argument_labels = std::move(labels);
    return call;
}

ast::ExpressionPtr Parser::finish_member_access(
    ast::ExpressionPtr base,
    const lexer::Token& operation
) {
    const lexer::Token member = consume(
        lexer::TokenKind::Identifier,
        "esperado nome de membro após '.' ou '?.'"
    );

    const auto span = span_from(base->span.begin, member.span.end);

    return std::make_unique<ast::MemberAccessExpression>(
        std::move(base),
        member.lexeme,
        operation.kind == lexer::TokenKind::QuestionDot,
        span
    );
}

ast::ExpressionPtr Parser::finish_postfix(
    ast::ExpressionPtr operand,
    const lexer::Token& operation
) {
    const auto span = span_from(
        operand->span.begin,
        operation.span.end
    );

    return std::make_unique<ast::PostfixExpression>(
        std::move(operand),
        postfix_operator(operation.kind),
        span
    );
}

std::optional<Parser::BindingPower> Parser::infix_binding_power(
    lexer::TokenKind kind
) noexcept {
    switch (kind) {
        case lexer::TokenKind::Equal:
        case lexer::TokenKind::PlusEqual:
        case lexer::TokenKind::MinusEqual:
        case lexer::TokenKind::StarEqual:
        case lexer::TokenKind::SlashEqual:
        case lexer::TokenKind::PercentEqual:
        case lexer::TokenKind::ForceUnwrapAssign:
        case lexer::TokenKind::SafeAssign:
        case lexer::TokenKind::NullCoalescingAssign:
            return BindingPower{
                assignment_precedence,
                assignment_precedence,
            };

        case lexer::TokenKind::PipePipe:
            return BindingPower{
                logical_or_precedence,
                static_cast<std::uint8_t>(logical_or_precedence + 1),
            };

        case lexer::TokenKind::AmpAmp:
            return BindingPower{
                logical_and_precedence,
                static_cast<std::uint8_t>(logical_and_precedence + 1),
            };

        case lexer::TokenKind::NullCoalescing:
            return BindingPower{
                coalescing_precedence,
                coalescing_precedence,
            };

        case lexer::TokenKind::EqualEqual:
        case lexer::TokenKind::BangEqual:
        case lexer::TokenKind::SafeEqualityAssign:
            return BindingPower{
                equality_precedence,
                static_cast<std::uint8_t>(equality_precedence + 1),
            };

        case lexer::TokenKind::Less:
        case lexer::TokenKind::LessEqual:
        case lexer::TokenKind::Greater:
        case lexer::TokenKind::GreaterEqual:
            return BindingPower{
                relational_precedence,
                static_cast<std::uint8_t>(relational_precedence + 1),
            };

        case lexer::TokenKind::Plus:
        case lexer::TokenKind::Minus:
            return BindingPower{
                additive_precedence,
                static_cast<std::uint8_t>(additive_precedence + 1),
            };

        case lexer::TokenKind::Star:
        case lexer::TokenKind::Slash:
        case lexer::TokenKind::Percent:
            return BindingPower{
                multiplicative_precedence,
                static_cast<std::uint8_t>(multiplicative_precedence + 1),
            };

        default:
            return std::nullopt;
    }
}

ast::UnaryOperator Parser::unary_operator(
    lexer::TokenKind kind
) noexcept {
    switch (kind) {
        case lexer::TokenKind::Plus:
            return ast::UnaryOperator::plus;
        case lexer::TokenKind::Minus:
            return ast::UnaryOperator::minus;
        case lexer::TokenKind::Bang:
            return ast::UnaryOperator::not_;
        default:
            assert(false && "token não representa operador unário");
            return ast::UnaryOperator::plus;
    }
}

ast::BinaryOperator Parser::binary_operator(
    lexer::TokenKind kind
) noexcept {
    switch (kind) {
        case lexer::TokenKind::Star:
            return ast::BinaryOperator::multiply;
        case lexer::TokenKind::Slash:
            return ast::BinaryOperator::divide;
        case lexer::TokenKind::Percent:
            return ast::BinaryOperator::modulo;
        case lexer::TokenKind::Plus:
            return ast::BinaryOperator::add;
        case lexer::TokenKind::Minus:
            return ast::BinaryOperator::subtract;
        case lexer::TokenKind::EqualEqual:
            return ast::BinaryOperator::equal;
        case lexer::TokenKind::BangEqual:
            return ast::BinaryOperator::not_equal;
        case lexer::TokenKind::Less:
            return ast::BinaryOperator::less_than;
        case lexer::TokenKind::LessEqual:
            return ast::BinaryOperator::less_than_or_equal;
        case lexer::TokenKind::Greater:
            return ast::BinaryOperator::greater_than;
        case lexer::TokenKind::GreaterEqual:
            return ast::BinaryOperator::greater_than_or_equal;
        case lexer::TokenKind::AmpAmp:
            return ast::BinaryOperator::logical_and;
        case lexer::TokenKind::PipePipe:
            return ast::BinaryOperator::logical_or;
        case lexer::TokenKind::SafeEqualityAssign:
            return ast::BinaryOperator::lifted_equal;
        case lexer::TokenKind::NullCoalescing:
            return ast::BinaryOperator::null_coalescing;
        default:
            assert(false && "token não representa operador binário");
            return ast::BinaryOperator::add;
    }
}

ast::AssignmentOperator Parser::assignment_operator(
    lexer::TokenKind kind
) noexcept {
    switch (kind) {
        case lexer::TokenKind::Equal:
            return ast::AssignmentOperator::assign;
        case lexer::TokenKind::PlusEqual: return ast::AssignmentOperator::add_assign;
        case lexer::TokenKind::MinusEqual: return ast::AssignmentOperator::subtract_assign;
        case lexer::TokenKind::StarEqual: return ast::AssignmentOperator::multiply_assign;
        case lexer::TokenKind::SlashEqual: return ast::AssignmentOperator::divide_assign;
        case lexer::TokenKind::PercentEqual: return ast::AssignmentOperator::modulo_assign;
        case lexer::TokenKind::ForceUnwrapAssign:
          return ast::AssignmentOperator::force_unwrap_assign;
        case lexer::TokenKind::SafeAssign:
            return ast::AssignmentOperator::conditional_assign;
        case lexer::TokenKind::NullCoalescingAssign:
            return ast::AssignmentOperator::null_coalescing_assign;
        default:
            assert(false && "token não representa operador de atribuição");
            return ast::AssignmentOperator::assign;
    }
}

ast::PostfixOperator Parser::postfix_operator(
    lexer::TokenKind kind
) noexcept {
    switch (kind) {
        case lexer::TokenKind::ForceUnwrap:
            return ast::PostfixOperator::force_unwrap;
        case lexer::TokenKind::PlusPlus: return ast::PostfixOperator::increment;
        case lexer::TokenKind::MinusMinus: return ast::PostfixOperator::decrement;
        default:
            assert(false && "token não representa operador postfix");
            return ast::PostfixOperator::propagate;
    }
}

source::SourceSpan Parser::span_from(
    const source::SourceLocation& begin,
    const source::SourceLocation& end
) noexcept {
    return source::SourceSpan{
        .begin = begin,
        .end = end,
    };
}

lexer::Token Parser::synthetic_token(lexer::TokenKind kind) const {
    const auto location = current().span.begin;

    return lexer::Token{
        .kind = kind,
        .lexeme = {},
        .span = span_from(location, location),
    };
}

} // namespace argos::parser
