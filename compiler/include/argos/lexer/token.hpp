#pragma once

#include "argos/lexer/token_kind.hpp"
#include "argos/source/source_span.hpp"

#include <string>

namespace argos::lexer {

struct Token final {
    TokenKind kind{TokenKind::invalid};
    std::string lexeme;
    source::SourceSpan span{};

    friend bool operator==(const Token&, const Token&) = default;
};

} // namespace argos::lexer
