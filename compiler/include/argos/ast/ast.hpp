#pragma once

#include "argos/source/source_span.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace argos::ast {
    enum class NodeKind : std::uint8_t {
        program,
        type_syntax,
        parameter,
        
        variable_declaration,
        function_declaration,

        block_statement,
        expression_statement,
        if_statement,
        while_statement,
        unsafe_block_statement,
        return_statement,

        binary_expression,
        unary_expression,
        integer_literal_expression,
        string_literal_expression,
        boolean_literal_expression,
        null_literal_expression,
        identifier_expression,
        call_expression,
        member_access_expression,
        array_expression,
        assignment_expression,
        conditional_expression,
        error_expression,
        grouping_expression,
        postfix_expression,
    };

    enum class Mutability : std::uint8_t {
        immutable,
        mutable_,
    };
    
    enum class UnaryOperator : std::uint8_t {
        plus,
        minus,
        not_,
    };

    enum class BinaryOperator : std::uint8_t {
        add,
        subtract,
        multiply,
        divide,
        modulo,
        equal,
        not_equal,
        less_than,
        less_than_or_equal,
        greater_than,
        greater_than_or_equal,
        logical_and,
        logical_or,
        lifted_equal,
        null_coalescing,
    };

    enum class AssignmentOperator : std::uint8_t {
        assign,
        add_assign,
        subtract_assign,
        multiply_assign,
        divide_assign,
        modulo_assign,
        conditional_assign,
        null_coalescing_assign,
    };

    enum class PostfixOperator : std::uint8_t {
        increment,
        decrement,
        propagate,
        checked_unwrap,
        force_unwrap,
    };

    struct Node {
        NodeKind kind;
        source::SourceSpan span;

        Node(NodeKind kind_value, source::SourceSpan span_value)
            : kind(kind_value), 
                span(std::move(span_value)) {
        }

        Node(const Node&) = delete;
        Node& operator = (const Node&) = delete;
        Node(Node&&) noexcept = default;
        Node& operator = (Node&&) noexcept = default;

        virtual ~Node() = default;
    };

    // Forward declarations
    struct BlockStatement;

    struct Expression : Node {
        protected:
            Expression(NodeKind kind_value, source::SourceSpan span_value)
                : Node(kind_value, std::move(span_value)) {
            }
    };

    struct BlockItem : Node {
        protected:
            BlockItem(NodeKind kind_value, source::SourceSpan span_value)
                : Node(kind_value, std::move(span_value)) {
            }
    };

    struct Declaration : BlockItem {
        protected:
            Declaration(NodeKind kind_value, source::SourceSpan span_value)
                : BlockItem(kind_value, std::move(span_value)) {
            }
    };

    struct Statement : BlockItem {
        protected:
            Statement(NodeKind kind_value, source::SourceSpan span_value)
                : BlockItem(kind_value, std::move(span_value)) {
            }
    };

    using ExpressionPtr = std::unique_ptr<Expression>;
    using BlockItemPtr = std::unique_ptr<BlockItem>;
    using DeclarationPtr = std::unique_ptr<Declaration>;
    using StatementPtr = std::unique_ptr<Statement>;

    struct TypeSyntax final : Node {
        std::string name;
        std::size_t nullable_depth{0};

        TypeSyntax(
            std::string name_value,
            std::size_t nullable_depth_value,
            source::SourceSpan span_value
        )
            : Node(NodeKind::type_syntax, std::move(span_value)),
                name(std::move(name_value)),
                nullable_depth(nullable_depth_value) {
        }
    };

    using TypeSyntaxPtr = std::unique_ptr<TypeSyntax>;

    struct Parameter final : Node {
        std::string name;
        TypeSyntaxPtr type;

        Parameter(
            std::string name_value,
            TypeSyntaxPtr type_value,
            source::SourceSpan span_value
        )
            : Node(NodeKind::parameter, std::move(span_value)),
                name(std::move(name_value)),
                type(std::move(type_value)) {
        }
    };

    using ParameterPtr = std::unique_ptr<Parameter>;

    struct Program final : Node {
        std::vector<BlockItemPtr> items;

        Program(
            std::vector<BlockItemPtr> items_value,
            source::SourceSpan span_value
        )
            : Node(NodeKind::program, std::move(span_value)),
                items(std::move(items_value)) {
        }
    };

    struct VariableDeclaration final : Declaration {
        Mutability mutability;
        std::string name;
        TypeSyntaxPtr type;
        ExpressionPtr initializer;

        VariableDeclaration(
            Mutability mutability_value,
            std::string name_value,
            TypeSyntaxPtr type_value,
            ExpressionPtr initializer_value,
            source::SourceSpan span_value
        )
            : Declaration(NodeKind::variable_declaration, std::move(span_value)),
                mutability(mutability_value),
                name(std::move(name_value)),
                type(std::move(type_value)),
                initializer(std::move(initializer_value)) {
        }
    };

    struct BlockStatement final : Statement {
        std::vector<BlockItemPtr> items;

        BlockStatement(
            std::vector<BlockItemPtr> items_value,
            source::SourceSpan span_value
        )
            : Statement(NodeKind::block_statement, std::move(span_value)),
                items(std::move(items_value)) {
        }
    };

    struct FunctionDeclaration final : Declaration {
        std::string name;
        std::vector<ParameterPtr> parameters;
        TypeSyntaxPtr return_type;
        std::unique_ptr<BlockStatement> body;

        FunctionDeclaration(
            std::string name_value,
            std::vector<ParameterPtr> parameters_value,
            TypeSyntaxPtr return_type_value,
            std::unique_ptr<BlockStatement> body_value,
            source::SourceSpan span_value
        )
            : Declaration(NodeKind::function_declaration, std::move(span_value)),
                name(std::move(name_value)),
                parameters(std::move(parameters_value)),
                return_type(std::move(return_type_value)),
                body(std::move(body_value)) {
        }
    };

    struct ExpressionStatement final : Statement {
        ExpressionPtr expression;

        ExpressionStatement(
            ExpressionPtr expression_value,
            source::SourceSpan span_value
        )
            : Statement(NodeKind::expression_statement, std::move(span_value)),
                expression(std::move(expression_value)) {
        }
    };

    struct ReturnStatement final : Statement {
        ExpressionPtr expression;

        ReturnStatement(
            ExpressionPtr expression_value,
            source::SourceSpan span_value
        )
            : Statement(NodeKind::return_statement, std::move(span_value)),
                expression(std::move(expression_value)) {
        }
    };

    struct IfStatement final : Statement {
        ExpressionPtr condition;
        std::unique_ptr<BlockStatement> then_branch;
        StatementPtr else_branch; // StatementPtr permite encadeamento de else-if

        IfStatement(
            ExpressionPtr condition_value,
            std::unique_ptr<BlockStatement> then_branch_value,
            StatementPtr else_branch_value,
            source::SourceSpan span_value
        )
            : Statement(NodeKind::if_statement, std::move(span_value)),
                condition(std::move(condition_value)),
                then_branch(std::move(then_branch_value)),
                else_branch(std::move(else_branch_value)) {
        }
    };

    struct WhileStatement final : Statement {
        ExpressionPtr condition;
        std::unique_ptr<BlockStatement> body;

        WhileStatement(
            ExpressionPtr condition_value,
            std::unique_ptr<BlockStatement> body_value,
            source::SourceSpan span_value
        )
            : Statement(NodeKind::while_statement, std::move(span_value)),
                condition(std::move(condition_value)),
                body(std::move(body_value)) {
        }
    };

    struct UnsafeBlockStatement final : Statement {
        std::unique_ptr<BlockStatement> block;

        UnsafeBlockStatement(
            std::unique_ptr<BlockStatement> block_value,
            source::SourceSpan span_value
        )
            : Statement(NodeKind::unsafe_block_statement, std::move(span_value)),
                block(std::move(block_value)) {
        }
    };

    struct ErrorExpression final : Expression {
        explicit ErrorExpression(source::SourceSpan span_value)
            : Expression(NodeKind::error_expression, std::move(span_value)) {
        }
    };

    struct IdentifierExpression final : Expression {
        std::string name;

        IdentifierExpression(
            std::string name_value,
            source::SourceSpan span_value
        )
            : Expression(NodeKind::identifier_expression, std::move(span_value)),
                name(std::move(name_value)) {
        }
    };

    struct IntegerLiteralExpression final : Expression {
        std::string spelling;

        IntegerLiteralExpression(
            std::string spelling_value,
            source::SourceSpan span_value
        )
            : Expression(
                NodeKind::integer_literal_expression,
                std::move(span_value)
            ),
            spelling(std::move(spelling_value)) {
        }
    };

    struct StringLiteralExpression final : Expression {
        std::string spelling;

        StringLiteralExpression(
            std::string spelling_value,
            source::SourceSpan span_value
        )
            : Expression(
                NodeKind::string_literal_expression,
                std::move(span_value)
            ),
            spelling(std::move(spelling_value)) {
        }
    };

    struct BooleanLiteralExpression final : Expression {
        bool value;

        BooleanLiteralExpression(
            bool value_value,
            source::SourceSpan span_value
        )
            : Expression(
                NodeKind::boolean_literal_expression,
                std::move(span_value)
            ),
            value(value_value) {
        }
    };

    struct NullLiteralExpression final : Expression {
        explicit NullLiteralExpression(source::SourceSpan span_value)
            : Expression(NodeKind::null_literal_expression, std::move(span_value)) {
        }
    };

    struct GroupingExpression final : Expression {
        ExpressionPtr expression;

        GroupingExpression(
            ExpressionPtr expression_value,
            source::SourceSpan span_value
        )
            : Expression(NodeKind::grouping_expression, std::move(span_value)),
            expression(std::move(expression_value)) {
        }
    };

    struct UnaryExpression final : Expression {
        UnaryOperator op;
        ExpressionPtr operand;

        UnaryExpression(
            UnaryOperator op_value,
            ExpressionPtr operand_value,
            source::SourceSpan span_value
        )
            : Expression(NodeKind::unary_expression, std::move(span_value)),
            op(op_value),
            operand(std::move(operand_value)) {
        }
    };

    struct BinaryExpression final : Expression {
        ExpressionPtr left;
        BinaryOperator op;
        ExpressionPtr right;

        BinaryExpression(
            ExpressionPtr left_value,
            BinaryOperator op_value,
            ExpressionPtr right_value,
            source::SourceSpan span_value
        )
            : Expression(NodeKind::binary_expression, std::move(span_value)),
            left(std::move(left_value)),
            op(op_value),
            right(std::move(right_value)) {
        }
    };

    struct AssignmentExpression final : Expression {
        ExpressionPtr target;
        AssignmentOperator op;
        ExpressionPtr value;

        AssignmentExpression(
            ExpressionPtr target_value,
            AssignmentOperator op_value,
            ExpressionPtr value_expression,
            source::SourceSpan span_value
        )
            : Expression(NodeKind::assignment_expression, std::move(span_value)),
            target(std::move(target_value)),
            op(op_value),
            value(std::move(value_expression)) {
        }
    };

    struct ConditionalExpression final : Expression {
        ExpressionPtr condition;
        ExpressionPtr then_expr;
        ExpressionPtr else_expr;

        ConditionalExpression(
            ExpressionPtr condition_value,
            ExpressionPtr then_expr_value,
            ExpressionPtr else_expr_value,
            source::SourceSpan span_value
        )
            : Expression(NodeKind::conditional_expression, std::move(span_value)),
            condition(std::move(condition_value)),
            then_expr(std::move(then_expr_value)),
            else_expr(std::move(else_expr_value)) {
        }
    };

    struct ArrayExpression final : Expression {
        std::vector<ExpressionPtr> elements;

        ArrayExpression(
            std::vector<ExpressionPtr> elements_value,
            source::SourceSpan span_value
        )
            : Expression(NodeKind::array_expression, std::move(span_value)),
            elements(std::move(elements_value)) {
        }
    };

    struct CallExpression final : Expression {
        ExpressionPtr callee;
        std::vector<ExpressionPtr> arguments;

        CallExpression(
            ExpressionPtr callee_value,
            std::vector<ExpressionPtr> arguments_value,
            source::SourceSpan span_value
        )
            : Expression(NodeKind::call_expression, std::move(span_value)),
            callee(std::move(callee_value)),
            arguments(std::move(arguments_value)) {
        }
    };

    struct MemberAccessExpression final : Expression {
        ExpressionPtr base;
        std::string member;
        bool safe{false};

        MemberAccessExpression(
            ExpressionPtr base_value,
            std::string member_value,
            bool safe_value,
            source::SourceSpan span_value
        )
            : Expression(
                NodeKind::member_access_expression,
                std::move(span_value)
            ),
            base(std::move(base_value)),
            member(std::move(member_value)),
            safe(safe_value) {
        }
    };

    struct PostfixExpression final : Expression {
        ExpressionPtr operand;
        PostfixOperator op;

        PostfixExpression(
            ExpressionPtr operand_value,
            PostfixOperator op_value,
            source::SourceSpan span_value
        )
            : Expression(NodeKind::postfix_expression, std::move(span_value)),
            operand(std::move(operand_value)),
            op(op_value) {
        }
    };
}