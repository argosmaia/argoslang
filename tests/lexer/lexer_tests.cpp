#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/lexer/lexer.hpp"
#include "argos/lexer/token_kind.hpp"
#include "argos/source/source_file.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <vector>

namespace {

std::vector<argos::lexer::TokenKind> kinds_of(
    const std::string& source,
    argos::diagnostics::DiagnosticEngine& diagnostics
) {
    const argos::source::SourceFile source_file{
        .path = std::filesystem::path{"test.argos"},
        .content = source,
    };

    argos::lexer::Lexer lexer{source_file, diagnostics};
    const auto tokens = lexer.tokenize();

    std::vector<argos::lexer::TokenKind> kinds;
    kinds.reserve(tokens.size());

    for (const auto& token : tokens) {
        kinds.push_back(token.kind);
    }

    return kinds;
}

} // namespace

TEST_CASE("lexes Swift-like null-safety chain") {
    argos::diagnostics::DiagnosticEngine diagnostics;

    const auto kinds = kinds_of(
        R"(let name: String = user?.profile?.name ?? "Anonymous")",
        diagnostics
    );

    REQUIRE_FALSE(diagnostics.has_errors());

    REQUIRE(kinds == std::vector{
        argos::lexer::TokenKind::keyword_let,
        argos::lexer::TokenKind::identifier,
        argos::lexer::TokenKind::colon,
        argos::lexer::TokenKind::identifier,
        argos::lexer::TokenKind::equal,
        argos::lexer::TokenKind::identifier,
        argos::lexer::TokenKind::safe_dot,
        argos::lexer::TokenKind::identifier,
        argos::lexer::TokenKind::safe_dot,
        argos::lexer::TokenKind::identifier,
        argos::lexer::TokenKind::coalesce,
        argos::lexer::TokenKind::string_literal,
        argos::lexer::TokenKind::end_of_file,
    });
}

TEST_CASE("uses maximal munch for nullable operators") {
    argos::diagnostics::DiagnosticEngine diagnostics;

    const auto kinds = kinds_of(
        "a ??= b; c ?= d; e ?== f; g !!;",
        diagnostics
    );

    REQUIRE_FALSE(diagnostics.has_errors());

    REQUIRE(kinds == std::vector{
        argos::lexer::TokenKind::identifier,
        argos::lexer::TokenKind::coalesce_assign,
        argos::lexer::TokenKind::identifier,
        argos::lexer::TokenKind::semicolon,

        argos::lexer::TokenKind::identifier,
        argos::lexer::TokenKind::conditional_assign,
        argos::lexer::TokenKind::identifier,
        argos::lexer::TokenKind::semicolon,

        argos::lexer::TokenKind::identifier,
        argos::lexer::TokenKind::lifted_equal,
        argos::lexer::TokenKind::identifier,
        argos::lexer::TokenKind::semicolon,

        argos::lexer::TokenKind::identifier,
        argos::lexer::TokenKind::double_bang,
        argos::lexer::TokenKind::semicolon,
        argos::lexer::TokenKind::end_of_file,
    });
}

TEST_CASE("supports nested block comments") {
    argos::diagnostics::DiagnosticEngine diagnostics;

    const auto kinds = kinds_of(
        "let /* outer /* inner */ outer */ value = null",
        diagnostics
    );

    REQUIRE_FALSE(diagnostics.has_errors());

    REQUIRE(kinds == std::vector{
        argos::lexer::TokenKind::keyword_let,
        argos::lexer::TokenKind::identifier,
        argos::lexer::TokenKind::equal,
        argos::lexer::TokenKind::keyword_null,
        argos::lexer::TokenKind::end_of_file,
    });
}

TEST_CASE("reports unterminated strings") {
    argos::diagnostics::DiagnosticEngine diagnostics;

    const auto kinds = kinds_of(
        R"("unterminated)",
        diagnostics
    );

    REQUIRE(diagnostics.has_errors());
    REQUIRE(kinds.front() == argos::lexer::TokenKind::invalid);
}
