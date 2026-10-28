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
        abstract class Base : Readable { public abstract function read() -> int; }
        final class Value : Base {
            public final override function read() -> int { return 42; }
        }
        var value: Readable = new Value();
        var n = value.read();
    )").valid);
    REQUIRE(has_code(check_source("interface I { function f() -> int; } class A : I {}"), "ARGOS-S0609"));
    REQUIRE(has_code(check_source("interface I { var x: int; }"), "ARGOS-S0607"));
    REQUIRE(has_code(check_source("abstract class A {} var a = new A();"), "ARGOS-S0602"));
    REQUIRE(has_code(check_source("interface I {} var i = new I();"), "ARGOS-S0602"));
    REQUIRE(has_code(check_source("final class A {} class B : A {}"), "ARGOS-S0605"));
    REQUIRE(has_code(check_source("class A : B {} class B : A {}"), "ARGOS-S0603"));
    REQUIRE(has_code(check_source("class A {} class B {} class C : A, B {}"), "ARGOS-S0604"));
    REQUIRE(has_code(check_source("class A { function f() {} } class B : A { override function f() {} }"), "ARGOS-S0608"));
    REQUIRE(has_code(check_source("class A { virtual function f() {} } class B : A { function f() {} }"), "ARGOS-S0608"));
    REQUIRE(has_code(check_source("class A { override function f() {} }"), "ARGOS-S0608"));
    REQUIRE(has_code(check_source("interface I { function f(); } class A { function f() {} } class B : A, I {}"), "ARGOS-S0606"));

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
    REQUIRE(has_code(check_source("interface I { private function f(); }"), "ARGOS-S0600"));
    REQUIRE(has_code(check_source("class A { private abstract function f(); }"), "ARGOS-S0600"));
    REQUIRE(has_code(check_source("class A { final virtual function f() {} }"), "ARGOS-S0600"));
    REQUIRE(has_code(check_source("class A { var x: int = 1; var y: int = this.x; }"), "ARGOS-S0614"));
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
      class Item : Named {
        private var mut stored: string = "";
        public var mut name: string {
          getter() { return stored; }
          setter() { stored = value; }
        }
      }
      var item: Named = new Item();
      item.name = "new";
    )").valid);
    REQUIRE(has_code(check_source("interface I { var x: int { getter(); } } class A : I {}"), "ARGOS-S0609"));
    REQUIRE(has_code(check_source("interface I { var mut x: int { getter(); setter(); } } class A : I { public var x: int { getter() { return 1; } } }"), "ARGOS-S0606"));
    REQUIRE(has_code(check_source("interface I { var x: int { getter() { return 1; } } }"), "ARGOS-S0607"));
    REQUIRE(has_code(check_source("interface R { var x: int { getter(); } } interface W { var mut x: int { getter(); setter(); } } class A : R, W { public var x: int { getter() { return 1; } } }"), "ARGOS-S0606"));
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
