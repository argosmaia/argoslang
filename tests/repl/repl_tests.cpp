#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/repl/repl_session.hpp"
#include "argos/source/source_file.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <sstream>
#include <string>

namespace {

bool execute(argos::repl::ReplSession &session, const std::string &input,
             std::ostringstream &output,
             argos::diagnostics::DiagnosticEngine &diagnostics) {
  argos::source::SourceFile source{
      .path = std::filesystem::path{"<repl-test>"},
      .content = {}};
  return session.execute(input, output, source, diagnostics);
}

} // namespace

TEST_CASE("REPL shares VM execution and keeps variable state") {
  argos::repl::ReplSession session;
  session.reset();
  std::ostringstream output;
  argos::diagnostics::DiagnosticEngine diagnostics;

  REQUIRE(execute(session, "var mut count: int = 0", output, diagnostics));
  REQUIRE(output.str().empty());
  REQUIRE(execute(session, "count = count + 1", output, diagnostics));
  REQUIRE(output.str() == "1\n");
  output.str({});
  output.clear();

  REQUIRE(execute(session, "while count < 5 { count = count + 1; if count == 3 break; }",
                  output, diagnostics));
  REQUIRE(output.str().empty());
  REQUIRE(execute(session, "count", output, diagnostics));
  REQUIRE(output.str() == "3\n");

  session.reset();
}

TEST_CASE("REPL runs stored functions and prints booleans by semantic type") {
  argos::repl::ReplSession session;
  session.reset();
  std::ostringstream output;
  argos::diagnostics::DiagnosticEngine diagnostics;

  REQUIRE(execute(session,
                  "function twice(_ value: int) -> int { return value * 2; }",
                  output, diagnostics));
  REQUIRE(execute(session, "twice(21)", output, diagnostics));
  REQUIRE(output.str() == "42\n");
  output.str({});
  output.clear();

  REQUIRE(execute(session, "var flag: bool = true", output, diagnostics));
  REQUIRE(execute(session, "print(flag)", output, diagnostics));
  REQUIRE(output.str() == "true\n");
  session.reset();
}

TEST_CASE("REPL prints string literals through the shared VM") {
  argos::repl::ReplSession session;
  session.reset();
  std::ostringstream output;
  argos::diagnostics::DiagnosticEngine diagnostics;

  REQUIRE(execute(session, "print(\"hello\")", output, diagnostics));
  REQUIRE(output.str() == "hello\n");
  output.str({});
  output.clear();
  REQUIRE_FALSE(execute(session, "var greeting: string = \"hello\"", output,
                        diagnostics));
  REQUIRE(diagnostics.has_errors());
  session.reset();
}

TEST_CASE("REPL evaluates real division and persists double variables") {
  argos::repl::ReplSession session;
  session.reset();
  std::ostringstream output;
  argos::diagnostics::DiagnosticEngine diagnostics;

  REQUIRE(execute(session, "1/2", output, diagnostics));
  REQUIRE(output.str() == "0.5\n");
  output.str({});
  output.clear();

  REQUIRE(execute(session, "var average: double = 1.0 / 2.0", output,
                  diagnostics));
  REQUIRE(execute(session, "average", output, diagnostics));
  REQUIRE(output.str() == "0.5\n");
  session.reset();
}

TEST_CASE("REPL supports typed uninitialized float variables") {
  argos::repl::ReplSession session;
  session.reset();
  std::ostringstream output;
  argos::diagnostics::DiagnosticEngine diagnostics;

  REQUIRE(execute(session, "var a: int", output, diagnostics));
  REQUIRE(output.str().empty());
  REQUIRE(execute(session, "var b: float", output, diagnostics));
  REQUIRE(output.str().empty());
  REQUIRE(execute(session, "var mut c: float", output, diagnostics));
  REQUIRE(output.str().empty());
  REQUIRE(execute(session, "a = 1", output, diagnostics));
  output.str({});
  output.clear();
  REQUIRE(execute(session, "b = 2", output, diagnostics));
  REQUIRE(output.str() == "2\n");
  output.str({});
  output.clear();
  REQUIRE(execute(session, "c = a / b", output, diagnostics));
  REQUIRE(output.str() == "0.5\n");
  output.str({});
  output.clear();
  REQUIRE(execute(session, "print(c)", output, diagnostics));
  REQUIRE(output.str() == "0.5\n");
  session.reset();
}

TEST_CASE("REPL converts integers and doubles to float variables") {
  argos::repl::ReplSession session;
  session.reset();
  std::ostringstream output;
  argos::diagnostics::DiagnosticEngine diagnostics;

  REQUIRE(execute(session, "var value: float = 2", output, diagnostics));
  REQUIRE(output.str().empty());
  REQUIRE(execute(session, "var mut changed: float = 1", output,
                  diagnostics));
  REQUIRE(execute(session, "changed = 2.0", output, diagnostics));
  REQUIRE(output.str() == "2\n");
  output.str({});
  output.clear();
  REQUIRE(execute(session, "changed = value / 2", output, diagnostics));
  REQUIRE(output.str() == "1\n");
  output.str({});
  output.clear();
  REQUIRE(execute(session, "print(value)", output, diagnostics));
  REQUIRE(output.str() == "2\n");
  output.str({});
  output.clear();
  REQUIRE(execute(session, "print(changed)", output, diagnostics));
  REQUIRE(output.str() == "1\n");
  session.reset();
}

TEST_CASE("REPL rejects persistent closures without losing numeric state") {
  argos::repl::ReplSession session;
  session.reset();
  std::ostringstream output;
  argos::diagnostics::DiagnosticEngine first;
  REQUIRE(execute(session, "var n = 3", output, first));
  argos::diagnostics::DiagnosticEngine rejected;
  REQUIRE_FALSE(execute(session, "var f = (x: int) => x + 1", output, rejected));
  REQUIRE(rejected.has_errors());
  argos::diagnostics::DiagnosticEngine last;
  REQUIRE(execute(session, "n", output, last));
  REQUIRE(output.str() == "3\n");
  session.reset();
}

TEST_CASE("REPL persists optional layers and rolls back failed unwrap",
          "[optional]") {
  argos::repl::ReplSession session;
  session.reset();
  std::ostringstream output;
  argos::diagnostics::DiagnosticEngine diagnostics;
  REQUIRE(execute(session, "var mut x: int? = nil", output, diagnostics));
  REQUIRE(execute(session,
                  "x ?"
                  "?= 0",
                  output, diagnostics));
  REQUIRE(execute(session, "x!!", output, diagnostics));
  REQUIRE(output.str() == "0\n0\n");
  REQUIRE(execute(session, "var inner: int? = nil", output, diagnostics));
  REQUIRE(execute(session, "var outer: int?? = inner", output, diagnostics));
  REQUIRE(execute(session, "outer ?== nil", output, diagnostics));
  REQUIRE(execute(session, "outer!! ?== nil", output, diagnostics));
  REQUIRE(output.str() == "0\n0\nfalse\ntrue\n");
  argos::diagnostics::DiagnosticEngine failed;
  REQUIRE_FALSE(execute(session, "x = nil; x!!", output, failed));
  argos::diagnostics::DiagnosticEngine next;
  output.str({});
  REQUIRE(execute(session, "x!!", output, next));
  REQUIRE(output.str() == "0\n");
  session.reset();
}

TEST_CASE("REPL executes optional arrays and preserves optional scalar payloads", "[optional]") {
  argos::repl::ReplSession session;
  session.reset();
  std::ostringstream output;
  argos::diagnostics::DiagnosticEngine diagnostics;
  REQUIRE(execute(session, "function items() -> [int]? { return [3, 4]; }", output, diagnostics));
  REQUIRE(execute(session, "items()?[1]", output, diagnostics));
  REQUIRE(execute(session, "var flag: bool? = false", output, diagnostics));
  REQUIRE(execute(session, "var real: double? = 1.5", output, diagnostics));
  REQUIRE(execute(session, "flag!!", output, diagnostics));
  REQUIRE(execute(session, "real!!", output, diagnostics));
  REQUIRE(output.str() == "4\nfalse\n1.5\n");
  session.reset();
}
