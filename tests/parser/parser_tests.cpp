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
        "var x: int = 42;\nvar mut y: string? = nil;",
        diagnostics
    );

    REQUIRE_FALSE(diagnostics.has_errors());
    REQUIRE(program.items.size() == 2);

    const auto* decl1 = dynamic_cast<const argos::ast::VariableDeclaration*>(program.items[0].get());
    REQUIRE(decl1 != nullptr);
    REQUIRE(decl1->name == "x");
    REQUIRE(decl1->mutability == argos::ast::Mutability::immutable);
    REQUIRE(decl1->type->name == "int");
    REQUIRE(decl1->type->nullable_depth == 0);

    const auto* decl2 = dynamic_cast<const argos::ast::VariableDeclaration*>(program.items[1].get());
    REQUIRE(decl2 != nullptr);
    REQUIRE(decl2->name == "y");
    REQUIRE(decl2->mutability == argos::ast::Mutability::mutable_);
    REQUIRE(decl2->type->name == "string");
    REQUIRE(decl2->type->nullable_depth == 1);
}

TEST_CASE("parses function declaration with return type and body") {
    argos::diagnostics::DiagnosticEngine diagnostics;

    const auto program = parse(
        "function add(a: int, b: int) -> int { return a + b; }",
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
    REQUIRE(fn->return_type->name == "int");
    REQUIRE(fn->body != nullptr);
    REQUIRE(fn->body->items.size() == 1);
}

TEST_CASE("rejects the removed func declaration spelling") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    (void)parse("func main() -> int { return 0; }", diagnostics);
    REQUIRE(diagnostics.has_errors());
}

TEST_CASE("parses operator precedence and null-safety operators") {
    argos::diagnostics::DiagnosticEngine diagnostics;

    const auto program = parse(
        "var res: string = user?.profile?.name ?? \"Default\";",
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
        "function main() -> void {\n"
        "    var mut i: byte = 0;\n"
        "    while i < 10 {\n"
        "        i = i + 1;\n"
        "    }\n"
        "    unsafe {\n"
        "        var ptr: usize = 0x24;\n"
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
    const auto program = parse("var result = left || right ?? fallback;", diagnostics);
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
    (void)parse("var plain = optional!;", diagnostics);
    REQUIRE(diagnostics.has_errors());
}

TEST_CASE("parses classes without losing following declarations") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto program = parse("class User {} var x: int = 2;", diagnostics);
    REQUIRE_FALSE(diagnostics.has_errors());
    REQUIRE(program.items.size() == 2);
    REQUIRE(program.items[0]->kind == argos::ast::NodeKind::class_declaration);
}

TEST_CASE("reports unsupported match statements") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto program = parse("match value { default => 1; } var x: int = 2;", diagnostics);
    REQUIRE(diagnostics.has_errors());
    REQUIRE(program.items.size() == 1);
}

TEST_CASE("parses immutable data and rejects removed protocol") {
  argos::diagnostics::DiagnosticEngine diagnostics;
  const auto program = parse("data User(id: int, name: string?);", diagnostics);
  REQUIRE_FALSE(diagnostics.has_errors());
  const auto *data = dynamic_cast<const argos::ast::ClassDeclaration *>(
      program.items[0].get());
  REQUIRE(data != nullptr);
  REQUIRE(data->is_data);
  REQUIRE(data->modifiers.final_);
  REQUIRE(data->members.size() == 3);
  const auto *field = dynamic_cast<const argos::ast::VariableDeclaration *>(
      data->members[1].get());
  REQUIRE(field->type->nullable_depth == 1);
  argos::diagnostics::DiagnosticEngine removed;
  (void)parse("protocol User(name: string);", removed);
  REQUIRE(removed.has_errors());
}

TEST_CASE("accepts escaped identifiers in declarations") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto program = parse("var `class`: int = 1;", diagnostics);
    REQUIRE_FALSE(diagnostics.has_errors());
    const auto *decl = dynamic_cast<const argos::ast::VariableDeclaration *>(program.items[0].get());
    REQUIRE(decl != nullptr);
    REQUIRE(decl->name == "class");
}

TEST_CASE("semicolon is optional before a closing brace or EOF") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto program = parse("function f() -> int { return 1 }\nvar x: int = 1", diagnostics);
    REQUIRE_FALSE(diagnostics.has_errors());
    REQUIRE(program.items.size() == 2);
}

TEST_CASE("single statement bodies may omit braces") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto program = parse(
        "function choose(flag: bool) -> int if flag return 1 else return 2\n"
        "function count() -> int { var mut x: int = 0; while x < 3 x = x + 1; return x }",
        diagnostics
    );
    REQUIRE_FALSE(diagnostics.has_errors());
    REQUIRE(program.items.size() == 2);
}

TEST_CASE("same-line statements still require a separator") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    (void)parse("function f() -> int { var x: int = 1 return x }", diagnostics);
    REQUIRE(diagnostics.has_errors());
}

TEST_CASE("parses computed properties and typed accessor signatures") {
    argos::diagnostics::DiagnosticEngine diagnostics;
    const auto program = parse(R"(
      class Account {
        construct(_ initial: int) {}
        public var mut balance: int {
          getter() { return 1; }
          setter(incoming) { print(incoming); }
        }
        var mut flag: bool { getter() { return true; } setter() { print(value); } }
      }
    )", diagnostics);
    REQUIRE_FALSE(diagnostics.has_errors());
    const auto& type = static_cast<const argos::ast::ClassDeclaration&>(*program.items[0]);
    const auto& constructor = static_cast<const argos::ast::FunctionDeclaration&>(*type.members[0]);
    REQUIRE(constructor.name == "construct");
    const auto& property = static_cast<const argos::ast::VariableDeclaration&>(*type.members[1]);
    REQUIRE(property.is_property);
    REQUIRE(property.getter->parameters.empty());
    REQUIRE(property.getter->return_type->name == "int");
    REQUIRE(property.setter->parameters.size() == 1);
    REQUIRE(property.setter->parameters[0]->name == "incoming");
    REQUIRE(property.setter->parameters[0]->type->name == "int");
    REQUIRE(property.setter->return_type->name == "void");
    const auto& flag = static_cast<const argos::ast::VariableDeclaration&>(*type.members[2]);
    REQUIRE(flag.setter->parameters[0]->name == "value");
}

TEST_CASE("rejects old constructors and malformed accessor syntax") {
    for (const auto& source : {
        "class A { init() {} }", "class A { public init() {} }",
        "class A { var x: int { get { return 1; } } }",
        "class A { var mut x: int { setter() {} } }",
        "class A { var x: int { getter() { return 1; } setter() {} } }",
        "class A { var x: int { getter() { return 1; } getter() { return 2; } } }",
        "class A { var x: int { getter(value) { return 1; } } }",
        "class A { var mut x: int { getter() { return 1; } setter(a, b) {} } }",
        "class A { var x: int = 0 { getter() { return 1; } } }",
        "var x: int { getter() { return 1; } }"
    }) {
        CAPTURE(source);
        argos::diagnostics::DiagnosticEngine diagnostics;
        (void)parse(source, diagnostics);
        REQUIRE(diagnostics.has_errors());
    }
}

TEST_CASE(
    "applies declaration visibility defaults and nominal relation clauses") {
  argos::diagnostics::DiagnosticEngine diagnostics;
  const auto program = parse(R"(
      class A { function hidden() {} public function visible() {} }
      function top() {}
      interface I { function visible(); private function hidden(); }
      sealed class Base permited Child {}
      class Child: Base by I { public function visible() {} function hidden() {} }
      data Person(name: string) permited Child;
    )",
                             diagnostics);
  REQUIRE_FALSE(diagnostics.has_errors());
  const auto *a = dynamic_cast<const argos::ast::ClassDeclaration *>(
      program.items[0].get());
  REQUIRE(a->modifiers.visibility == argos::ast::Visibility::public_);
  REQUIRE(a->members[0]->modifiers.visibility ==
          argos::ast::Visibility::private_);
  REQUIRE(a->members[1]->modifiers.visibility ==
          argos::ast::Visibility::public_);
  const auto *top = dynamic_cast<const argos::ast::FunctionDeclaration *>(
      program.items[1].get());
  REQUIRE(top->modifiers.visibility == argos::ast::Visibility::public_);
  const auto *interface = dynamic_cast<const argos::ast::ClassDeclaration *>(
      program.items[2].get());
  REQUIRE(interface->modifiers.visibility == argos::ast::Visibility::public_);
  REQUIRE(interface->members[0]->modifiers.visibility ==
          argos::ast::Visibility::public_);
  REQUIRE(interface->members[1]->modifiers.visibility ==
          argos::ast::Visibility::private_);
  const auto *base = dynamic_cast<const argos::ast::ClassDeclaration *>(
      program.items[3].get());
  REQUIRE(base->modifiers.sealed_);
  REQUIRE(base->permited == std::vector<std::string>{"Child"});
  const auto *child = dynamic_cast<const argos::ast::ClassDeclaration *>(
      program.items[4].get());
  REQUIRE(child->interfaces == std::vector<std::string>{"I"});
  REQUIRE(child->bases == std::vector<std::string>{"Base", "I"});
  const auto *data = dynamic_cast<const argos::ast::ClassDeclaration *>(
      program.items[5].get());
  REQUIRE(data->modifiers.visibility == argos::ast::Visibility::public_);
  REQUIRE(data->members[0]->modifiers.sealed_);
  argos::diagnostics::DiagnosticEngine old_spelling;
  (void)parse("sealed class A permitted B {}", old_spelling);
  REQUIRE(old_spelling.has_errors());
}

TEST_CASE("parses packages nested visibility components and body record "
          "declarations") {
  argos::diagnostics::DiagnosticEngine diagnostics;
  const auto program = parse(R"(
      package br.argos;
      default class Outer { private class Inner {} protected class Protected {} }
      class Car with br.engine.Motor, GPS {}
      data Person permited Controller { name: string; age: uint_8; }
      class Controller {}
    )",
                             diagnostics);
  REQUIRE_FALSE(diagnostics.has_errors());
  REQUIRE(program.package_name == "br.argos");
  const auto *outer = dynamic_cast<const argos::ast::ClassDeclaration *>(
      program.items[0].get());
  REQUIRE(outer->name == "br.argos.Outer");
  REQUIRE(outer->modifiers.visibility == argos::ast::Visibility::package_);
  const auto *inner = dynamic_cast<const argos::ast::ClassDeclaration *>(
      outer->members[0].get());
  REQUIRE(inner->name == "br.argos.Outer.Inner");
  REQUIRE(inner->enclosing_type == outer->name);
  const auto *car = dynamic_cast<const argos::ast::ClassDeclaration *>(
      program.items[1].get());
  REQUIRE(car->components.size() == 2);
  REQUIRE(car->bases.empty());
  const auto *data = dynamic_cast<const argos::ast::ClassDeclaration *>(
      program.items[2].get());
  REQUIRE(data->members.size() == 3);
  const auto *constructor =
      dynamic_cast<const argos::ast::FunctionDeclaration *>(
          data->members.back().get());
  REQUIRE(constructor->parameters.size() == 2);
  REQUIRE(constructor->modifiers.visibility == argos::ast::Visibility::public_);
}
