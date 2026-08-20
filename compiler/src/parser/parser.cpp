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
constexpr std::uint8_t logical_or_precedence = 15;
constexpr std::uint8_t logical_and_precedence = 18;
constexpr std::uint8_t coalescing_precedence = 20;
constexpr std::uint8_t equality_precedence = 30;
constexpr std::uint8_t relational_precedence = 35;
constexpr std::uint8_t additive_precedence = 40;
constexpr std::uint8_t multiplicative_precedence = 50;
constexpr std::uint8_t prefix_precedence = 60;
constexpr std::uint8_t postfix_precedence = 70;

bool is_assignment_operator(lexer::TokenKind kind) noexcept {
    return kind == lexer::TokenKind::Equal
        || kind == lexer::TokenKind::SafeAssign
        || kind == lexer::TokenKind::NullCoalescingAssign;
}

bool is_postfix_operator(lexer::TokenKind kind) noexcept {
    return kind == lexer::TokenKind::Question
        || kind == lexer::TokenKind::Bang
        || kind == lexer::TokenKind::ForceUnwrap;
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
            case lexer::TokenKind::KwLet:
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

void Parser::consume_statement_terminator(std::string context) {
    if (match(lexer::TokenKind::Semicolon)) {
        return;
    }

    if (can_finish_statement()) {
        return;
    }

    report(
        current(),
        "esperado ';' ou quebra de linha após " + std::move(context)
    );
}

bool Parser::has_line_break_before_current() const noexcept {
    if (index_ == 0) {
        return false;
    }

    return previous().span.end.line < current().span.begin.line;
}

bool Parser::can_finish_statement() const noexcept {
    return is_at_end()
        || check(lexer::TokenKind::RightBrace)
        || has_line_break_before_current();
}

ast::BlockItemPtr Parser::parse_block_item() {
    if (match(lexer::TokenKind::KwLet)) {
        return parse_variable_declaration(previous());
    }

    if (match(lexer::TokenKind::KwVar)) {
        return parse_variable_declaration(previous());
    }

    if (match(lexer::TokenKind::KwFunction)) {
        return parse_function_declaration(previous());
    }

    return parse_statement();
}

ast::BlockItemPtr Parser::parse_variable_declaration(
    const lexer::Token& keyword
) {
    const auto mutability = keyword.kind == lexer::TokenKind::KwVar
        ? ast::Mutability::mutable_
        : ast::Mutability::immutable;

    const lexer::Token name = consume(
        lexer::TokenKind::Identifier,
        "esperado nome após 'let' ou 'var'"
    );

    consume(
        lexer::TokenKind::Colon,
        "esperado ':' após o nome da variável; ArgosLang exige tipo explícito"
    );

    auto type = parse_type();
    ast::ExpressionPtr initializer;

    if (match(lexer::TokenKind::Equal)) {
        initializer = parse_expression();
    }

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
        "esperado nome após 'func'"
    );

    consume(
        lexer::TokenKind::LeftParen,
        "esperado '(' após o nome da função"
    );

    std::vector<ast::ParameterPtr> parameters;

    if (!check(lexer::TokenKind::RightParen)) {
        while (!is_at_end()) {
            const lexer::Token parameter_name = consume(
                lexer::TokenKind::Identifier,
                "esperado nome do parâmetro"
            );

            consume(
                lexer::TokenKind::Colon,
                "esperado ':' após o nome do parâmetro"
            );

            auto parameter_type = parse_type();
            const auto parameter_span = span_from(
                parameter_name.span.begin,
                parameter_type->span.end
            );

            parameters.push_back(std::make_unique<ast::Parameter>(
                parameter_name.lexeme,
                std::move(parameter_type),
                parameter_span
            ));

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

    auto body = parse_required_block(
        "esperado '{' para iniciar o corpo da função"
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

ast::StatementPtr Parser::parse_statement() {
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

    if (match(lexer::TokenKind::KwUnsafe)) {
        return parse_unsafe_statement(previous());
    }

    return parse_expression_statement();
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
    auto then_branch = parse_required_block(
        "esperado '{' após a condição do 'if'"
    );

    ast::StatementPtr else_branch;

    if (match(lexer::TokenKind::KwElse)) {
        if (match(lexer::TokenKind::KwIf)) {
            else_branch = parse_if_statement(previous());
        } else {
            else_branch = parse_required_block(
                "esperado '{' ou 'if' após 'else'"
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
    auto body = parse_required_block(
        "esperado '{' para iniciar o corpo do 'while'"
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
    const lexer::Token name = consume(
        lexer::TokenKind::Identifier,
        "esperado nome de tipo"
    );

    std::size_t nullable_depth = 0;
    source::SourceLocation end = name.span.end;

    while (match(lexer::TokenKind::Question)) {
        ++nullable_depth;
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
    if (match(lexer::TokenKind::Identifier)) {
        const lexer::Token token = previous();
        return std::make_unique<ast::IdentifierExpression>(
            token.lexeme,
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

    if (match(lexer::TokenKind::StringLiteral)) {
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

    if (!check(lexer::TokenKind::RightParen)) {
        while (!is_at_end()) {
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

    return std::make_unique<ast::CallExpression>(
        std::move(callee),
        std::move(arguments),
        span
    );
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
        case lexer::TokenKind::Question:
            return ast::PostfixOperator::propagate;
        case lexer::TokenKind::Bang:
            return ast::PostfixOperator::checked_unwrap;
        case lexer::TokenKind::ForceUnwrap:
            return ast::PostfixOperator::force_unwrap;
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