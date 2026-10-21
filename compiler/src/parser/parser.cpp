#include "argos/parser/parser.hpp"

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

bool is_assignment_operator(lexer::TokenKind kind) noexcept {
    return kind == lexer::TokenKind::Equal
        || kind == lexer::TokenKind::PlusEqual
        || kind == lexer::TokenKind::MinusEqual
        || kind == lexer::TokenKind::StarEqual
        || kind == lexer::TokenKind::SlashEqual
        || kind == lexer::TokenKind::PercentEqual
        || kind == lexer::TokenKind::SafeAssign
        || kind == lexer::TokenKind::NullCoalescingAssign;
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

ast::Program Parser::parse_program() {
    std::vector<ast::BlockItemPtr> items;

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

    return ast::Program{
        std::move(items),
        span_from(begin, end),
    };
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
    diagnostics_.error(
        "ARGOS-P0001",
        std::move(message),
        token.span
    );
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
    if (match(lexer::TokenKind::KwVar)) {
        return parse_variable_declaration(previous());
    }

    if (match(lexer::TokenKind::KwFunction)) {
        return parse_function_declaration(previous());
    }
    if (match(lexer::TokenKind::KwProtocol)) {
        return parse_protocol_declaration(previous());
    }

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
    case lexer::TokenKind::KwInit:
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

    return parse_statement();
}

ast::BlockItemPtr Parser::parse_variable_declaration(
    const lexer::Token& keyword
) {
    const auto mutability = match(lexer::TokenKind::KwMut)
        ? ast::Mutability::mutable_
        : ast::Mutability::immutable;

    const lexer::Token name = consume(
        lexer::TokenKind::Identifier,
        "esperado nome após 'var' ou 'var mut'"
    );

    ast::TypeSyntaxPtr type;
    if (match(lexer::TokenKind::Colon)) type = parse_type();
    ast::ExpressionPtr initializer;

    if (match(lexer::TokenKind::Equal)) {
        initializer = parse_expression();
    }
    if (!type && !initializer) report(current(), "declaração sem tipo exige inicializador");

    consume_statement_terminator("a declaração de variável");

    const auto end = previous().span.end;

    return std::make_unique<ast::VariableDeclaration>(
        mutability,
        name.lexeme,
        std::move(type),
        std::move(initializer),
        span_from(keyword.span.begin, end)
    );
}

ast::BlockItemPtr Parser::parse_function_declaration(
    const lexer::Token& keyword
) {
    const lexer::Token name = consume(
        lexer::TokenKind::Identifier,
        "esperado nome após 'function'"
    );

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

    auto body = parse_simple_body(
        "esperado corpo após a assinatura da função"
    );

    const auto end = body->span.end;

    return std::make_unique<ast::FunctionDeclaration>(
        name.lexeme,
        std::move(parameters),
        std::move(return_type),
        std::move(body),
        span_from(keyword.span.begin, end)
    );
}

ast::BlockItemPtr Parser::parse_protocol_declaration(const lexer::Token& keyword) {
    const auto name = consume(lexer::TokenKind::Identifier,
                              "esperado nome após 'protocol'");
    if (check(lexer::TokenKind::Less)) {
        report(current(), "protocol genérico ainda não é suportado");
        skip_unsupported_construct();
        return nullptr;
    }
    consume(lexer::TokenKind::LeftParen, "esperado '(' após o nome do protocol");
    std::vector<std::unique_ptr<ast::VariableDeclaration>> components;
    while (!check(lexer::TokenKind::RightParen) && !is_at_end()) {
        const auto component_name = consume(lexer::TokenKind::Identifier,
                                             "esperado nome do componente");
        consume(lexer::TokenKind::Colon, "esperado ':' após o componente");
        auto type = parse_type();
        ast::ExpressionPtr initializer;
        if (match(lexer::TokenKind::Equal)) initializer = parse_expression();
        const auto end = initializer ? initializer->span.end : type->span.end;
        components.push_back(std::make_unique<ast::VariableDeclaration>(
            ast::Mutability::immutable, component_name.lexeme, std::move(type),
            std::move(initializer), span_from(component_name.span.begin, end)));
        if (!match(lexer::TokenKind::Comma)) break;
    }
    const auto closing = consume(lexer::TokenKind::RightParen,
                                  "esperado ')' após componentes do protocol");
    if (check(lexer::TokenKind::Colon) || check(lexer::TokenKind::KwType)) {
        report(current(), "conformidade e constraints de protocol ainda não são suportadas");
        skip_unsupported_construct();
        return nullptr;
    }
    source::SourceLocation end = closing.span.end;
    if (match(lexer::TokenKind::LeftBrace)) {
        if (!check(lexer::TokenKind::RightBrace)) {
            report(current(), "membros no corpo de protocol ainda não são suportados");
            while (!check(lexer::TokenKind::RightBrace) && !is_at_end()) advance();
        }
        end = consume(lexer::TokenKind::RightBrace,
                      "esperado '}' após o corpo do protocol").span.end;
    } else {
        consume_statement_terminator("declaração de protocol");
        end = previous().span.end;
    }
    return std::make_unique<ast::ProtocolDeclaration>(
        name.lexeme, std::move(components), span_from(keyword.span.begin, end));
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
    lexer::Token name = synthetic_token(lexer::TokenKind::Identifier);
    if (is_type_token(current().kind)) {
        name = current().kind == lexer::TokenKind::EscapedIdentifier
                   ? consume(lexer::TokenKind::Identifier, "esperado nome de tipo")
                   : advance();
    }
    else report(current(), "esperado nome de tipo");

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
