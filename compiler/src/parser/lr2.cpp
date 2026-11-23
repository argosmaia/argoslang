#include "argos/parser/lr2.hpp"
#include <algorithm>
#include <compare>
#include <deque>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

namespace argos::parser::lr2 {
namespace {
using Word = std::vector<Symbol>;
using Words = std::set<Word>;
struct Item {
  std::size_t production, dot;
  Lookahead lookahead;
  auto operator<=>(const Item &) const = default;
};
using State = std::set<Item>;
Words product(const Words &left, const Words &right) {
  Words result;
  for (const auto &a : left) {
    if (a.size() == 2) {
      result.insert(a);
      continue;
    }
    for (const auto &b : right) {
      auto word = a;
      for (const auto symbol : b)
        if (word.size() < 2)
          word.push_back(symbol);
      result.insert(std::move(word));
    }
  }
  return result;
}
Words first_word(const Word &word, const std::map<Symbol, Words> &first) {
  Words result{{}};
  for (const auto symbol : word)
    result = product(result, first.at(symbol));
  return result;
}
} // namespace

Table::Table(Symbol start, std::vector<Production> productions)
    : productions_(std::move(productions)) {
  if (productions_.empty())
    throw std::invalid_argument("gramática LR(2) vazia");
  std::set<Symbol> nonterminals;
  Symbol augmented = start;
  for (const auto &p : productions_) {
    if (p.lhs == eof)
      throw std::invalid_argument("EOF não pode ser não terminal");
    nonterminals.insert(p.lhs);
    augmented = std::max(augmented, p.lhs);
    for (const auto s : p.rhs) {
      if (s == eof)
        throw std::invalid_argument("EOF é reservado ao lookahead LR(2)");
      augmented = std::max(augmented, s);
    }
  }
  if (!nonterminals.contains(start))
    throw std::invalid_argument("símbolo inicial sem produção");
  if (augmented == std::numeric_limits<Symbol>::max())
    throw std::invalid_argument("símbolo de gramática fora do intervalo");
  ++augmented;
  const auto augmented_rule = productions_.size();
  productions_.push_back({augmented, {start}});
  nonterminals.insert(augmented);
  std::map<Symbol, std::vector<std::size_t>> rules;
  std::map<Symbol, Words> first;
  first[eof] = {{eof}};
  for (std::size_t n = 0; n < productions_.size(); ++n) {
    const auto &p = productions_[n];
    rules[p.lhs].push_back(n);
    first.try_emplace(p.lhs);
    for (const auto s : p.rhs) {
      first.try_emplace(s);
      if (!nonterminals.contains(s))
        first[s] = {{s}};
    }
  }
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &p : productions_) {
      auto words = first_word(p.rhs, first);
      for (auto &word : words)
        changed = first[p.lhs].insert(word).second || changed;
    }
  }
  const auto closure = [&](State state) {
    std::deque<Item> pending(state.begin(), state.end());
    while (!pending.empty()) {
      const auto item = pending.front();
      pending.pop_front();
      const auto &p = productions_[item.production];
      if (item.dot == p.rhs.size() || !nonterminals.contains(p.rhs[item.dot]))
        continue;
      Word suffix(p.rhs.begin() + static_cast<std::ptrdiff_t>(item.dot + 1),
                  p.rhs.end());
      suffix.push_back(item.lookahead.first);
      suffix.push_back(item.lookahead.second);
      for (const auto &word : first_word(suffix, first)) {
        if (word.size() != 2)
          throw std::logic_error("FIRST2 incompleto");
        for (const auto rule : rules.at(p.rhs[item.dot])) {
          Item next{rule, 0, {word[0], word[1]}};
          if (state.insert(next).second)
            pending.push_back(next);
        }
      }
    }
    return state;
  };
  std::vector<State> states{closure(State{{augmented_rule, 0, {eof, eof}}})};
  std::map<State, std::size_t> indices{{states.front(), 0}};
  std::vector<std::map<Symbol, std::size_t>> transitions;
  for (std::size_t n = 0; n < states.size(); ++n) {
    std::map<Symbol, State> kernels;
    for (const auto &item : states[n]) {
      const auto &p = productions_[item.production];
      if (item.dot < p.rhs.size())
        kernels[p.rhs[item.dot]].insert(
            {item.production, item.dot + 1, item.lookahead});
    }
    std::map<Symbol, std::size_t> next;
    for (auto &[symbol, kernel] : kernels) {
      auto target = closure(std::move(kernel));
      const auto [found, added] = indices.emplace(target, states.size());
      if (added)
        states.push_back(std::move(target));
      next.emplace(symbol, found->second);
    }
    transitions.push_back(std::move(next));
  }
  actions_.resize(states.size());
  gotos_.resize(states.size());
  for (std::size_t n = 0; n < states.size(); ++n) {
    const auto action = [&](Lookahead look, Action value) {
      const auto [found, added] = actions_[n].emplace(look, value);
      if (!added && found->second != value)
        throw std::invalid_argument("conflito LR(2) no estado " +
                                    std::to_string(n) + " com lookahead " +
                                    std::to_string(look.first) + "," +
                                    std::to_string(look.second));
    };
    for (const auto &[s, target] : transitions[n])
      if (nonterminals.contains(s))
        gotos_[n][s] = target;
    for (const auto &item : states[n]) {
      const auto &p = productions_[item.production];
      if (item.dot == p.rhs.size()) {
        action(item.lookahead,
               {item.production == augmented_rule ? Action::Kind::accept
                                                  : Action::Kind::reduce,
                item.production});
      } else if (!nonterminals.contains(p.rhs[item.dot])) {
        Word suffix(p.rhs.begin() + static_cast<std::ptrdiff_t>(item.dot),
                    p.rhs.end());
        suffix.push_back(item.lookahead.first);
        suffix.push_back(item.lookahead.second);
        for (const auto &word : first_word(suffix, first))
          action({word[0], word[1]},
                 {Action::Kind::shift, transitions[n].at(p.rhs[item.dot])});
      }
    }
  }
}

Result Table::parse(const std::vector<Symbol> &tokens) const {
  if (const auto end = std::find(tokens.begin(), tokens.end(), eof);
      end != tokens.end()) {
    Result result;
    result.error_position = static_cast<std::size_t>(end - tokens.begin());
    return result;
  }
  std::vector<std::size_t> states{0};
  std::vector<std::unique_ptr<Node>> nodes;
  std::size_t position = 0;
  const auto at = [&](std::size_t p) {
    return p < tokens.size() ? tokens[p] : eof;
  };
  for (;;) {
    const Lookahead look{at(position), at(position + 1)};
    const auto &row = actions_[states.back()];
    const auto found = row.find(look);
    if (found == row.end()) {
      Result result;
      result.error_position = position;
      for (const auto &[expected, value] : row) {
        (void)value;
        result.expected.push_back(expected);
      }
      return result;
    }
    const auto a = found->second;
    if (a.kind == Action::Kind::accept)
      return {std::move(nodes.back()), position, {}};
    if (a.kind == Action::Kind::shift) {
      if (position == tokens.size())
        throw std::logic_error("gramática tentou consumir EOF");
      auto node = std::make_unique<Node>();
      node->symbol = at(position);
      node->token = position++;
      nodes.push_back(std::move(node));
      states.push_back(a.target);
    } else {
      const auto &p = productions_[a.target];
      auto node = std::make_unique<Node>();
      node->symbol = p.lhs;
      node->production = a.target;
      const auto start = nodes.size() - p.rhs.size();
      for (std::size_t n = start; n < nodes.size(); ++n)
        node->children.push_back(std::move(nodes[n]));
      nodes.resize(start);
      states.resize(states.size() - p.rhs.size());
      states.push_back(gotos_[states.back()].at(p.lhs));
      nodes.push_back(std::move(node));
    }
  }
}
} // namespace argos::parser::lr2
