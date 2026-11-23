#pragma once
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace argos::parser::lr2 {
using Symbol = int;
inline constexpr Symbol eof = -1;
using Lookahead = std::pair<Symbol, Symbol>;
struct Production {
  Symbol lhs;
  std::vector<Symbol> rhs;
};
struct Node {
  Symbol symbol{};
  std::optional<std::size_t> production;
  std::size_t token{};
  std::vector<std::unique_ptr<Node>> children;
};
struct Result {
  std::unique_ptr<Node> root;
  std::size_t error_position{};
  std::vector<Lookahead> expected;
};

// Canonical LR(2): states contain production/dot/two-terminal-lookahead
// items. No LR(0) core merging, conflict resolution or backtracking.
class Table final {
public:
  Table(Symbol start, std::vector<Production> productions);
  [[nodiscard]] Result parse(const std::vector<Symbol> &tokens) const;
  [[nodiscard]] std::size_t state_count() const { return actions_.size(); }

private:
  struct Action {
    enum class Kind { shift, reduce, accept } kind;
    std::size_t target;
    bool operator==(const Action &) const = default;
  };
  std::vector<Production> productions_;
  std::vector<std::map<Lookahead, Action>> actions_;
  std::vector<std::map<Symbol, std::size_t>> gotos_;
};
} // namespace argos::parser::lr2
