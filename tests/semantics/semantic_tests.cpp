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

TEST_CASE("resolves intrinsic string and inferred variables") {
    REQUIRE(check_source("var text = \"hello\"; var other: string? = text;").valid);
    REQUIRE(check_source("var number = 12; var copy: int = number;").valid);
}

TEST_CASE("fits integer literals contextually without widening variables") {
    REQUIRE(check_source("var byteValue: byte = 255; var value: uint = 4294967295;"
                         "var sum: byte = byteValue + 1;").valid);
    REQUIRE_FALSE(check_source("var byteValue: byte = 256;").valid);
    REQUIRE_FALSE(check_source("var huge = 99999999999999;").valid);
    REQUIRE_FALSE(check_source("var byteValue: byte = 1; var count: int = byteValue;").valid);
}

TEST_CASE("converts integer and real values when initializing real variables") {
    REQUIRE(check_source("var fromInt: float = 2;"
                         "var fromDouble: float = 2.0;"
                         "var mut assigned: float = 1;"
                         "assigned = 2.0;").valid);
}

TEST_CASE("rejects unknown and uninferrable types") {
    REQUIRE(has_code(check_source("var user: User? = nil;"), "ARGOS-S0400"));
    REQUIRE(has_code(check_source("var value = nil;"), "ARGOS-S0102"));
    REQUIRE(has_code(check_source("var impossible: never;"), "ARGOS-S0100"));
}

TEST_CASE("preserves optional depth and rejects implicit unwrap") {
    REQUIRE(check_source("var value: int? = 1; var nested: int?? = value;").valid);
    REQUIRE_FALSE(check_source("var value: int? = 1; var plain: int = value;").valid);
    REQUIRE(has_code(check_source("var number: int? = nil; var text: string? = nil;"
                                  "var same = number ?== text;"), "ARGOS-S0329"));
}

TEST_CASE("requires return on every reachable branch") {
    REQUIRE(check_source("function choose(flag: bool) -> int if flag return 1 else return 2").valid);
    REQUIRE(check_source("function choose(flag: bool) -> int { if flag { return 1; } else { return 2; } }").valid);
    REQUIRE(has_code(check_source("function choose(flag: bool) -> int { if flag { return 1; } }"),
                     "ARGOS-S0202"));
}

TEST_CASE("checks external parameter labels") {
    REQUIRE(check_source("function add(_ a: int, rhs b: int) -> int { return a + b; }"
                         "var sum = add(1, rhs: 2);").valid);
    REQUIRE(has_code(check_source("function add(_ a: int, rhs b: int) -> int { return a + b; }"
                                  "var sum = add(1, missing: 2);"), "ARGOS-S0345"));
    REQUIRE(has_code(check_source("function f(label a: int, label b: int) -> int { return a + b; }"),
                     "ARGOS-S0347"));
}

TEST_CASE("tracks initialization and one assignment to immutable var") {
    REQUIRE(check_source("var x: int; x = 1; var y = x;").valid);
    REQUIRE(has_code(check_source("var x: int; var y = x;"), "ARGOS-S0301"));
    REQUIRE(has_code(check_source("var x: int; x = 1; x = 2;"), "ARGOS-S0330"));
    REQUIRE(has_code(check_source("var mut x: int; x += 1;"), "ARGOS-S0337"));
}

TEST_CASE("merges initialization across branches and loops") {
    REQUIRE(check_source("var mut x: int; if true { x = 1; } else { x = 2; } var y = x;").valid);
    REQUIRE(has_code(check_source("var mut x: int; if true { x = 1; } var y = x;"),
                     "ARGOS-S0301"));
    REQUIRE(has_code(check_source("var mut x: int; while false { x = 1; } var y = x;"),
                     "ARGOS-S0301"));
}

TEST_CASE("rejects nominal member access until nominal types exist") {
    REQUIRE(has_code(check_source("var text: string? = nil; var n = text?.length;"),
                     "ARGOS-S0352"));
}

TEST_CASE("resolves protocol components and safe member chains") {
    const auto source = "protocol Profile(name: string);"
                        "protocol User(profile: Profile?);"
                        "function display(user: User?) -> string {"
                        "  return user?.profile?.name ?? \"Anonymous\";"
                        "}";
    REQUIRE(check_source(source).valid);
    REQUIRE(has_code(check_source("protocol User(name: string);"
                                  "function f(user: User) -> string { return user.missing; }"),
                     "ARGOS-S0353"));
    REQUIRE(has_code(check_source("protocol User(name: string, name: string);"),
                     "ARGOS-S0500"));
}

TEST_CASE("checks the null-safety example with explicit nominal definitions") {
    const auto source =
        "protocol Profile(name: string);"
        "protocol User(profile: Profile?);"
        "function loadUser() -> User? { return nil; }"
        "function displayName(user: User?) -> string {"
        " var name: string = user?.profile?.name ?? \"Anonymous\";"
        " return name;"
        "}"
        "var mut cachedUser: User? = nil;"
        "cachedUser ?" "?= loadUser();"
        "var mut preferredName: string = \"Unknown\";"
        "preferredName ?= cachedUser?.profile?.name;";
    REQUIRE(check_source(source).valid);
}

TEST_CASE("retains resolved symbol bindings for later phases") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const argos::source::SourceFile source_file{
        .path = std::filesystem::path{"test.argos"},
        .content = "var value: int = 1; var copy = value;"};
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
        .content = "var first: byte = 255; var second: byte = first + 1;"};
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
