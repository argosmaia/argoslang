#include "argos/vm/vm.hpp"
#include "argos/ir/validator.hpp"

#include <charconv>
#include <bit>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <iostream>

namespace argos::vm {

namespace {

[[noreturn]] void invalid(std::string_view detail) {
  throw std::runtime_error("IR inválida para VM: " + std::string{detail});
}

} // namespace

std::int32_t VM::run() const {
  if (module_ == nullptr)
    throw std::runtime_error("nenhum módulo foi carregado na VM");
  if (const auto error = argos::ir::validate_module(*module_))
    invalid(*error);
  for (const auto &function : module_->functions) {
    if (function.name == "main") {
      if (function.parameter_count != 0)
        invalid("main não pode receber parâmetros");
      return static_cast<std::int32_t>(execute(function.id, {}));
    }
  }
  throw std::runtime_error("módulo não contém uma função main");
}

std::uint64_t VM::execute(argos::ir::FunctionId function_id,
                          const std::vector<std::uint64_t> &arguments) const {
  if (module_ == nullptr || function_id >= module_->functions.size())
    invalid("função ausente");
  const auto &function = module_->functions[function_id];
  if (function.id != function_id)
    invalid("identificador de função inconsistente");
  if (function.parameter_count != arguments.size()) invalid("quantidade de argumentos inválida");

  std::unordered_map<argos::ir::LabelId, std::size_t> blocks;
  for (std::size_t index = 0; index < function.blocks.size(); ++index) {
    if (!blocks.emplace(function.blocks[index].label, index).second)
      invalid("rótulo duplicado");
  }
  if (function.blocks.empty())
    invalid("função sem blocos");

  std::vector<std::uint64_t> registers(function.register_count, 0);
  std::vector<std::uint64_t> slots(function.slot_count, 0);
  for (std::size_t index = 0; index < arguments.size(); ++index) slots[index] = arguments[index];
  const auto register_value = [&](argos::ir::Register reg) -> std::uint64_t {
    if (reg >= registers.size())
      invalid("registrador fora dos limites");
    return registers[reg];
  };
  const auto set_register = [&](argos::ir::Register reg, std::uint64_t value) {
    if (reg >= registers.size())
      invalid("registrador fora dos limites");
    registers[reg] = value;
  };
  const auto integer_value = [&](argos::ir::Register reg) {
    return static_cast<std::int32_t>(register_value(reg));
  };
  const auto double_value = [&](argos::ir::Register reg) {
    if (function.register_types[reg] == argos::ir::ValueType::float64)
      return std::bit_cast<double>(register_value(reg));
    return static_cast<double>(integer_value(reg));
  };

  std::size_t block_index = 0;
  std::size_t instruction_index = 0;
  while (true) {
    if (block_index >= function.blocks.size())
      invalid("bloco fora dos limites");
    const auto &block = function.blocks[block_index];
    if (instruction_index >= block.instructions.size()) {
      // Falling through is valid only when a following basic block exists.
      if (++block_index >= function.blocks.size())
        return 0;
      instruction_index = 0;
      continue;
    }
    const auto &instruction = block.instructions[instruction_index++];
    std::ostream &output = output_ ? *output_ : std::cout;
    const auto jump_to = [&](argos::ir::LabelId label) {
      const auto target = blocks.find(label);
      if (target == blocks.end())
        invalid("alvo de salto ausente");
      block_index = target->second;
      instruction_index = 0;
    };

    using argos::ir::Opcode;
    switch (instruction.opcode) {
    case Opcode::nop:
      break;
    case Opcode::load_const: {
      if (instruction.constant >= module_->constants.size())
        invalid("constante fora dos limites");
      const auto &text = module_->constants[instruction.constant].value;
      if (function.register_types[instruction.dst] ==
          argos::ir::ValueType::float64) {
        double value{};
        const auto [end, error] =
            std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size())
          invalid("constante double inválida");
        set_register(instruction.dst, std::bit_cast<std::uint64_t>(value));
      } else {
        std::int32_t value{};
        const auto [end, error] =
            std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size())
          invalid("constante Int inválida");
        set_register(instruction.dst, static_cast<std::uint64_t>(
                                          static_cast<std::int64_t>(value)));
      }
      break;
    }
    case Opcode::load_var:
      if (instruction.slot >= slots.size())
        invalid("slot fora dos limites");
      set_register(instruction.dst, slots[instruction.slot]);
      break;
    case Opcode::store_var:
      if (instruction.slot >= slots.size())
        invalid("slot fora dos limites");
      slots[instruction.slot] = register_value(instruction.src1);
      break;
    case Opcode::neg: {
      if (function.register_types[instruction.dst] ==
          argos::ir::ValueType::float64) {
        set_register(instruction.dst, std::bit_cast<std::uint64_t>(
                                          -double_value(instruction.src1)));
      } else {
        const auto value = integer_value(instruction.src1);
        if (value == std::numeric_limits<std::int32_t>::min())
          throw std::runtime_error("overflow aritmético na VM");
        set_register(instruction.dst, static_cast<std::uint64_t>(
                                          static_cast<std::int64_t>(-value)));
      }
      break;
    }
    case Opcode::int_to_double:
      set_register(instruction.dst, std::bit_cast<std::uint64_t>(
                                        static_cast<double>(integer_value(instruction.src1))));
      break;
    case Opcode::logical_not:
      set_register(instruction.dst,
                   register_value(instruction.src1) == 0 ? 1 : 0);
      break;
    case Opcode::add:
      if (function.register_types[instruction.dst] == argos::ir::ValueType::float64)
        set_register(instruction.dst, std::bit_cast<std::uint64_t>(
                                          double_value(instruction.src1) +
                                          double_value(instruction.src2)));
      else
        set_register(instruction.dst, static_cast<std::uint64_t>(
                                          static_cast<std::int64_t>(
                                              static_cast<std::int32_t>(
                                                  static_cast<std::uint32_t>(integer_value(instruction.src1)) +
                                                  static_cast<std::uint32_t>(integer_value(instruction.src2))))));
      break;
    case Opcode::sub:
      if (function.register_types[instruction.dst] == argos::ir::ValueType::float64)
        set_register(instruction.dst, std::bit_cast<std::uint64_t>(
                                          double_value(instruction.src1) -
                                          double_value(instruction.src2)));
      else
        set_register(instruction.dst, static_cast<std::uint64_t>(
                                          static_cast<std::int64_t>(
                                              static_cast<std::int32_t>(
                                                  static_cast<std::uint32_t>(integer_value(instruction.src1)) -
                                                  static_cast<std::uint32_t>(integer_value(instruction.src2))))));
      break;
    case Opcode::mul:
      if (function.register_types[instruction.dst] == argos::ir::ValueType::float64)
        set_register(instruction.dst, std::bit_cast<std::uint64_t>(
                                          double_value(instruction.src1) *
                                          double_value(instruction.src2)));
      else
        set_register(instruction.dst, static_cast<std::uint64_t>(
                                          static_cast<std::int64_t>(
                                              static_cast<std::int32_t>(
                                                  static_cast<std::uint32_t>(integer_value(instruction.src1)) *
                                                  static_cast<std::uint32_t>(integer_value(instruction.src2))))));
      break;
    case Opcode::div:
      set_register(instruction.dst, std::bit_cast<std::uint64_t>(
                                        double_value(instruction.src1) /
                                        double_value(instruction.src2)));
      break;
    case Opcode::mod: {
      const auto lhs = integer_value(instruction.src1);
      const auto rhs = integer_value(instruction.src2);
      if (rhs == 0 ||
          (lhs == std::numeric_limits<std::int32_t>::min() && rhs == -1))
        throw std::runtime_error(
            "módulo por zero ou resultado fora do intervalo\n  em " +
            function.name);
      set_register(instruction.dst, static_cast<std::uint64_t>(
                                        static_cast<std::int64_t>(lhs % rhs)));
      break;
    }
    case Opcode::cmp_eq:
      set_register(instruction.dst,
                   function.register_types[instruction.src1] ==
                           argos::ir::ValueType::float64
                       ? double_value(instruction.src1) ==
                             double_value(instruction.src2)
                       : register_value(instruction.src1) ==
                             register_value(instruction.src2));
      break;
    case Opcode::cmp_ne:
      set_register(instruction.dst,
                   function.register_types[instruction.src1] ==
                           argos::ir::ValueType::float64
                       ? double_value(instruction.src1) !=
                             double_value(instruction.src2)
                       : register_value(instruction.src1) !=
                             register_value(instruction.src2));
      break;
    case Opcode::cmp_lt:
      set_register(instruction.dst,
                   function.register_types[instruction.src1] ==
                           argos::ir::ValueType::float64
                       ? double_value(instruction.src1) < double_value(instruction.src2)
                       : integer_value(instruction.src1) < integer_value(instruction.src2));
      break;
    case Opcode::cmp_le:
      set_register(instruction.dst,
                   function.register_types[instruction.src1] ==
                           argos::ir::ValueType::float64
                       ? double_value(instruction.src1) <= double_value(instruction.src2)
                       : integer_value(instruction.src1) <= integer_value(instruction.src2));
      break;
    case Opcode::cmp_gt:
      set_register(instruction.dst,
                   function.register_types[instruction.src1] ==
                           argos::ir::ValueType::float64
                       ? double_value(instruction.src1) > double_value(instruction.src2)
                       : integer_value(instruction.src1) > integer_value(instruction.src2));
      break;
    case Opcode::cmp_ge:
      set_register(instruction.dst,
                   function.register_types[instruction.src1] ==
                           argos::ir::ValueType::float64
                       ? double_value(instruction.src1) >= double_value(instruction.src2)
                       : integer_value(instruction.src1) >= integer_value(instruction.src2));
      break;
    case Opcode::jmp:
      jump_to(instruction.label);
      break;
    case Opcode::jmp_if_false:
      if (register_value(instruction.src1) == 0)
        jump_to(instruction.label);
      break;
    case Opcode::call: {
      std::vector<std::uint64_t> call_arguments;
      for (const auto reg : instruction.arguments) call_arguments.push_back(register_value(reg));
      try {
        set_register(instruction.dst, execute(instruction.function, call_arguments));
      } catch (const std::exception &error) {
        throw std::runtime_error(std::string{error.what()} + "\n  chamado por " +
                                 function.name);
      }
      break;
    }
    case Opcode::print_int: output << integer_value(instruction.src1); break;
    case Opcode::print_bool: output << (register_value(instruction.src1) != 0 ? "true" : "false"); break;
    case Opcode::print_double:
      output << std::setprecision(17) << std::defaultfloat
             << std::bit_cast<double>(register_value(instruction.src1));
      break;
    case Opcode::print_string: {
      if (instruction.constant >= module_->constants.size()) invalid("constante fora dos limites");
      const auto &literal = module_->constants[instruction.constant].value;
      if (literal.size() < 2) invalid("literal de string inválido");
      output << literal.substr(1, literal.size() - 2);
      break;
    }
    case Opcode::print_newline: output << '\n'; break;
    case Opcode::ret:
      return register_value(instruction.src1);
    default:
      invalid("opcode não suportado pela VM");
    }
  }
}
} // namespace argos::vm
