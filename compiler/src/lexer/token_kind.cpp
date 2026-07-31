#include "argos/lexer/token_kind.hpp"

namespace argos::lexer {

std::string_view token_kind_name(TokenKind kind) noexcept {
    switch (kind) {
        case TokenKind::end_of_file: return "end_of_file";
        case TokenKind::invalid: return "invalid";
        case TokenKind::identifier: return "identifier";
        case TokenKind::integer_literal: return "integer_literal";
        case TokenKind::string_literal: return "string_literal";
        case TokenKind::keyword_let: return "keyword_let";
        case TokenKind::keyword_var: return "keyword_var";
        case TokenKind::keyword_func: return "keyword_func";
        case TokenKind::keyword_return: return "keyword_return";
        case TokenKind::keyword_if: return "keyword_if";
        case TokenKind::keyword_else: return "keyword_else";
        case TokenKind::keyword_null: return "keyword_null";
        case TokenKind::keyword_true: return "keyword_true";
        case TokenKind::keyword_false: return "keyword_false";
        case TokenKind::left_parenthesis: return "left_parenthesis";
        case TokenKind::right_parenthesis: return "right_parenthesis";
        case TokenKind::left_brace: return "left_brace";
        case TokenKind::right_brace: return "right_brace";
        case TokenKind::comma: return "comma";
        case TokenKind::colon: return "colon";
        case TokenKind::semicolon: return "semicolon";
        case TokenKind::dot: return "dot";
        case TokenKind::arrow: return "arrow";
        case TokenKind::equal: return "equal";
        case TokenKind::equal_equal: return "equal_equal";
        case TokenKind::bang: return "bang";
        case TokenKind::bang_equal: return "bang_equal";
        case TokenKind::double_bang: return "double_bang";
        case TokenKind::question: return "question";
        case TokenKind::safe_dot: return "safe_dot";
        case TokenKind::coalesce: return "coalesce";
        case TokenKind::coalesce_assign: return "coalesce_assign";
        case TokenKind::conditional_assign: return "conditional_assign";
        case TokenKind::lifted_equal: return "lifted_equal";
        case TokenKind::plus: return "plus";
        case TokenKind::minus: return "minus";
        case TokenKind::star: return "star";
        case TokenKind::slash: return "slash";
    }

    return "unknown";
}

} // namespace argos::lexer
