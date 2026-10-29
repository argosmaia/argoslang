#include "argos/diagnostics/diagnostic_catalog.hpp"
#include "argos/diagnostics/diagnostic_engine.hpp"
#include "argos/diagnostics/diagnostic_printer.hpp"
#include "argos/source/source_file.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <sstream>

TEST_CASE("catalog diagnostics retain title, explanation, and help") {
  argos::diagnostics::DiagnosticEngine diagnostics;
  argos::source::SourceFile source{
      .path = std::filesystem::path{"test.argos"},
      .content = "var value: int;"};
  const auto *definition =
      argos::diagnostics::catalog::semantic::find_definition("ARGOS-S0301");
  REQUIRE(definition != nullptr);
  diagnostics.emit(*definition, {}, "variável 'value' usada antes da inicialização");

  std::ostringstream output;
  argos::diagnostics::DiagnosticPrinter{}.print(source, diagnostics, output);
  REQUIRE(output.str().find("variável usada antes da inicialização") !=
          std::string::npos);
  REQUIRE(output.str().find("falando simples:") != std::string::npos);
  REQUIRE(output.str().find("tente assim:") != std::string::npos);
}

TEST_CASE("object safety diagnostics provide actionable explanations",
          "[diagnostics][oop]") {
  for (const auto code :
       {"ARGOS-S0620", "ARGOS-S0621", "ARGOS-S0622", "ARGOS-S0623",
        "ARGOS-S0624", "ARGOS-S0625", "ARGOS-S0626", "ARGOS-S0627",
        "ARGOS-S0628", "ARGOS-S0629", "ARGOS-S0630", "ARGOS-S0631",
        "ARGOS-S0632"}) {
    const auto *definition =
        argos::diagnostics::catalog::semantic::find_definition(code);
    REQUIRE(definition != nullptr);
    REQUIRE_FALSE(definition->title.empty());
    REQUIRE_FALSE(definition->explanation.empty());
    REQUIRE_FALSE(definition->help.empty());
  }
}
