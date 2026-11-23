#include "argos/parser/lr2.hpp"
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
using namespace argos::parser::lr2;

TEST_CASE(
    "canonical LR2 distinguishes reductions that need two lookahead tokens",
    "[parser][lr2]") {
  // Not LR(1): after c, the first lookahead a cannot select A or B.
  Table table{100,
              {{100, {101, 1, 1}}, {100, {102, 1, 2}}, {101, {3}}, {102, {3}}}};
  auto aa = table.parse({3, 1, 1});
  auto ab = table.parse({3, 1, 2});
  REQUIRE(aa.root);
  REQUIRE(ab.root);
  REQUIRE(aa.root->production == 0);
  REQUIRE(ab.root->production == 1);
  REQUIRE_FALSE(table.parse({3, 2, 1}).root);
  REQUIRE_FALSE(table.parse({3, 1}).root);
  REQUIRE_FALSE(table.parse({3, 1, 1, 1}).root);
}

TEST_CASE("LR2 supports nullable productions and left recursion",
          "[parser][lr2]") {
  Table table{100, {{100, {100, 1}}, {100, {}}}};
  REQUIRE(table.parse({}).root);
  REQUIRE(table.parse({1, 1, 1}).root);
  auto invalid = table.parse({1, 2});
  REQUIRE_FALSE(invalid.root);
  REQUIRE_FALSE(invalid.expected.empty());
}

TEST_CASE(
    "LR2 rejects ambiguous grammars instead of resolving conflicts silently",
    "[parser][lr2]") {
  REQUIRE_THROWS_AS((Table{100, {{100, {100, 100}}, {100, {1}}}}),
                    std::invalid_argument);
}
