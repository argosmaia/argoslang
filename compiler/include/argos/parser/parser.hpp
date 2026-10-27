#pragma once

#include "argos/ast/ast.hpp"
#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/lexer/token.hpp"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

namespace argos::parser {

class Parser final {
public:
    Parser(
        std::vector<lexer::Token> tokens,
        diagnostics::DiagnosticEngine& diagnostics
    );

    [[nodiscard]]
    ast::Program parse_program();

private:
    struct BindingPower final {
        std::uint8_t left;
        std::uint8_t right;
    };

    [[nodiscard]] bool is_at_end() const noexcept;
    [[nodiscard]] const lexer::Token& current() const noexcept;
    [[nodiscard]] const lexer::Token& previous() const noexcept;
    [[nodiscard]] const lexer::Token& peek(std::size_t distance) const noexcept;

    lexer::Token advance();
    bool check(lexer::TokenKind kind) const noexcept;
    bool match(lexer::TokenKind kind);
    bool match(std::initializer_list<lexer::TokenKind> kinds);

    lexer::Token consume(
        lexer::TokenKind kind,
        std::string message
    );

    void report(
        const lexer::Token& token,
        std::string message
    );

    void synchronize();
    void skip_unsupported_construct();

    void consume_statement_terminator(std::string context);

    [[nodiscard]] bool has_line_break_before_current() const noexcept;

    ast::BlockItemPtr parse_block_item();
    ast::BlockItemPtr parse_variable_declaration(
        const lexer::Token& keyword
    );
    ast::BlockItemPtr parse_function_declaration(
        const lexer::Token& keyword
    );
    ast::Modifiers parse_modifiers();
    void parse_accessors(ast::VariableDeclaration& property);
    bool in_class_{false};
    ast::BlockItemPtr parse_class_declaration(const lexer::Token& keyword);
    ast::ExpressionPtr parse_lambda();
    bool starts_lambda() const;
    bool in_interface_{false};
    ast::BlockItemPtr parse_protocol_declaration(const lexer::Token& keyword);

    ast::StatementPtr parse_statement();
    std::unique_ptr<ast::BlockStatement> parse_required_block(
        std::string message
    );
    std::unique_ptr<ast::BlockStatement> parse_simple_body(
        std::string message
    );
    std::unique_ptr<ast::BlockStatement> parse_block_statement(
        const lexer::Token& left_brace
    );
    ast::StatementPtr parse_return_statement(
        const lexer::Token& keyword
    );
    ast::StatementPtr parse_if_statement(
        const lexer::Token& keyword
    );
    ast::StatementPtr parse_while_statement(
        const lexer::Token& keyword
    );
    ast::StatementPtr parse_break_statement(const lexer::Token& keyword);
    ast::StatementPtr parse_continue_statement(const lexer::Token& keyword);
    ast::StatementPtr parse_unsafe_statement(
        const lexer::Token& keyword
    );
    ast::StatementPtr parse_expression_statement();

    ast::TypeSyntaxPtr parse_type();

    ast::ExpressionPtr parse_expression(
        std::uint8_t minimum_binding_power = 0
    );
    ast::ExpressionPtr parse_prefix_expression();
    ast::ExpressionPtr parse_primary_expression();

    ast::ExpressionPtr finish_call(ast::ExpressionPtr callee);
    ast::ExpressionPtr finish_member_access(
        ast::ExpressionPtr base,
        const lexer::Token& operation
    );
    ast::ExpressionPtr finish_postfix(
        ast::ExpressionPtr operand,
        const lexer::Token& operation
    );

    [[nodiscard]]
    static std::optional<BindingPower> infix_binding_power(
        lexer::TokenKind kind
    ) noexcept;

    [[nodiscard]]
    static ast::UnaryOperator unary_operator(
        lexer::TokenKind kind
    ) noexcept;

    [[nodiscard]]
    static ast::BinaryOperator binary_operator(
        lexer::TokenKind kind
    ) noexcept;

    [[nodiscard]]
    static ast::AssignmentOperator assignment_operator(
        lexer::TokenKind kind
    ) noexcept;

    [[nodiscard]]
    static ast::PostfixOperator postfix_operator(
        lexer::TokenKind kind
    ) noexcept;

    [[nodiscard]]
    static source::SourceSpan span_from(
        const source::SourceLocation& begin,
        const source::SourceLocation& end
    ) noexcept;

    [[nodiscard]]
    lexer::Token synthetic_token(lexer::TokenKind kind) const;

    std::vector<lexer::Token> tokens_;
    diagnostics::DiagnosticEngine& diagnostics_;
    std::size_t index_{0};
};

} // namespace argos::parser
