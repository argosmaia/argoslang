#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/lexer/lexer.hpp"
#include "argos/lexer/token_kind.hpp"
#include "argos/source/source_file.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <algorithm>
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
        R"(var name: string = user?.profile?.name ?? "Anonymous")",
        diagnostics
    );

    REQUIRE_FALSE(diagnostics.has_errors());

    REQUIRE(kinds == std::vector{
        argos::lexer::TokenKind::KwVar,
        argos::lexer::TokenKind::Identifier,
        argos::lexer::TokenKind::Colon,
        argos::lexer::TokenKind::KwString,
        argos::lexer::TokenKind::Equal,
        argos::lexer::TokenKind::Identifier,
        argos::lexer::TokenKind::QuestionDot,
        argos::lexer::TokenKind::Identifier,
        argos::lexer::TokenKind::QuestionDot,
        argos::lexer::TokenKind::Identifier,
        argos::lexer::TokenKind::NullCoalescing,
        argos::lexer::TokenKind::StringLiteral,
        argos::lexer::TokenKind::EndOfFile,
    });
}

TEST_CASE("uses maximal munch for nullable operators") {
    argos::diagnostics::DiagnosticEngine diagnostics;

    const auto kinds = kinds_of(
        "a ?" "?= b; c ?= d; e ?== f; g !!;",
        diagnostics
    );

    REQUIRE_FALSE(diagnostics.has_errors());

    REQUIRE(kinds == std::vector{
        argos::lexer::TokenKind::Identifier,
        argos::lexer::TokenKind::NullCoalescingAssign,
        argos::lexer::TokenKind::Identifier,
        argos::lexer::TokenKind::Semicolon,

        argos::lexer::TokenKind::Identifier,
        argos::lexer::TokenKind::SafeAssign,
        argos::lexer::TokenKind::Identifier,
        argos::lexer::TokenKind::Semicolon,

        argos::lexer::TokenKind::Identifier,
        argos::lexer::TokenKind::SafeEqualityAssign,
        argos::lexer::TokenKind::Identifier,
        argos::lexer::TokenKind::Semicolon,

        argos::lexer::TokenKind::Identifier,
        argos::lexer::TokenKind::ForceUnwrap,
        argos::lexer::TokenKind::Semicolon,
        argos::lexer::TokenKind::EndOfFile,
    });
}

TEST_CASE("supports nested block comments") {
    argos::diagnostics::DiagnosticEngine diagnostics;

    const auto kinds = kinds_of(
        "var /* outer /* inner */ outer */ value = nil",
        diagnostics
    );

    REQUIRE_FALSE(diagnostics.has_errors());

    REQUIRE(kinds == std::vector{
        argos::lexer::TokenKind::KwVar,
        argos::lexer::TokenKind::Identifier,
        argos::lexer::TokenKind::Equal,
        argos::lexer::TokenKind::KwNull,
        argos::lexer::TokenKind::EndOfFile,
    });
}

TEST_CASE("treats newlines inside block comments as eligible terminators") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto kinds = kinds_of("var x = 1 /* note\n continued */ var y = 2", diagnostics);
    REQUIRE_FALSE(diagnostics.has_errors());
    REQUIRE(std::count(kinds.begin(), kinds.end(), argos::lexer::TokenKind::Semicolon) == 1);
}

TEST_CASE("reports unterminated strings") {
    argos::diagnostics::DiagnosticEngine diagnostics;

    const auto kinds = kinds_of(
        R"("unterminated)",
        diagnostics
    );

    REQUIRE(diagnostics.has_errors());
    REQUIRE(kinds.front() == argos::lexer::TokenKind::Invalid);
}

TEST_CASE("lexes Unicode identifiers and new literal forms") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto kinds = kinds_of(
        "var café = 1.25e2; var letter: char = 'á'; var text = \"\"\"a\nb\"\"\";",
        diagnostics
    );
    REQUIRE_FALSE(diagnostics.has_errors());
    REQUIRE(kinds[1] == argos::lexer::TokenKind::Identifier);
    REQUIRE(kinds[3] == argos::lexer::TokenKind::FloatLiteral);
    REQUIRE(kinds[10] == argos::lexer::TokenKind::CharacterLiteral);
    REQUIRE(kinds[15] == argos::lexer::TokenKind::MultilineStringLiteral);
}

TEST_CASE("inserts terminators only at eligible newlines") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto kinds = kinds_of("var x = (1 +\n 2)\nvar y = 3", diagnostics);
    REQUIRE_FALSE(diagnostics.has_errors());
    REQUIRE(std::count(kinds.begin(), kinds.end(), argos::lexer::TokenKind::Semicolon) == 1);
}

TEST_CASE("keeps multiline signatures and operator continuations together") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto kinds = kinds_of("function f()\n -> int { return\n1\n+ 2; }", diagnostics);
    REQUIRE_FALSE(diagnostics.has_errors());
    REQUIRE(kinds[4] == argos::lexer::TokenKind::Arrow);
    REQUIRE(std::count(kinds.begin(), kinds.end(), argos::lexer::TokenKind::Semicolon) == 2);
}

TEST_CASE("accepts only canonical function keyword and lowercase primitive types") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto kinds = kinds_of(
        "function f(value: int) -> string; func f(); Int String Float",
        diagnostics
    );
    REQUIRE_FALSE(diagnostics.has_errors());
    REQUIRE(kinds[0] == argos::lexer::TokenKind::KwFunction);
    REQUIRE(kinds[5] == argos::lexer::TokenKind::KwInt);
    REQUIRE(kinds[8] == argos::lexer::TokenKind::KwString);
    REQUIRE(kinds[10] == argos::lexer::TokenKind::Identifier);
    REQUIRE(kinds[15] == argos::lexer::TokenKind::Identifier);
    REQUIRE(kinds[16] == argos::lexer::TokenKind::Identifier);
    REQUIRE(kinds[17] == argos::lexer::TokenKind::Identifier);
}

TEST_CASE("rejects malformed UTF-8 identifiers") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto kinds = kinds_of(std::string{"var "} + static_cast<char>(0xC3) + " = 1", diagnostics);
    REQUIRE(diagnostics.has_errors());
    REQUIRE(std::find(kinds.begin(), kinds.end(), argos::lexer::TokenKind::Invalid) != kinds.end());
}

TEST_CASE("rejects malformed UTF-8 even inside strings") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    (void)kinds_of(std::string{"\""} + static_cast<char>(0xC3) + "\"", diagnostics);
    REQUIRE(diagnostics.has_errors());
}

TEST_CASE("rejects invalid character escapes") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto kinds = kinds_of("'\\q'", diagnostics);
    REQUIRE(diagnostics.has_errors());
    REQUIRE(kinds.front() == argos::lexer::TokenKind::Invalid);
}

TEST_CASE("reserves declaration and control keywords from the grammar") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto kinds = kinds_of("protocol User(); match value {} defer {} extern C function f();", diagnostics);
    REQUIRE_FALSE(diagnostics.has_errors());
    REQUIRE(kinds[0] == argos::lexer::TokenKind::KwProtocol);
    REQUIRE(std::find(kinds.begin(), kinds.end(), argos::lexer::TokenKind::KwMatch) != kinds.end());
    REQUIRE(std::find(kinds.begin(), kinds.end(), argos::lexer::TokenKind::KwDefer) != kinds.end());
    REQUIRE(std::find(kinds.begin(), kinds.end(), argos::lexer::TokenKind::KwExtern) != kinds.end());
}

TEST_CASE("lexes construct and accessors without legacy constructor keyword") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto kinds = kinds_of("construct getter setter init get set", diagnostics);
    using K = argos::lexer::TokenKind;
    REQUIRE_FALSE(diagnostics.has_errors());
    REQUIRE(kinds == std::vector<K>{K::KwConstruct, K::KwGetter, K::KwSetter,
                                   K::Identifier, K::Identifier, K::Identifier, K::EndOfFile});
}
