#pragma once

#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/lexer/token.hpp"
#include "argos/source/source_file.hpp"
#include "argos/source/source_location.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace argos::lexer {

class Lexer final {
public:
    Lexer(
        const source::SourceFile& source_file,
        diagnostics::DiagnosticEngine& diagnostics
    );

    [[nodiscard]]
    std::vector<Token> tokenize();

private:
    [[nodiscard]] bool is_at_end() const noexcept;
    [[nodiscard]] char current() const noexcept;
    [[nodiscard]] char peek(std::size_t distance = 1) const noexcept;

    char advance() noexcept;
    bool match(char expected) noexcept;

    void skip_trivia();
    void skip_line_comment();
    void skip_block_comment();

    Token scan_token();
    Token scan_identifier_or_keyword();
    Token scan_number();
    Token scan_string();
    Token scan_character();
    Token scan_escaped_identifier();
    Token scan_multiline_string();

    Token make_token(
        TokenKind kind,
        source::SourceLocation begin
    ) const;

    Token make_invalid_token(
        source::SourceLocation begin,
        std::string message
    );

    [[nodiscard]]
    static TokenKind keyword_kind(std::string_view lexeme) noexcept;

    const source::SourceFile& source_file_;
    diagnostics::DiagnosticEngine& diagnostics_;

    std::size_t start_offset_{0};
    std::size_t offset_{0};
    std::size_t line_{1};
    std::size_t column_{1};
    std::size_t delimiter_depth_{0};
    TokenKind previous_kind_{TokenKind::Invalid};
    bool pending_semicolon_{false};
    source::SourceLocation pending_location_{};
};

} // namespace argos::lexer
