#include "argos/ast/ast_printer.hpp"

#include <string>
#include <string_view>

namespace argos::ast {
namespace {

std::string_view mutability_name(Mutability mutability) noexcept {
    return mutability == Mutability::mutable_ ? "var" : "let";
}

std::string_view unary_operator_name(UnaryOperator op) noexcept {
    switch (op) {
        case UnaryOperator::plus:
            return "+";
        case UnaryOperator::minus:
            return "-";
        case UnaryOperator::not_:
            return "!";
    }
    return "<?>";
}

std::string_view binary_operator_name(BinaryOperator op) noexcept {
    switch (op) {
        case BinaryOperator::add:
            return "+";
        case BinaryOperator::subtract:
            return "-";
        case BinaryOperator::multiply:
            return "*";
        case BinaryOperator::divide:
            return "/";
        case BinaryOperator::modulo:
            return "%";
        case BinaryOperator::equal:
            return "==";
        case BinaryOperator::not_equal:
            return "!=";
        case BinaryOperator::less_than:
            return "<";
        case BinaryOperator::less_than_or_equal:
            return "<=";
        case BinaryOperator::greater_than:
            return ">";
        case BinaryOperator::greater_than_or_equal:
            return ">=";
        case BinaryOperator::logical_and:
            return "&&";
        case BinaryOperator::logical_or:
            return "||";
        case BinaryOperator::lifted_equal:
            return "?==";
        case BinaryOperator::null_coalescing:
            return "??";
    }
    return "<?>";
}

std::string_view assignment_operator_name(AssignmentOperator op) noexcept {
    switch (op) {
        case AssignmentOperator::assign:
            return "=";
        case AssignmentOperator::add_assign:
            return "+=";
        case AssignmentOperator::subtract_assign:
            return "-=";
        case AssignmentOperator::multiply_assign:
            return "*=";
        case AssignmentOperator::divide_assign:
            return "/=";
        case AssignmentOperator::modulo_assign:
            return "%=";
        case AssignmentOperator::conditional_assign:
            return "?=";
        case AssignmentOperator::null_coalescing_assign:
            return "?" "?=";
    }
    return "<?>";
}

std::string_view postfix_operator_name(PostfixOperator op) noexcept {
    switch (op) {
        case PostfixOperator::increment:
            return "++";
        case PostfixOperator::decrement:
            return "--";
        case PostfixOperator::propagate:
            return "?";
        case PostfixOperator::checked_unwrap:
            return "!";
        case PostfixOperator::force_unwrap:
            return "!!";
    }
    return "<?>";
}

} // namespace

std::string AstPrinter::print(const Program &program) {
    output_.clear();
    indentation_ = 0;
    write_node(program);
    return output_;
}

void AstPrinter::write_node(const Node &node) {
    switch (node.kind) {
        case NodeKind::program: {
            const auto &value = static_cast<const Program &>(node);
            write_line("Program");
            IndentGuard indent{*this};
            for (const auto &item : value.items) {
                write_node(*item);
            }
            return;
        }

        case NodeKind::type_syntax:
            write_type(static_cast<const TypeSyntax &>(node));
            return;

        case NodeKind::parameter: {
            const auto &value = static_cast<const Parameter &>(node);
            write_line("Parameter " + value.name);
            IndentGuard indent{*this};
            if (value.type) write_type(*value.type);
            return;
        }

        case NodeKind::variable_declaration: {
            const auto &value = static_cast<const VariableDeclaration &>(node);
            write_line("VariableDeclaration " +
                       std::string{mutability_name(value.mutability)} + " " +
                       value.name);
            IndentGuard indent{*this};
            if (value.type) write_type(*value.type);
            if (value.initializer != nullptr) {
                write_label("initializer");
                IndentGuard nested{*this};
                write_node(*value.initializer);
            }
            return;
        }

        case NodeKind::function_declaration: {
            const auto &value = static_cast<const FunctionDeclaration &>(node);
            write_line("FunctionDeclaration " + value.name);
            IndentGuard indent{*this};

            write_label("parameters");
            {
                IndentGuard nested{*this};
                for (const auto &parameter : value.parameters) {
                    write_node(*parameter);
                }
            }

            write_label("returnType");
            {
                IndentGuard nested{*this};
                if (value.return_type != nullptr) {
                    write_type(*value.return_type);
                } else {
                    write_line("Void (implicit)");
                }
            }

            write_label("body");
            {
                IndentGuard nested{*this};
                write_node(*value.body);
            }
            return;
        }

        case NodeKind::protocol_declaration: {
            const auto &value = static_cast<const ProtocolDeclaration &>(node);
            write_line("ProtocolDeclaration " + value.name);
            IndentGuard indent{*this};
            for (const auto &component : value.components) write_node(*component);
            return;
        }

        case NodeKind::block_statement: {
            const auto &value = static_cast<const BlockStatement &>(node);
            write_line("BlockStatement");
            IndentGuard indent{*this};
            for (const auto &item : value.items) {
                write_node(*item);
            }
            return;
        }

        case NodeKind::expression_statement: {
            const auto &value = static_cast<const ExpressionStatement &>(node);
            write_line("ExpressionStatement");
            IndentGuard indent{*this};
            write_node(*value.expression);
            return;
        }

        case NodeKind::return_statement: {
            const auto &value = static_cast<const ReturnStatement &>(node);
            write_line("ReturnStatement");
            if (value.expression != nullptr) {
                IndentGuard indent{*this};
                write_node(*value.expression);
            }
            return;
        }

        case NodeKind::if_statement: {
            const auto &value = static_cast<const IfStatement &>(node);
            write_line("IfStatement");
            IndentGuard indent{*this};

            write_label("condition");
            {
                IndentGuard nested{*this};
                write_node(*value.condition);
            }

            write_label("then");
            {
                IndentGuard nested{*this};
                write_node(*value.then_branch);
            }

            if (value.else_branch != nullptr) {
                write_label("else");
                IndentGuard nested{*this};
                write_node(*value.else_branch);
            }
            return;
        }

        case NodeKind::while_statement: {
            const auto &value = static_cast<const WhileStatement &>(node);
            write_line("WhileStatement");
            IndentGuard indent{*this};

            write_label("condition");
            {
                IndentGuard nested{*this};
                write_node(*value.condition);
            }

            write_label("body");
            {
                IndentGuard nested{*this};
                write_node(*value.body);
            }
            return;
        }

        case NodeKind::unsafe_block_statement: {
            const auto &value = static_cast<const UnsafeBlockStatement &>(node);
            write_line("UnsafeBlockStatement");
            IndentGuard indent{*this};
            write_node(*value.block);
            return;
        }

        case NodeKind::error_expression:
            write_line("ErrorExpression");
            return;

        case NodeKind::identifier_expression: {
            const auto &value = static_cast<const IdentifierExpression &>(node);
            write_line("IdentifierExpression " + value.name);
            return;
        }

        case NodeKind::integer_literal_expression: {
            const auto &value = static_cast<const IntegerLiteralExpression &>(node);
            write_line("IntegerLiteralExpression " + value.spelling);
            return;
        }
        case NodeKind::float_literal_expression: {
            const auto &value = static_cast<const FloatLiteralExpression &>(node);
            write_line("FloatLiteralExpression " + value.spelling);
            return;
        }
        case NodeKind::character_literal_expression: {
            const auto &value = static_cast<const CharacterLiteralExpression &>(node);
            write_line("CharacterLiteralExpression " + value.spelling);
            return;
        }

        case NodeKind::string_literal_expression: {
            const auto &value = static_cast<const StringLiteralExpression &>(node);
            write_line("StringLiteralExpression " + value.spelling);
            return;
        }

        case NodeKind::boolean_literal_expression: {
            const auto &value = static_cast<const BooleanLiteralExpression &>(node);
            write_line(value.value ? "BooleanLiteralExpression true"
                                   : "BooleanLiteralExpression false");
            return;
        }

        case NodeKind::null_literal_expression:
            write_line("NullLiteralExpression");
            return;

        case NodeKind::grouping_expression: {
            const auto &value = static_cast<const GroupingExpression &>(node);
            write_line("GroupingExpression");
            IndentGuard indent{*this};
            write_node(*value.expression);
            return;
        }

        case NodeKind::unary_expression: {
            const auto &value = static_cast<const UnaryExpression &>(node);
            write_line("UnaryExpression " + std::string{unary_operator_name(value.op)});
            IndentGuard indent{*this};
            write_node(*value.operand);
            return;
        }

        case NodeKind::binary_expression: {
            const auto &value = static_cast<const BinaryExpression &>(node);
            write_line("BinaryExpression " +
                       std::string{binary_operator_name(value.op)});
            IndentGuard indent{*this};
            write_node(*value.left);
            write_node(*value.right);
            return;
        }

        case NodeKind::assignment_expression: {
            const auto &value = static_cast<const AssignmentExpression &>(node);
            write_line("AssignmentExpression " +
                       std::string{assignment_operator_name(value.op)});
            IndentGuard indent{*this};
            write_label("target");
            {
                IndentGuard nested{*this};
                write_node(*value.target);
            }
            write_label("value");
            {
                IndentGuard nested{*this};
                write_node(*value.value);
            }
            return;
        }

        case NodeKind::conditional_expression: {
            const auto &value = static_cast<const ConditionalExpression &>(node);
            write_line("ConditionalExpression");
            IndentGuard indent{*this};
            write_label("condition");
            {
                IndentGuard nested{*this};
                write_node(*value.condition);
            }
            write_label("then");
            {
                IndentGuard nested{*this};
                write_node(*value.then_expr);
            }
            write_label("else");
            {
                IndentGuard nested{*this};
                write_node(*value.else_expr);
            }
            return;
        }

        case NodeKind::array_expression: {
            const auto &value = static_cast<const ArrayExpression &>(node);
            write_line("ArrayExpression");
            IndentGuard indent{*this};
            for (const auto &element : value.elements) {
                write_node(*element);
            }
            return;
        }

        case NodeKind::call_expression: {
            const auto &value = static_cast<const CallExpression &>(node);
            write_line("CallExpression");
            IndentGuard indent{*this};
            write_label("callee");
            {
                IndentGuard nested{*this};
                write_node(*value.callee);
            }
            write_label("arguments");
            {
                IndentGuard nested{*this};
                for (const auto &argument : value.arguments) {
                    write_node(*argument);
                }
            }
            return;
        }

        case NodeKind::member_access_expression: {
            const auto &value = static_cast<const MemberAccessExpression &>(node);
            write_line(std::string{value.safe ? "SafeMemberAccessExpression "
                                              : "MemberAccessExpression "} +
                       value.member);
            IndentGuard indent{*this};
            write_node(*value.base);
            return;
        }

        case NodeKind::postfix_expression: {
            const auto &value = static_cast<const PostfixExpression &>(node);
            write_line("PostfixExpression " +
                       std::string{postfix_operator_name(value.op)});
            IndentGuard indent{*this};
            write_node(*value.operand);
            return;
        }
    }
}

void AstPrinter::write_type(const TypeSyntax &type) {
    std::string text = "TypeSyntax " + type.name;
    text.append(type.nullable_depth, '?');
    write_line(text);
}

void AstPrinter::write_line(std::string_view text) {
    output_.append(indentation_ * 2, ' ');
    output_.append(text);
    output_.push_back('\n');
}

void AstPrinter::write_label(std::string_view label) {
    write_line(std::string{label} + ":");
}

AstPrinter::IndentGuard::IndentGuard(AstPrinter &printer) noexcept
    : printer_(printer) {
    ++printer_.indentation_;
}

AstPrinter::IndentGuard::~IndentGuard() { --printer_.indentation_; }

} // namespace argos::ast
