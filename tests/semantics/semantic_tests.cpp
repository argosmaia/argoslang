#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/lexer/lexer.hpp"
#include "argos/parser/parser.hpp"
#include "argos/semantics/semantic_analyzer.hpp"
#include "argos/source/source_file.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string>

namespace {

struct CheckResult final {
    bool valid;
    std::vector<std::string> codes;
};

CheckResult check_source(const std::string &source) {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const argos::source::SourceFile source_file{
        .path = std::filesystem::path{"test.argos"}, .content = source};
    argos::lexer::Lexer lexer{source_file, diagnostics};
    argos::parser::Parser parser{lexer.tokenize(), diagnostics};
    auto program = parser.parse_program();
    bool valid = false;
    if (!diagnostics.has_errors()) {
        argos::semantics::SemanticAnalyzer analyzer{diagnostics};
        valid = analyzer.analyze(program);
    }
    std::vector<std::string> codes;
    for (const auto &diagnostic : diagnostics.diagnostics()) codes.push_back(diagnostic.code);
    return {valid && !diagnostics.has_errors(), std::move(codes)};
}

bool has_code(const CheckResult &result, const std::string &code) {
    return std::find(result.codes.begin(), result.codes.end(), code) != result.codes.end();
}

} // namespace

TEST_CASE("resolves intrinsic String and inferred variables") {
    REQUIRE(check_source("let text = \"hello\"; let other: String? = text;").valid);
    REQUIRE(check_source("let number = 12; var copy: Int = number;").valid);
}

TEST_CASE("fits integer literals contextually without widening variables") {
    REQUIRE(check_source("let byte: Byte = 255; let value: UInt = 4294967295;"
                         "let sum: Byte = byte + 1;").valid);
    REQUIRE_FALSE(check_source("let byte: Byte = 256;").valid);
    REQUIRE_FALSE(check_source("let huge = 99999999999999;").valid);
    REQUIRE_FALSE(check_source("let byte: Byte = 1; let count: Int = byte;").valid);
}

TEST_CASE("rejects unknown and uninferrable types") {
    REQUIRE(has_code(check_source("let user: User? = null;"), "ARGOS-S0400"));
    REQUIRE(has_code(check_source("let value = null;"), "ARGOS-S0102"));
    REQUIRE(has_code(check_source("let impossible: Never;"), "ARGOS-S0100"));
}

TEST_CASE("preserves optional depth and rejects implicit unwrap") {
    REQUIRE(check_source("let value: Int? = 1; let nested: Int?? = value;").valid);
    REQUIRE_FALSE(check_source("let value: Int? = 1; let plain: Int = value;").valid);
    REQUIRE(has_code(check_source("let number: Int? = null; let text: String? = null;"
                                  "let same = number ?== text;"), "ARGOS-S0329"));
}

TEST_CASE("requires return on every reachable branch") {
    REQUIRE(check_source("func choose(flag: Bool) -> Int if flag return 1 else return 2").valid);
    REQUIRE(check_source("func choose(flag: Bool) -> Int { if flag { return 1; } else { return 2; } }").valid);
    REQUIRE(has_code(check_source("func choose(flag: Bool) -> Int { if flag { return 1; } }"),
                     "ARGOS-S0202"));
}

TEST_CASE("checks external parameter labels") {
    REQUIRE(check_source("func add(_ a: Int, rhs b: Int) -> Int { return a + b; }"
                         "let sum = add(1, rhs: 2);").valid);
    REQUIRE(has_code(check_source("func add(_ a: Int, rhs b: Int) -> Int { return a + b; }"
                                  "let sum = add(1, missing: 2);"), "ARGOS-S0345"));
    REQUIRE(has_code(check_source("func f(label a: Int, label b: Int) -> Int { return a + b; }"),
                     "ARGOS-S0347"));
}

TEST_CASE("tracks initialization and one assignment to let") {
    REQUIRE(check_source("let x: Int; x = 1; let y = x;").valid);
    REQUIRE(has_code(check_source("let x: Int; let y = x;"), "ARGOS-S0301"));
    REQUIRE(has_code(check_source("let x: Int; x = 1; x = 2;"), "ARGOS-S0330"));
    REQUIRE(has_code(check_source("var x: Int; x += 1;"), "ARGOS-S0337"));
}

TEST_CASE("merges initialization across branches and loops") {
    REQUIRE(check_source("var x: Int; if true { x = 1; } else { x = 2; } let y = x;").valid);
    REQUIRE(has_code(check_source("var x: Int; if true { x = 1; } let y = x;"),
                     "ARGOS-S0301"));
    REQUIRE(has_code(check_source("var x: Int; while false { x = 1; } let y = x;"),
                     "ARGOS-S0301"));
}

TEST_CASE("rejects nominal member access until nominal types exist") {
    REQUIRE(has_code(check_source("let text: String? = null; let n = text?.length;"),
                     "ARGOS-S0352"));
}

TEST_CASE("resolves protocol components and safe member chains") {
    const auto source = "protocol Profile(name: String);"
                        "protocol User(profile: Profile?);"
                        "func display(user: User?) -> String {"
                        "  return user?.profile?.name ?? \"Anonymous\";"
                        "}";
    REQUIRE(check_source(source).valid);
    REQUIRE(has_code(check_source("protocol User(name: String);"
                                  "func f(user: User) -> String { return user.missing; }"),
                     "ARGOS-S0353"));
    REQUIRE(has_code(check_source("protocol User(name: String, name: String);"),
                     "ARGOS-S0500"));
}

TEST_CASE("checks the null-safety example with explicit nominal definitions") {
    const auto source =
        "protocol Profile(name: String);"
        "protocol User(profile: Profile?);"
        "func loadUser() -> User? { return null; }"
        "func displayName(user: User?) -> String {"
        " let name: String = user?.profile?.name ?? \"Anonymous\";"
        " return name;"
        "}"
        "var cachedUser: User? = null;"
        "cachedUser ?" "?= loadUser();"
        "var preferredName: String = \"Unknown\";"
        "preferredName ?= cachedUser?.profile?.name;";
    REQUIRE(check_source(source).valid);
}

TEST_CASE("retains resolved symbol bindings for later phases") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const argos::source::SourceFile source_file{
        .path = std::filesystem::path{"test.argos"},
        .content = "let value: Int = 1; let copy = value;"};
    argos::lexer::Lexer lexer{source_file, diagnostics};
    argos::parser::Parser parser{lexer.tokenize(), diagnostics};
    auto program = parser.parse_program();
    argos::semantics::SemanticAnalyzer analyzer{diagnostics};
    REQUIRE(analyzer.analyze(program));
    const auto *copy = dynamic_cast<const argos::ast::VariableDeclaration *>(program.items[1].get());
    REQUIRE(copy != nullptr);
    const auto binding = analyzer.symbol_id_of(*copy->initializer);
    REQUIRE(binding.has_value());
    const auto *symbol = analyzer.symbol_by_id(*binding);
    REQUIRE(symbol != nullptr);
    REQUIRE(symbol->name == "value");
    REQUIRE(analyzer.type_of(*copy->initializer).kind == argos::semantics::TypeKind::int_type);
}

TEST_CASE("records contextual coercions without changing source types") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const argos::source::SourceFile source_file{
        .path = std::filesystem::path{"test.argos"},
        .content = "let first: Byte = 255; let second: Byte = first + 1;"};
    argos::lexer::Lexer lexer{source_file, diagnostics};
    argos::parser::Parser parser{lexer.tokenize(), diagnostics};
    auto program = parser.parse_program();
    argos::semantics::SemanticAnalyzer analyzer{diagnostics};
    REQUIRE(analyzer.analyze(program));
    const auto *first = dynamic_cast<const argos::ast::VariableDeclaration *>(program.items[0].get());
    const auto *second = dynamic_cast<const argos::ast::VariableDeclaration *>(program.items[1].get());
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    const auto first_coercion = analyzer.coercion_of(*first->initializer);
    REQUIRE(first_coercion.has_value());
    REQUIRE(first_coercion->kind == argos::semantics::TypeKind::byte);
    REQUIRE(analyzer.type_of(*first->initializer).kind == argos::semantics::TypeKind::int_type);
    const auto *binary = dynamic_cast<const argos::ast::BinaryExpression *>(second->initializer.get());
    REQUIRE(binary != nullptr);
    const auto binary_coercion = analyzer.coercion_of(*binary->right);
    REQUIRE(binary_coercion.has_value());
    REQUIRE(binary_coercion->kind == argos::semantics::TypeKind::byte);
}
