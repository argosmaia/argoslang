#include "argos/ir/optimizer.hpp"
#include "argos/ir/instruction_traits.hpp"
#include "argos/ir/validator.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace argos::ir {
void canonicalize_control_flow(Module &module) {
  for (auto &f : module.functions) {
    LabelId next = 0;
    for (const auto &b : f.blocks)
      next = std::max(next, b.label + 1);
    for (std::size_t n = 0; n < f.blocks.size(); ++n) {
      auto &instructions = f.blocks[n].instructions;
      for (std::size_t p = 0; p < instructions.size(); ++p) {
        if (instructions[p].opcode != Opcode::jmp_if_false)
          continue;
        if (p + 2 == instructions.size() &&
            instructions.back().opcode == Opcode::jmp)
          break;
        BasicBlock continuation{
            next++,
            {instructions.begin() + static_cast<std::ptrdiff_t>(p + 1),
             instructions.end()}};
        instructions.resize(p + 1);
        Instruction jump;
        jump.opcode = Opcode::jmp;
        jump.label = continuation.label;
        instructions.push_back(jump);
        f.blocks.push_back(std::move(continuation));
        break;
      }
    }
  }
}

void remove_unreachable_blocks(Module &module) {
  for (auto &f : module.functions) {
    if (f.blocks.empty())
      continue;
    std::unordered_map<LabelId, std::size_t> indices;
    for (std::size_t n = 0; n < f.blocks.size(); ++n)
      indices.emplace(f.blocks[n].label, n);
    std::unordered_set<LabelId> reachable;
    std::vector<LabelId> pending{f.blocks.front().label};
    while (!pending.empty()) {
      const auto label = pending.back();
      pending.pop_back();
      if (!reachable.insert(label).second || !indices.contains(label))
        continue;
      for (const auto &i : f.blocks[indices.at(label)].instructions) {
        if (i.opcode == Opcode::jmp || i.opcode == Opcode::jmp_if_false)
          pending.push_back(i.label);
        if (i.opcode == Opcode::jmp || i.opcode == Opcode::ret)
          break;
      }
    }
    std::erase_if(f.blocks, [&](const BasicBlock &b) {
      return !reachable.contains(b.label);
    });
  }
}

namespace {
bool removable(Opcode op) {
  switch (op) {
  case Opcode::nop:
  case Opcode::load_const:
  case Opcode::load_string:
  case Opcode::load_var:
  case Opcode::int_to_double:
  case Opcode::round_float:
  case Opcode::logical_not:
  case Opcode::add:
  case Opcode::sub:
  case Opcode::mul:
  case Opcode::neg:
  case Opcode::cmp_eq:
  case Opcode::cmp_ne:
  case Opcode::cmp_lt:
  case Opcode::cmp_le:
  case Opcode::cmp_gt:
  case Opcode::cmp_ge:
    return true;
  default:
    return false;
  }
}
std::optional<std::int32_t> integer_constant(const Module &m,
                                             const Instruction &i) {
  if (i.opcode != Opcode::load_const)
    return std::nullopt;
  const auto &text = m.constants[i.constant].value;
  std::int32_t value{};
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size())
    return std::nullopt;
  return value;
}
} // namespace

void optimize_module(Module &module) {
  if (auto error = validate_module(module))
    throw std::runtime_error("IR inválida antes da otimização: " + *error);
  for (auto &f : module.functions) {
    for (auto &b : f.blocks) {
      std::unordered_map<Register, std::int32_t> known;
      for (std::size_t p = 0; p < b.instructions.size(); ++p) {
        auto &i = b.instructions[p];
        std::optional<std::int32_t> folded;
        if (i.opcode == Opcode::load_const &&
            f.register_types[i.dst] != ValueType::float64)
          folded = integer_constant(module, i);
        else if (known.contains(i.src1)) {
          const auto a = known.at(i.src1);
          if (i.opcode == Opcode::logical_not)
            folded = a == 0;
          if (i.opcode == Opcode::neg &&
              f.register_types[i.dst] == ValueType::int32)
            folded =
                std::bit_cast<std::int32_t>(0u - static_cast<std::uint32_t>(a));
          if (known.contains(i.src2)) {
            const auto c = known.at(i.src2);
            const auto x = static_cast<std::uint32_t>(a),
                       y = static_cast<std::uint32_t>(c);
            switch (i.opcode) {
            case Opcode::add:
              folded = std::bit_cast<std::int32_t>(x + y);
              break;
            case Opcode::sub:
              folded = std::bit_cast<std::int32_t>(x - y);
              break;
            case Opcode::mul:
              folded = std::bit_cast<std::int32_t>(x * y);
              break;
            case Opcode::cmp_eq:
              folded = a == c;
              break;
            case Opcode::cmp_ne:
              folded = a != c;
              break;
            case Opcode::cmp_lt:
              folded = a < c;
              break;
            case Opcode::cmp_le:
              folded = a <= c;
              break;
            case Opcode::cmp_gt:
              folded = a > c;
              break;
            case Opcode::cmp_ge:
              folded = a >= c;
              break;
            default:
              break;
            }
          }
          if (i.opcode == Opcode::jmp_if_false) {
            if (a == 0) {
              i.opcode = Opcode::jmp;
              b.instructions.resize(p + 1);
            } else {
              b.instructions.erase(b.instructions.begin() +
                                   static_cast<std::ptrdiff_t>(p));
              --p;
            }
            continue;
          }
        }
        if (writes_register(i.opcode)) {
          known.erase(i.dst);
          if (folded) {
            known.emplace(i.dst, *folded);
            i.opcode = Opcode::load_const;
            i.constant = static_cast<ConstantId>(module.constants.size());
            module.constants.push_back({i.constant, std::to_string(*folded)});
          }
        }
      }
    }
  }
  remove_unreachable_blocks(module);
  // Registers are mutable: only erase a definition if the register has no uses
  // anywhere. This is deliberately more conservative than SSA dead-code
  // removal.
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto &f : module.functions) {
      std::unordered_set<Register> used;
      for (const auto &b : f.blocks)
        for (const auto &i : b.instructions)
          visit_register_reads(i, [&](Register r) { used.insert(r); });
      for (auto &b : f.blocks) {
        const auto count =
            std::erase_if(b.instructions, [&](const Instruction &i) {
              return i.opcode == Opcode::nop ||
                     (writes_register(i.opcode) && !used.contains(i.dst) &&
                      removable(i.opcode));
            });
        changed = changed || count != 0;
      }
    }
  }
  if (auto error = validate_module(module))
    throw std::runtime_error("IR inválida após otimização: " + *error);
}
} // namespace argos::ir
