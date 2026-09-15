#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/lexer/lexer.hpp"
#include "argos/parser/parser.hpp"
#include "argos/source/source_file.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>

namespace {

argos::ast::Program parse(
    const std::string& source,
    argos::diagnostics::DiagnosticEngine& diagnostics
) {
    const argos::source::SourceFile source_file{
        .path = std::filesystem::path{"test.argos"},
        .content = source,
    };

    argos::lexer::Lexer lexer{source_file, diagnostics};
    auto tokens = lexer.tokenize();

    argos::parser::Parser parser{std::move(tokens), diagnostics};
    return parser.parse_program();
}

} // namespace

TEST_CASE("parses variable declarations") {
    argos::diagnostics::DiagnosticEngine diagnostics;

    const auto program = parse(
        "let x: Int = 42;\nvar y: String? = null;",
        diagnostics
    );

    REQUIRE_FALSE(diagnostics.has_errors());
    REQUIRE(program.items.size() == 2);

    const auto* decl1 = dynamic_cast<const argos::ast::VariableDeclaration*>(program.items[0].get());
    REQUIRE(decl1 != nullptr);
    REQUIRE(decl1->name == "x");
    REQUIRE(decl1->mutability == argos::ast::Mutability::immutable);
    REQUIRE(decl1->type->name == "Int");
    REQUIRE(decl1->type->nullable_depth == 0);

    const auto* decl2 = dynamic_cast<const argos::ast::VariableDeclaration*>(program.items[1].get());
    REQUIRE(decl2 != nullptr);
    REQUIRE(decl2->name == "y");
    REQUIRE(decl2->mutability == argos::ast::Mutability::mutable_);
    REQUIRE(decl2->type->name == "String");
    REQUIRE(decl2->type->nullable_depth == 1);
}

TEST_CASE("parses function declaration with return type and body") {
    argos::diagnostics::DiagnosticEngine diagnostics;

    const auto program = parse(
        "func add(a: Int, b: Int) -> Int { return a + b; }",
        diagnostics
    );

    REQUIRE_FALSE(diagnostics.has_errors());
    REQUIRE(program.items.size() == 1);

    const auto* fn = dynamic_cast<const argos::ast::FunctionDeclaration*>(program.items[0].get());
    REQUIRE(fn != nullptr);
    REQUIRE(fn->name == "add");
    REQUIRE(fn->parameters.size() == 2);
    REQUIRE(fn->parameters[0]->name == "a");
    REQUIRE(fn->parameters[1]->name == "b");
    REQUIRE(fn->return_type != nullptr);
    REQUIRE(fn->return_type->name == "Int");
    REQUIRE(fn->body != nullptr);
    REQUIRE(fn->body->items.size() == 1);
}

TEST_CASE("parses operator precedence and null-safety operators") {
    argos::diagnostics::DiagnosticEngine diagnostics;

    const auto program = parse(
        "let res: String = user?.profile?.name ?? \"Default\";",
        diagnostics
    );

    REQUIRE_FALSE(diagnostics.has_errors());
    REQUIRE(program.items.size() == 1);

    const auto* decl = dynamic_cast<const argos::ast::VariableDeclaration*>(program.items[0].get());
    REQUIRE(decl != nullptr);
    REQUIRE(decl->initializer != nullptr);
    REQUIRE(decl->initializer->kind == argos::ast::NodeKind::binary_expression);

    const auto* binary = dynamic_cast<const argos::ast::BinaryExpression*>(decl->initializer.get());
    REQUIRE(binary != nullptr);
    REQUIRE(binary->op == argos::ast::BinaryOperator::null_coalescing);
}

TEST_CASE("parses while loops and unsafe blocks") {
    argos::diagnostics::DiagnosticEngine diagnostics;

    const auto program = parse(
        "func main() -> Void {\n"
        "    var i: Byte = 0;\n"
        "    while i < 10 {\n"
        "        i = i + 1;\n"
        "    }\n"
        "    unsafe {\n"
        "        let ptr: USize = 0x24;\n"
        "    }\n"
        "}",
        diagnostics
    );

    REQUIRE_FALSE(diagnostics.has_errors());
    REQUIRE(program.items.size() == 1);

    const auto* fn = dynamic_cast<const argos::ast::FunctionDeclaration*>(program.items[0].get());
    REQUIRE(fn != nullptr);
    REQUIRE(fn->body != nullptr);
    REQUIRE(fn->body->items.size() == 3);

    const auto* whileStmt = dynamic_cast<const argos::ast::WhileStatement*>(fn->body->items[1].get());
    REQUIRE(whileStmt != nullptr);

    const auto* unsafeStmt = dynamic_cast<const argos::ast::UnsafeBlockStatement*>(fn->body->items[2].get());
    REQUIRE(unsafeStmt != nullptr);
    REQUIRE(unsafeStmt->block->items.size() == 1);
}

TEST_CASE("uses normative coalescing precedence") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto program = parse("let result = left || right ?? fallback;", diagnostics);
    REQUIRE_FALSE(diagnostics.has_errors());
    const auto *decl = dynamic_cast<const argos::ast::VariableDeclaration *>(program.items[0].get());
    REQUIRE(decl != nullptr);
    const auto *coalesce = dynamic_cast<const argos::ast::BinaryExpression *>(decl->initializer.get());
    REQUIRE(coalesce != nullptr);
    REQUIRE(coalesce->op == argos::ast::BinaryOperator::null_coalescing);
    const auto *logical = dynamic_cast<const argos::ast::BinaryExpression *>(coalesce->left.get());
    REQUIRE(logical != nullptr);
    REQUIRE(logical->op == argos::ast::BinaryOperator::logical_or);
}

TEST_CASE("rejects legacy postfix bang") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    (void)parse("let plain = optional!;", diagnostics);
    REQUIRE(diagnostics.has_errors());
}

TEST_CASE("reports unsupported nominal declarations") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto program = parse("class User {} let x: Int = 2;", diagnostics);
    REQUIRE(diagnostics.has_errors());
    REQUIRE(program.items.size() == 1);
}

TEST_CASE("reports unsupported match statements") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto program = parse("match value { default => 1; } let x: Int = 2;", diagnostics);
    REQUIRE(diagnostics.has_errors());
    REQUIRE(program.items.size() == 1);
}

TEST_CASE("parses a non-generic data protocol") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto program = parse("protocol User(id: UInt, name: String?);", diagnostics);
    REQUIRE_FALSE(diagnostics.has_errors());
    const auto *protocol = dynamic_cast<const argos::ast::ProtocolDeclaration *>(program.items[0].get());
    REQUIRE(protocol != nullptr);
    REQUIRE(protocol->name == "User");
    REQUIRE(protocol->components.size() == 2);
    REQUIRE(protocol->components[1]->type->nullable_depth == 1);
}

TEST_CASE("accepts escaped identifiers in declarations") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto program = parse("let `class`: Int = 1;", diagnostics);
    REQUIRE_FALSE(diagnostics.has_errors());
    const auto *decl = dynamic_cast<const argos::ast::VariableDeclaration *>(program.items[0].get());
    REQUIRE(decl != nullptr);
    REQUIRE(decl->name == "class");
}

TEST_CASE("semicolon is optional before a closing brace or EOF") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto program = parse("func f() -> Int { return 1 }\nlet x: Int = 1", diagnostics);
    REQUIRE_FALSE(diagnostics.has_errors());
    REQUIRE(program.items.size() == 2);
}

TEST_CASE("single statement bodies may omit braces") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto program = parse(
        "func choose(flag: Bool) -> Int if flag return 1 else return 2\n"
        "func count() -> Int { var x: Int = 0; while x < 3 x = x + 1; return x }",
        diagnostics
    );
    REQUIRE_FALSE(diagnostics.has_errors());
    REQUIRE(program.items.size() == 2);
}

TEST_CASE("same-line statements still require a separator") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    (void)parse("func f() -> Int { let x: Int = 1 return x }", diagnostics);
    REQUIRE(diagnostics.has_errors());
}
