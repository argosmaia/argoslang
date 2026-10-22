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
