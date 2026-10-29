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

CheckResult check_units(std::initializer_list<std::string> sources) {
  argos::diagnostics::DiagnosticEngine diagnostics;
  argos::ast::Program combined{{}, {}};
  std::size_t index = 0;
  for (const auto &source : sources) {
    const argos::source::SourceFile file{
        .path =
            std::filesystem::path{"unit" + std::to_string(index++) + ".argos"},
        .content = source};
    argos::lexer::Lexer lexer{file, diagnostics};
    argos::parser::Parser parser{lexer.tokenize(), diagnostics};
    auto unit = parser.parse_program();
    for (auto &item : unit.items)
      combined.items.push_back(std::move(item));
  }
  argos::semantics::SemanticAnalyzer analyzer{diagnostics};
  const bool valid = !diagnostics.has_errors() && analyzer.analyze(combined);
  std::vector<std::string> codes;
  for (const auto &diagnostic : diagnostics.diagnostics())
    codes.push_back(diagnostic.code);
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

TEST_CASE("checks string values and boolean short-circuit operands") {
    REQUIRE(check_source("function identity(_ value: string) -> string {"
                         " return value; }"
                         "var text: string = identity(\"hello\");"
                         "var same: bool = text == \"hello\";"
                         "var result: bool = false && same || true;").valid);
    REQUIRE_FALSE(check_source("var result: bool = 1 && true;").valid);
}

TEST_CASE("rejects unknown and uninferrable types") {
    REQUIRE(has_code(check_source("var user: User? = nil;"), "ARGOS-S0400"));
    REQUIRE(has_code(check_source("var value: Int = 1;"), "ARGOS-S0400"));
    REQUIRE(has_code(check_source("var value: String = \"hi\";"), "ARGOS-S0400"));
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

TEST_CASE("resolves data components and safe member chains") {
  const auto source =
      "data Profile(name: string) permited Display;"
      "data User(profile: Profile?) permited Display;"
      "class Display { public function display(user: User?) -> string {"
      "  return user?.profile?.name ?? \"Anonymous\";"
      "} }";
  REQUIRE(check_source(source).valid);
  REQUIRE(has_code(
      check_source("data User(name: string);"
                   "function f(user: User) -> string { return user.missing; }"),
      "ARGOS-S0353"));
  REQUIRE(has_code(check_source("data User(name: string, name: string);"),
                   "ARGOS-S0001"));
}

TEST_CASE("checks the null-safety example with explicit nominal definitions") {
  const auto source =
      "data Profile(name: string) { public function getName() -> string { "
      "return name; } }"
      "data User(profile: Profile?) { public function getProfile() -> Profile? "
      "{ return profile; } }"
      "function loadUser() -> User? { return nil; }"
      "function displayName(user: User?) -> string {"
      " var name: string = user?.getProfile()?.getName() ?? \"Anonymous\";"
      " return name;"
      "}"
      "var mut cachedUser: User? = nil;"
      "cachedUser ?"
      "?= loadUser();"
      "var mut preferredName: string = \"Unknown\";"
      "preferredName ?= cachedUser?.getProfile()?.getName();";
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

TEST_CASE("checks classes constructors identity and mutable fields", "[oop]") {
    REQUIRE(check_source(R"(
        class Account {
            public var id: string;
            private var mut balance: int = 0;
            public construct(id: string) { this.id = id; }
            public function deposit(_ n: int) -> int {
                balance = balance + n; return this.balance;
            }
        }
        var a = new Account(id: "one");
        var b: Account = a;
        var result: int = b.deposit(2);
        var same = a == b;
    )").valid);
    REQUIRE(has_code(check_source("class A { var id: int; }"), "ARGOS-S0614"));
    REQUIRE(has_code(check_source("class A { var id: int; construct() {} }"), "ARGOS-S0614"));
    REQUIRE(has_code(check_source("class A { var id: int; construct() { if true return; this.id = 1; } }"), "ARGOS-S0614"));
    REQUIRE(has_code(check_source("class A { var id: int; construct() { this.id = this.id; } }"), "ARGOS-S0301"));
    REQUIRE(has_code(check_source("class A { var id: int; construct() { this.id = 1; this.id = 2; } }"), "ARGOS-S0330"));
    REQUIRE(has_code(check_source("class A { public var id: int = 1; } var a = new A(); a.id = 2;"), "ARGOS-S0330"));
}

TEST_CASE("enforces class access control", "[oop]") {
    REQUIRE(has_code(check_source("class A { private var x: int = 1; } var a = new A(); var x = a.x;"), "ARGOS-S0611"));
    REQUIRE(has_code(check_source("class A { private construct() {} } var a = new A();"), "ARGOS-S0611"));
    REQUIRE(has_code(check_source("class A { public construct() {} } var a = new A(); a.construct();"), "ARGOS-P0001"));
    REQUIRE(has_code(check_source("class A { public function f() -> int { return 1; } } var a = new A(); a.f = 1;"), "ARGOS-S0330"));
}

TEST_CASE("checks interfaces virtual overrides and nominal subtyping", "[oop]") {
  REQUIRE(check_source(R"(
        interface Readable { function read() -> int; }
        abstract class Base by Readable { public abstract function read() -> int; }
        final class Value : Base {
            public final override function read() -> int { return 42; }
        }
        var value: Readable = new Value();
        var n = value.read();
    )")
              .valid);
  REQUIRE(has_code(
      check_source("interface I { function f() -> int; } class A by I {}"),
      "ARGOS-S0609"));
  REQUIRE(has_code(check_source("interface I { var x: int; }"), "ARGOS-S0607"));
  REQUIRE(has_code(check_source("abstract class A {} var a = new A();"),
                   "ARGOS-S0602"));
  REQUIRE(
      has_code(check_source("interface I {} var i = new I();"), "ARGOS-S0602"));
  REQUIRE(
      has_code(check_source("final class A {} class B : A {}"), "ARGOS-S0605"));
  REQUIRE(
      has_code(check_source("class A : B {} class B : A {}"), "ARGOS-S0603"));
  REQUIRE(has_code(check_source("class A {} class B {} class C : A, B {}"),
                   "ARGOS-S0604"));
  REQUIRE(has_code(check_source("class A { public function f() {} } class B : "
                                "A { override function f() {} }"),
                   "ARGOS-S0608"));
  REQUIRE(has_code(check_source("class A { public virtual function f() {} } "
                                "class B : A { public function f() {} }"),
                   "ARGOS-S0608"));
  REQUIRE(has_code(check_source("class A { override function f() {} }"),
                   "ARGOS-S0608"));
  REQUIRE(
      has_code(check_source("interface I { function f(); } class A { internal "
                            "function f() {} } class B : A by I {}"),
               "ARGOS-S0606"));
}

TEST_CASE("supports structurally typed higher order functions and closures", "[functional]") {
    REQUIRE(check_source(R"(
        function apply(_ f: (int) -> int, _ x: int) -> int { return f(x); }
        function make(_ n: int) -> (int) -> int { return (x: int) => x + n; }
        function increment(_ x: int) -> int { return x + 1; }
        var f: (int) -> int = make(2);
        var n = apply(f, 4);
        var m = apply(increment, 5);
        var twice = (f: (int) -> int) -> (int) -> int => (x: int) => f(f(x));
        var result: int = twice(increment)(2);
    )").valid);
    REQUIRE_FALSE(check_source("var f: (bool) -> int = (x: int) => x + 1;").valid);
    REQUIRE_FALSE(check_source("var f = (x: int) => x + 1; var n = f(true);").valid);
    REQUIRE(has_code(check_source("var f = (x) => x;"), "ARGOS-S0612"));
    REQUIRE(has_code(check_source("var f = () => nil;"), "ARGOS-S0612"));
    REQUIRE(has_code(check_source("var f = () -> int => true;"), "ARGOS-S0201"));
    REQUIRE(has_code(check_source("var f = () -> int => { if true return 1; };"), "ARGOS-S0202"));
}

TEST_CASE("isolates closure capture initialization and loop control", "[functional]") {
    REQUIRE(check_source("var x = 2; var f = () => x;").valid);
    REQUIRE(has_code(check_source("var mut x = 2; var f = () => x;"), "ARGOS-S0613"));
    REQUIRE(has_code(check_source("var x: int; var f = () => { x = 2; }; var y = x;"), "ARGOS-S0613"));
    REQUIRE(has_code(check_source("var x: int; var f = () => x;"), "ARGOS-S0301"));
    REQUIRE(has_code(check_source("while true { var f = () => { break; }; }"), "ARGOS-S0203"));
    REQUIRE(has_code(check_source("while true { var f = () => { continue; }; }"), "ARGOS-S0204"));
}

TEST_CASE("rejects contradictory member modifiers and unsafe field defaults", "[oop]") {
  REQUIRE(check_source("interface I { private function f(); }").valid);
  REQUIRE(has_code(check_source("class A { private abstract function f(); }"),
                   "ARGOS-S0600"));
  REQUIRE(has_code(check_source("class A { final virtual function f() {} }"),
                   "ARGOS-S0600"));
  REQUIRE(
      has_code(check_source("class A { var x: int = 1; var y: int = this.x; }"),
               "ARGOS-S0614"));
  REQUIRE(has_code(check_source("var value = print(1);"), "ARGOS-S0100"));
}

TEST_CASE("prevents observation of partially initialized instances", "[oop]") {
    REQUIRE(has_code(check_source(R"(
      class A {
        var x: int;
        construct() { this.read(); this.x = 1; }
        function read() -> int { return x; }
      }
    )"), "ARGOS-S0614"));
    REQUIRE(has_code(check_source(R"(
      function observe(_ a: A) -> A { return a; }
      class A { var x: int; construct() { var alias = observe(this); this.x = 1; } }
    )"), "ARGOS-S0614"));
    REQUIRE(has_code(check_source(R"(
      class B { public var mut value: int = 0; }
      class A { var child: B; construct() { this.child.value = 1; this.child = new B(); } }
    )"), "ARGOS-S0301"));
    REQUIRE(has_code(check_source(R"(
      class A {
        var x: int;
        construct() { var read = () => this.x; var n = read(); this.x = 1; }
      }
    )"), "ARGOS-S0613"));

}

TEST_CASE("construct replaces old constructor methods without compatibility", "[oop]") {
    REQUIRE(check_source("class A { var x: int; construct(_ x: int) { this.x = x; } } var a = new A(1);").valid);
    REQUIRE_FALSE(check_source("class A { init() {} } var a = new A();").valid);
    REQUIRE_FALSE(check_source("class A { public init() {} } var a = new A();").valid);
    REQUIRE(has_code(check_source("class A { function init() {} }"), "ARGOS-S0600"));
    REQUIRE(has_code(check_source("class A { construct() -> int { return 1; } }"), "ARGOS-S0600"));
    REQUIRE(has_code(check_source("class A { var required: int; var `construct`: int = 1; }"), "ARGOS-S0600"));
}

TEST_CASE("checks property accessors and rejects writes to getter only properties", "[oop][accessors]") {
    REQUIRE(check_source(R"(
      class Counter {
        private var mut stored: int = 0;
        construct(_ x: int) { stored = x; }
        public var mut value: int {
          getter() { return stored; }
          setter(incoming) { stored = incoming; }
        }
        public var doubled: int { getter() { return value * 2; } }
        function increase() { value += 1; }
      }
      var counter = new Counter(2);
      counter.value = 3;
      counter.value += 4;
      var n: int = counter.doubled;
    )").valid);
    REQUIRE(has_code(check_source("class A { public var x: int { getter() { return 1; } } } var a = new A(); a.x = 2;"), "ARGOS-S0330"));
    REQUIRE(has_code(check_source("class A { public var mut x: int { getter() { return 1; } } } var a = new A(); a.x += 2;"), "ARGOS-S0330"));
    REQUIRE(has_code(check_source("class A { var x: int { getter() { return true; } } }"), "ARGOS-S0201"));
    REQUIRE(has_code(check_source("class A { var x: int { getter() {} } }"), "ARGOS-S0202"));
    REQUIRE(has_code(check_source("class A { var mut x: int { getter() { return 1; } setter() { return value; } } }"), "ARGOS-S0201"));
    REQUIRE(has_code(check_source("class A { var x: int { getter(); } }"), "ARGOS-S0601"));
    REQUIRE(has_code(check_source("class A { private var x: int { getter() { return 1; } } } var a = new A(); var n = a.x;"), "ARGOS-S0611"));
    REQUIRE(has_code(check_source("class A { var field: int; construct() { this.x = 1; field = 2; } var mut x: int { getter() { return field; } setter() { field = value; } } }"), "ARGOS-S0614"));
    REQUIRE(has_code(check_source("class A { var mut x: int { getter() { return 1; } setter() {} } } var a = new A(); a.x /= 2;"), "ARGOS-S0331"));
}

TEST_CASE("checks interface property contracts and required setters", "[oop][accessors]") {
  REQUIRE(check_source(R"(
      interface Named { var mut name: string { getter(); setter(); } }
      class Item by Named {
        private var mut stored: string = "";
        public var mut name: string {
          getter() { return stored; }
          setter() { stored = value; }
        }
      }
      var item: Named = new Item();
      item.name = "new";
    )")
              .valid);
  REQUIRE(has_code(
      check_source("interface I { var x: int { getter(); } } class A by I {}"),
      "ARGOS-S0609"));
  REQUIRE(has_code(
      check_source(
          "interface I { var mut x: int { getter(); setter(); } } class A by I "
          "{ public var x: int { getter() { return 1; } } }"),
      "ARGOS-S0606"));
  REQUIRE(has_code(
      check_source("interface I { var x: int { getter() { return 1; } } }"),
      "ARGOS-S0607"));
  REQUIRE(has_code(
      check_source("interface R { var x: int { getter(); } } interface W { var "
                   "mut x: int { getter(); setter(); } } class A by R, W { "
                   "public var x: int { getter() { return 1; } } }"),
      "ARGOS-S0606"));
}

TEST_CASE("null safety enforces optional boundaries", "[optional]") {
  REQUIRE(check_source("var a: int? = 0; var yes: bool = a ?== 0; var mut b: "
                       "int = 1; b !!= a;")
              .valid);
  REQUIRE(check_source("var a: void? = nil;").valid == false);
  REQUIRE_FALSE(check_source("var a: int = nil;").valid);
  REQUIRE_FALSE(check_source("var a: int? = 1; var b: int = a;").valid);
  REQUIRE_FALSE(check_source("var a: int?; var b = a ?? 0;").valid);
  REQUIRE_FALSE(check_source("var a: int? = nil; a ?"
                             "?= 1;")
                    .valid);
  REQUIRE_FALSE(check_source("var mut a: int = 1; a !!= 2;").valid);
  REQUIRE_FALSE(
      check_source("var mut a: int = 1; var b: string? = nil; a !!= b;").valid);
  REQUIRE_FALSE(check_source("var a: int? = 1; if a { }").valid);
  REQUIRE_FALSE(check_source("var a: int? = 1; var b = a + 1;").valid);
  REQUIRE_FALSE(check_source("var a: int = 1; var b = a!!;").valid);
  REQUIRE_FALSE(check_source("var a: int? = nil; var b = a?;").valid);
}

TEST_CASE("optional arrays use typed safe subscripts", "[optional][array]") {
  REQUIRE(
      check_source("var a: [int]? = [1, 2]; var b: int = a?[0] ?? 3;").valid);
  REQUIRE(
      check_source("var a: [int?]? = [nil, 2]; var b: int? = a?[0];").valid);
  REQUIRE_FALSE(check_source("var a: [int]? = nil; var b = a[0];").valid);
  REQUIRE_FALSE(check_source("var a = [1, 2]; var b = a?[0];").valid);
  REQUIRE_FALSE(check_source("var a: [int]? = nil; var b = a?[true];").valid);
  REQUIRE_FALSE(check_source("var a: [int]? = nil; a?[0] = 2;").valid);
  REQUIRE_FALSE(check_source("var a = [1, 2]; a[0] = 2;").valid);
  REQUIRE_FALSE(check_source("var a: [void]? = nil;").valid);
  REQUIRE_FALSE(check_source("var a: [int] = [1, true];").valid);
}

TEST_CASE("lazy optional paths do not establish definite initialization",
          "[optional]") {
  REQUIRE_FALSE(
      check_source(
          "var y: int; var a: int? = 1; var z = a ?? (y = 2); print(y);")
          .valid);
  REQUIRE_FALSE(check_source("var y: int; var mut a: int? = nil; a ?"
                             "?= (y = 2); print(y);")
                    .valid);
  REQUIRE_FALSE(
      check_source(
          "var y: int; var a: [int]? = nil; var z = a?[(y = 0)]; print(y);")
          .valid);
  REQUIRE_FALSE(check_source("var mut a: [int]; a[0] = 1;").valid);
  REQUIRE_FALSE(
      check_source(
          "class C { public function f(_ x: int) -> int { return x; } } var c: "
          "C? = nil; var y: int; var z = c?.f(y = 1); print(y);")
          .valid);
  REQUIRE_FALSE(
      check_source("var mut x: int?; var y: int? = 1; x ?= y;").valid);
}

TEST_CASE(
    "optional arrays allow multiline index and explicit unwrap for writing",
    "[optional][array]") {
  REQUIRE(
      check_source("var mut a: [int]? = [1, 2]; a!![0] = 3; var b = a?[\n0\n];")
          .valid);
  REQUIRE(check_source("var a: [int] = []; function f(_ x: [int]) -> [int] { "
                       "return []; } var b = f([]);")
              .valid);
  REQUIRE_FALSE(check_source("var a = [];").valid);
  REQUIRE_FALSE(check_source("var a: [[int]]? = nil;").valid);
}

TEST_CASE("enforces overload signatures and resolves exact argument types",
          "[oop][overload]") {
  REQUIRE(check_source(R"(
      function select(_ x: int) -> int { return x; }
      overload function select(_ x: string) -> int { return 2; }
      var a = select(1); var b = select("text");
      class A {
        construct(_ x: int) {}
        overload construct() {}
        public function f(_ x: int) -> int { return x; }
        public overload function f(_ x: bool) -> int { return 2; }
      }
      var a1 = new A(); var result = a1.f(true);
    )")
              .valid);
  REQUIRE(
      has_code(check_source("function f(_ x: int) {} function f(_ x: bool) {}"),
               "ARGOS-S0620"));
  REQUIRE(has_code(check_source("function f(_ x: int) {} overload function f(_ "
                                "x: int) -> int { return 1; }"),
                   "ARGOS-S0001"));
  REQUIRE(has_code(check_source("function f(_ x: int?) {} overload function "
                                "f(_ x: string?) {} f(nil);"),
                   "ARGOS-S0621"));
  REQUIRE(has_code(check_source("function f(_ x: int) {} overload function f(_ "
                                "x: string) {} f(true);"),
                   "ARGOS-S0621"));
  REQUIRE(has_code(check_source("function f(_ x: int) {} overload function f(_ "
                                "x: bool) {} var ref = f;"),
                   "ARGOS-S0621"));
  REQUIRE(has_code(
      check_source("class A { function f(_ x: int) {} overload function f(_ x: "
                   "string) {} } var a = new A(); a.f(true);"),
      "ARGOS-S0621"));
}

TEST_CASE("requires one superclass and a valid construction chain",
          "[oop][super]") {
  REQUIRE(check_source(R"(
      class A { protected var x: int; protected construct(_ n: int) { x = n; } }
      class B : A { construct(_ n: int) { super(n); } function read() -> int { return super.x; } }
      var b = new B(3);
    )")
              .valid);
  REQUIRE(has_code(check_source("class A {} class B {} class C : A, B {}"),
                   "ARGOS-S0604"));
  REQUIRE(has_code(
      check_source("class A { construct(_ n: int) {} } class B : A {}"),
      "ARGOS-S0622"));
  REQUIRE(has_code(
      check_source(
          "class A {} class B : A { construct() { print(1); super(); } }"),
      "ARGOS-S0622"));
  REQUIRE(has_code(
      check_source(
          "class A {} class B : A { construct() { super(); super(); } }"),
      "ARGOS-S0622"));
  REQUIRE(
      has_code(check_source("class A { construct(_ x: int) {} } class B : A { "
                            "var x: int = 1; construct() { super(this.x); } }"),
               "ARGOS-S0622"));
  REQUIRE(has_code(check_source("class A {} function f() { super(); }"),
                   "ARGOS-S0622"));
  REQUIRE(has_code(
      check_source("abstract class A { public abstract function f(); } class B "
                   ": A { public override function f() { super.f(); } }"),
      "ARGOS-S0622"));
  REQUIRE(has_code(check_source("class A {} class B : A { public function f() "
                                "{ var f = () => super; } }"),
                   "ARGOS-S0622"));
}

TEST_CASE("preserves encapsulation through inherited fields and overloads",
          "[oop][visibility]") {
  REQUIRE(has_code(check_source("class A { private var x: int = 1; } class B : "
                                "A { function f() -> int { return x; } }"),
                   "ARGOS-S0611"));
  REQUIRE(has_code(check_source("class A { protected var x: int = 1; } var a = "
                                "new A(); var n = a.x;"),
                   "ARGOS-S0611"));
  REQUIRE(has_code(
      check_source("class A { private construct() {} } class B : A {}"),
      "ARGOS-S0611"));
  REQUIRE(has_code(
      check_source("class A { private function f(_ n: int) {} public overload "
                   "function f(_ s: string) {} } var a = new A(); a.f(1);"),
      "ARGOS-S0611"));
  REQUIRE(
      check_source("class A { private function f(_ n: int) {} public overload "
                   "function f(_ s: string) {} } var a = new A(); a.f(\"ok\");")
          .valid);
  REQUIRE(has_code(check_source("class A { public virtual function f() {} } "
                                "class B : A { override function f() {} }"),
                   "ARGOS-S0608"));
  REQUIRE(has_code(check_source("class A { public function f() {} } class B : "
                                "A { public override function f() {} }"),
                   "ARGOS-S0608"));
  REQUIRE(has_code(check_source("class A { public virtual function f() {} } "
                                "class B : A { public function f() {} }"),
                   "ARGOS-S0608"));
  REQUIRE(
      check_source("interface I { private var x: int { getter(); } }").valid);
}

TEST_CASE("data is final and transitively immutable", "[oop][data]") {
  REQUIRE(check_source(
              "data Point(x: int, label: string?); data Box(point: Point); var "
              "b = new Box(point: new Point(x: 1, label: nil));")
              .valid);
  REQUIRE(
      has_code(check_source("data A { var mut x: int = 1; }"), "ARGOS-S0623"));
  REQUIRE(has_code(check_source("data A(values: [int]);"), "ARGOS-S0623"));
  REQUIRE(has_code(check_source("class Mutable {} data A(value: Mutable);"),
                   "ARGOS-S0623"));
  REQUIRE(
      has_code(check_source("data A(callback: () -> int);"), "ARGOS-S0623"));
  REQUIRE(has_code(check_source("data A(next: A?);"), "ARGOS-S0623"));
  REQUIRE(
      has_code(check_source("data A(x: int); class B : A {}"), "ARGOS-S0605"));
  REQUIRE(has_code(
      check_source("data A(x: int) { function change() { this.x = 2; } }"),
      "ARGOS-S0330"));
  REQUIRE_FALSE(check_source("protocol A(x: int);").valid);
  REQUIRE_FALSE(check_source("class A { @override function f() {} }").valid);
}

TEST_CASE("overload keys distinguish optional functions from optional returns",
          "[oop][overload][optional]") {
  REQUIRE(check_source(R"(
      function f(_ callback: (() -> int)?) -> int { return 1; }
      overload function f(_ callback: () -> int?) -> int { return 2; }
      var optionalFunction: (() -> int)? = nil;
      var returningOptional = () -> int? => nil;
      var first = f(optionalFunction);
      var second = f(returningOptional);
    )")
              .valid);
}

TEST_CASE("constructors cannot dispatch into partially built derived objects",
          "[oop][super][safety]") {
  REQUIRE(has_code(check_source(R"(
      class A {
        var x: int;
        construct() { x = 1; read(); }
        public virtual function read() -> int { return x; }
      }
      class B : A {
        var y: int = 2;
        public override function read() -> int { return y; }
      }
    )"),
                   "ARGOS-S0614"));
  REQUIRE(has_code(check_source(R"(
      function observe(_ a: A) -> int { return a.read(); }
      class A {
        var x: int;
        construct() { x = 1; observe(this); }
        public virtual function read() -> int { return x; }
      }
    )"),
                   "ARGOS-S0614"));
}

TEST_CASE("overload resolution prefers a more specific nominal contract",
          "[oop][overload]") {
  REQUIRE(check_source(R"(
      interface I {}
      class A by I {}
      class B : A {}
      function f(_ a: I) -> int { return 1; }
      overload function f(_ a: A) -> int { return 2; }
      var n = f(new B());
    )")
              .valid);
  REQUIRE(check_source(R"(
      function f(items: [int]) {}
      overload function f(value: int) {}
      f(items: []);
    )")
              .valid);
  REQUIRE(has_code(check_source(R"(
      function f(_ a: int, _ b: double) {}
      overload function f(_ a: double, _ b: int) {}
      f(1, 2);
    )"),
                   "ARGOS-S0621"));
}

TEST_CASE("nested member receivers do not hide ambiguous function references",
          "[oop][overload]") {
  REQUIRE(has_code(check_source(R"(
      function f(_ n: int) -> int { return n; }
      overload function f(_ b: bool) -> int { return 1; }
      class A { public function value() -> int { return 1; } }
      function create(_ callback: (int) -> int) -> A { return new A(); }
      var n = create(f).value();
    )"),
                   "ARGOS-S0621"));
}

TEST_CASE("public declarations and private methods are the default",
          "[oop][access]") {
  REQUIRE(has_code(
      check_source("class A { function f() {} } var a = new A(); a.f();"),
      "ARGOS-S0611"));
  REQUIRE(
      check_source(
          "class A { function hidden() -> int { return 1; } public function "
          "f() -> int { return hidden(); } } var a = new A(); a.f();")
          .valid);
  REQUIRE(check_source("interface I { private function f(); } class A by I { "
                       "function f() {} }")
              .valid);
  REQUIRE(check_source("interface I { protected function f(); } class A by I { "
                       "protected function f() {} }")
              .valid);
}

TEST_CASE("sealed classes admit only explicitly permited descendants",
          "[oop][sealed]") {
  REQUIRE(check_source("sealed class Pessoa permited Funcionario, Aluno {} "
                       "class Funcionario: Pessoa {} class Aluno: Pessoa {}")
              .valid);
  REQUIRE(has_code(
      check_source("sealed class A permited B {} class B: A {} class C: A {}"),
      "ARGOS-S0624"));
  REQUIRE(has_code(check_source("sealed class A permited Missing {}"),
                   "ARGOS-S0624"));
  REQUIRE(
      has_code(check_source("sealed class A permited B, B {} class B: A {}"),
               "ARGOS-S0624"));
  REQUIRE(has_code(check_source("class A permited B {} class B {}"),
                   "ARGOS-S0624"));
  REQUIRE(
      has_code(check_source("sealed class A {} class B: A {}"), "ARGOS-S0624"));
  REQUIRE(check_source("sealed interface I permited A { function f(); } class "
                       "A by I { public function f() {} }")
              .valid);
}

TEST_CASE("by implements contracts and colon inherits the single superclass",
          "[oop][relations]") {
  REQUIRE(check_source("class B {} interface I {} class A: B by I {}").valid);
  REQUIRE(
      has_code(check_source("interface I {} class A: I {}"), "ARGOS-S0625"));
  REQUIRE(has_code(check_source("class B {} class A by B {}"), "ARGOS-S0625"));
  REQUIRE(has_code(check_source("interface I {} interface J by I {}"),
                   "ARGOS-S0625"));
  REQUIRE(
      check_source("interface I {} interface J: I {} class A by J {}").valid);
}

TEST_CASE(
    "data fields are sealed to their owner and explicitly permited entities",
    "[oop][data][access]") {
  REQUIRE(check_source(R"(
    data Person(name: string) permited Funcionario, Aluno;
    class Funcionario { public function name(_ p: Person) -> string { return p.name; } }
    class Aluno { public function name(_ p: Person) -> string { return p.name; } }
  )")
              .valid);
  REQUIRE(has_code(check_source("data Person(name: string); var p = new "
                                "Person(name: \"Ada\"); var n = p.name;"),
                   "ARGOS-S0624"));
  REQUIRE(has_code(
      check_source("data Person(name: string) permited A; class A {} class B { "
                   "function f(_ p: Person) -> string { return p.name; } }"),
      "ARGOS-S0624"));
  REQUIRE(has_code(
      check_source("data Person(name: string) permited A; class A { function "
                   "f(_ p: Person) { p.name = \"changed\"; } }"),
      "ARGOS-S0330"));
}

TEST_CASE("with delegates public overloads without introducing subtyping",
          "[oop][with]") {
  REQUIRE(check_source(R"(
    class B { public function f(_ n: int) -> int { return n; } public overload function f(_ s: string) -> int { return 2; } }
    class C { public function g() {} }
    class A with B, C { public function h() -> int { C.g(); return B.f(1); } }
    var a = new A(); a.f(1); a.f("x"); a.g(); a.h();
  )")
              .valid);
  REQUIRE(
      has_code(check_source("class B {} class A with B {} var b: B = new A();"),
               "ARGOS-S0101"));
  REQUIRE(has_code(check_source("class B { private function f() {} } class A "
                                "with B {} var a = new A(); a.f();"),
                   "ARGOS-S0353"));
  REQUIRE(has_code(check_source("class B { protected function f() {} } class A "
                                "with B {} var a = new A(); a.f();"),
                   "ARGOS-S0353"));
  REQUIRE(
      has_code(check_source("class B {} class A with B, B {}"), "ARGOS-S0626"));
  REQUIRE(has_code(check_source("class A with B {} class B with A {}"),
                   "ARGOS-S0626"));
  REQUIRE(has_code(check_source("abstract class B {} class A with B {}"),
                   "ARGOS-S0626"));
  REQUIRE(has_code(check_source("interface B {} class A with B {}"),
                   "ARGOS-S0626"));
}

TEST_CASE("delegation conflicts require an explicit same-signature method",
          "[oop][with]") {
  REQUIRE(
      has_code(check_source("class B { public function f() {} } class C { "
                            "public function f() {} } class A with B, C {}"),
               "ARGOS-S0627"));
  REQUIRE(check_source("class B { public function f() {} } class C { public "
                       "function f() {} } class A with B, C { public function "
                       "f() { B.f(); C.f(); } } var a = new A(); a.f();")
              .valid);
  REQUIRE(check_source("class B { public function f(_ n: int) {} } class C { "
                       "public function f(_ s: string) {} } class A with B, C "
                       "{} var a = new A(); a.f(1); a.f(\"x\");")
              .valid);
  REQUIRE(
      has_code(check_source("class B { public function f() {} } class Base { "
                            "public function f() {} } class A: Base with B {}"),
               "ARGOS-S0627"));
  REQUIRE(has_code(
      check_source(
          "abstract class Base { public abstract function f(); } class B { "
          "public function f() {} } class A: Base with B {}"),
      "ARGOS-S0609"));
}

TEST_CASE("exclusive components use existing constructor assignment syntax",
          "[oop][with][ownership]") {
  REQUIRE(
      check_source(
          "class B { public construct(_ n: int) {} public function f() {} } "
          "class A with B { public construct() { B = new B(1); B.f(); f(); } }")
          .valid);
  REQUIRE(has_code(
      check_source(
          "class B { public construct(_ n: int) {} } class A with B {}"),
      "ARGOS-S0628"));
  REQUIRE(has_code(check_source("class B { public construct(_ n: int) {} } "
                                "class A with B { construct() {} }"),
                   "ARGOS-S0614"));
  REQUIRE(has_code(
      check_source(
          "class B { public construct(_ n: int) {} public function f() {} } "
          "class A with B { construct() { B.f(); B = new B(1); } }"),
      "ARGOS-S0628"));
  REQUIRE(
      check_source("class B {} class A with B { construct() { B = new B(); } }")
          .valid);
  REQUIRE(has_code(check_source("class B {} class A with B { construct() { B = "
                                "new B(); B = new B(); } }"),
                   "ARGOS-S0330"));
  REQUIRE(
      has_code(check_source("class B { public construct(_ n: int) {} } class A "
                            "with B { construct(_ b: B) { B = b; } }"),
               "ARGOS-S0628"));
  REQUIRE(has_code(
      check_source(
          "class B {} class A with B { function escape() -> B { return B; } }"),
      "ARGOS-S0631"));
  REQUIRE_FALSE(check_source("class B {} class A with &B {}").valid);
  REQUIRE_FALSE(check_source("class B {} class A with shared B {}").valid);
}

TEST_CASE("private and protected top-level classes are rejected",
          "[oop][visibility]") {
  REQUIRE(has_code(check_source("private class A {}"), "ARGOS-S0629"));
  REQUIRE(has_code(check_source("protected class A {}"), "ARGOS-S0629"));
  REQUIRE(
      check_source("class A { private class B {} protected class C {} public "
                   "function f() { var b = new B(); var c = new C(); } }")
          .valid);
  REQUIRE(has_code(
      check_source("class A { private class B {} } var b = new A.B();"),
      "ARGOS-S0630"));
  REQUIRE(check_source("class A { protected class B {} } class D: A { function "
                       "f() { var b = new B(); } }")
              .valid);
  REQUIRE(has_code(check_source("class A { private class B { public class C {} "
                                "} } var c = new A.B.C();"),
                   "ARGOS-S0630"));
  REQUIRE_FALSE(check_source("public default class A {}").valid);
}

TEST_CASE("sealed permissions must name direct existing subclasses",
          "[oop][sealed]") {
  REQUIRE(has_code(check_source("sealed class A {}"), "ARGOS-S0624"));
  REQUIRE(has_code(check_source("sealed class A permited B {} class B {}"),
                   "ARGOS-S0624"));
  REQUIRE(check_source(
              "abstract sealed class A permited B { public abstract function "
              "f(); } class B: A { public override function f() {} }")
              .valid);
}

TEST_CASE("body data components generate canonical constructor hash and text",
          "[oop][data]") {
  REQUIRE(
      check_source("data Person permited Reader { name: string; age: uint_8; } "
                   "class Reader { public function age(_ p: Person) -> uint_8 "
                   "{ return p.age; } } var p = new Person(name: \"Ada\", age: "
                   "255); var hash = p.hashCode(); var text = p.toString();")
          .valid);
  REQUIRE_FALSE(
      check_source("data A { age: uint_8; } var a = new A(age: 256);").valid);
  REQUIRE(has_code(check_source("data A { name: string; } var a = new A(name: "
                                "\"x\"); var n = a.name;"),
                   "ARGOS-S0624"));
  REQUIRE(has_code(
      check_source("data A { function hashCode() -> int { return 0; } }"),
      "ARGOS-S0632"));
}

TEST_CASE("default access follows the declared package across source units",
          "[oop][package]") {
  REQUIRE(check_units({"package auth; default class A {}",
                       "package auth; function f() { var a = new A(); }"})
              .valid);
  REQUIRE(has_code(
      check_units({"package auth; default class A {}",
                   "package client; function f() { var a = new auth.A(); }"}),
      "ARGOS-S0630"));
  REQUIRE(has_code(check_units({"package auth; default class A {}",
                                "package client; class B: auth.A {}"}),
                   "ARGOS-S0630"));
  REQUIRE(
      check_units({"package auth; class A {}",
                   "package client; function f() { var a = new auth.A(); }"})
          .valid);
  REQUIRE(has_code(
      check_units({"package auth; default class A { public class B {} }",
                   "package client; function f() { var b = new auth.A.B(); }"}),
      "ARGOS-S0630"));
  REQUIRE(check_units({"package auth; function f() -> int { return 1; }",
                       "package client; function f() -> int { return 2; } "
                       "function main() -> int { return f() + auth.f(); }"})
              .valid);
}

TEST_CASE("data permissions remain lexical and do not transfer through APIs",
          "[oop][data][package]") {
  REQUIRE(check_units({"package model; data Person permited api.Controller { "
                       "name: string; }",
                       "package api; class Controller { public function read(_ "
                       "p: model.Person) -> string { return p.name; } }"})
              .valid);
  REQUIRE(has_code(
      check_units(
          {"package model; data Person permited api.Controller { name: string; "
           "}",
           "package api; class Controller {} class Serializer { function "
           "read(_ p: model.Person) -> string { return p.name; } }"}),
      "ARGOS-S0624"));
  REQUIRE(
      has_code(check_source("data A(x: int) permited B; class B {} class C: B "
                            "{ function read(_ a: A) -> int { return a.x; } }"),
               "ARGOS-S0624"));
  REQUIRE(check_source("data A(x: int); function transport(_ a: A) -> A { "
                       "return a; } var a = transport(new A(x: 1));")
              .valid);
}

TEST_CASE("qualified delegated components preserve package type identity",
          "[oop][with][package]") {
  REQUIRE(
      check_units(
          {"package one; class Engine { public function f() -> int { return 1; "
           "} }",
           "package two; class Engine { public function g() -> int { return 2; "
           "} }",
           "package app; class Car with one.Engine, two.Engine { public "
           "function both() -> int { return one.Engine.f() + two.Engine.g(); } "
           "} var c = new Car(); c.f(); c.g(); c.both();"})
          .valid);
  REQUIRE_FALSE(check_source("class B {} class A with B { construct() { var "
                             "leaked = this.`$with.A.B`; } }")
                    .valid);
}

TEST_CASE(
    "composition can bypass an inaccessible default constructor explicitly",
    "[oop][with]") {
  REQUIRE(check_source("class Engine { private construct() {} public overload "
                       "construct(_ n: int) {} public function run() {} } "
                       "class Car with Engine { public construct() { Engine = "
                       "new Engine(4); } } var c = new Car(); c.run();")
              .valid);
  REQUIRE(has_code(
      check_source(
          "class Engine { protected construct() {} } class Car with Engine {}"),
      "ARGOS-S0628"));
}

TEST_CASE("generated data helpers implement matching interface contracts", "[oop][data]") {
  REQUIRE(check_source("interface Value { function hashCode() -> int; function toString() -> string; } data Item(value: int) by Value; var item: Value = new Item(value: 3); item.hashCode(); item.toString();").valid);
  REQUIRE(has_code(check_source("interface Value { function hashCode() -> string; } data Item(value: int) by Value;"), "ARGOS-S0606"));
}
