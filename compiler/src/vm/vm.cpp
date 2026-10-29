#include "argos/vm/vm.hpp"
#include "argos/ir/validator.hpp"
#include <algorithm>

#include <bit>
#include <charconv>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace argos::vm {

namespace {

[[noreturn]] void invalid(std::string_view detail) {
  throw std::runtime_error("IR inválida para VM: " + std::string{detail});
}

} // namespace

std::int32_t VM::run() const {
  arrays_.clear();
  optionals_.clear();
  closures_.clear();
  objects_.clear();
  strings_.clear();
  struct HeapReset {
    decltype(arrays_) &arrays;
    decltype(optionals_) &optionals;
    decltype(closures_)& closures;
    decltype(objects_)& objects;
    decltype(strings_) &strings;
    ~HeapReset() {
      arrays.clear();
      optionals.clear();
      closures.clear();
      while (!objects.empty())
        objects.pop_back();
      strings.clear();
    }
  } reset{arrays_, optionals_, closures_, objects_, strings_};
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

  const auto string_value = [&](std::uint64_t handle) -> std::string_view {
    if (handle == 0)
      invalid("string ausente");
    if (handle <= module_->constants.size()) {
      const auto &literal = module_->constants[handle - 1].value;
      return std::string_view{literal};
    }
    const auto index = handle - module_->constants.size() - 1;
    if (index >= strings_.size())
      invalid("handle de string inválido");
    return strings_[index];
  };
  const auto intern_string = [&](std::string value) {
    strings_.push_back(std::move(value));
    return static_cast<std::uint64_t>(module_->constants.size() +
                                      strings_.size());
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
    case Opcode::check_uint8:
      if (integer_value(instruction.src1) < 0 ||
          integer_value(instruction.src1) > 255)
        throw std::runtime_error("uint_8 fora do intervalo 0..255 em " +
                                 function.name + ":" +
                                 std::to_string(instruction.source_line) + ":" +
                                 std::to_string(instruction.source_column));
      set_register(instruction.dst, register_value(instruction.src1));
      break;
    case Opcode::hash_scalar: {
      const auto type = function.register_types[instruction.src1];
      std::uint32_t value;
      if (type == argos::ir::ValueType::string) {
        value = 2166136261u;
        for (const char byte :
             string_value(register_value(instruction.src1)))
          value = (value ^ static_cast<unsigned char>(byte)) * 16777619u;
      } else if (type == argos::ir::ValueType::float64) {
        const auto number = double_value(instruction.src1);
        const auto bits = number == 0.0 ? std::uint64_t{0}
                                        : std::bit_cast<std::uint64_t>(number);
        value = static_cast<std::uint32_t>(bits ^ (bits >> 32));
      } else
        value = static_cast<std::uint32_t>(register_value(instruction.src1));
      set_register(instruction.dst,
                   static_cast<std::uint64_t>(static_cast<std::int64_t>(
                       static_cast<std::int32_t>(value))));
      break;
    }
    case Opcode::stringify_scalar: {
      std::ostringstream text;
      const auto type = function.register_types[instruction.src1];
      if (type == argos::ir::ValueType::boolean)
        text << (register_value(instruction.src1) ? "true" : "false");
      else if (type == argos::ir::ValueType::float64)
        text << std::setprecision(17) << std::defaultfloat
             << double_value(instruction.src1);
      else if (type == argos::ir::ValueType::string)
        text << string_value(register_value(instruction.src1));
      else
        text << integer_value(instruction.src1);
      set_register(instruction.dst, intern_string(text.str()));
      break;
    }
    case Opcode::concat_strings: {
      const auto text =
          std::string{string_value(register_value(instruction.src1))} +
          std::string{string_value(register_value(instruction.src2))};
      set_register(instruction.dst, intern_string(text));
      break;
    }
    case Opcode::make_nil:
      set_register(instruction.dst, 0);
      break;
    case Opcode::make_optional:
      optionals_.push_back(register_value(instruction.src1));
      set_register(instruction.dst, optionals_.size());
      break;
    case Opcode::is_nil:
      set_register(instruction.dst, register_value(instruction.src1) == 0);
      break;
    case Opcode::unwrap_check: {
      const auto id = register_value(instruction.src1);
      if (!id)
        throw std::runtime_error("tentativa de desembrulhar nil em " +
                                 function.name + ":" +
                                 std::to_string(instruction.source_line) + ":" +
                                 std::to_string(instruction.source_column));
      if (id > optionals_.size())
        invalid("handle opcional inválido");
      set_register(instruction.dst, optionals_[id - 1]);
      break;
    }
    case Opcode::make_array: {
      std::vector<std::uint64_t> values;
      for (const auto r : instruction.arguments)
        values.push_back(register_value(r));
      arrays_.push_back(std::move(values));
      set_register(instruction.dst, arrays_.size());
      break;
    }
    case Opcode::array_store:
    case Opcode::array_load: {
      const auto id = register_value(instruction.src1);
      if (!id || id > arrays_.size())
        invalid("array inválido");
      const auto index = integer_value(instruction.src2);
      auto &values = arrays_[id - 1];
      if (index < 0 || static_cast<std::size_t>(index) >= values.size())
        throw std::runtime_error("índice fora dos limites em " + function.name +
                                 ":" + std::to_string(instruction.source_line) +
                                 ":" +
                                 std::to_string(instruction.source_column));
      if (instruction.opcode == Opcode::array_load)
        set_register(instruction.dst, values[static_cast<std::size_t>(index)]);
      else
        values[static_cast<std::size_t>(index)] =
            register_value(instruction.arguments[0]);
      break;
    }
    case Opcode::nop:
      break;
    case Opcode::new_object:
      objects_.push_back({instruction.constant, std::vector<std::uint64_t>(module_->objects[instruction.constant].fields.size())});
      set_register(instruction.dst, objects_.size());
      break;
    case Opcode::load_field:
    case Opcode::store_field: {
      const auto id = register_value(instruction.src1);
      if (id == 0 || id > objects_.size() ||
          (objects_[id - 1].layout != instruction.constant &&
           std::find(
               module_->objects[objects_[id - 1].layout].ancestors.begin(),
               module_->objects[objects_[id - 1].layout].ancestors.end(),
               instruction.constant) ==
               module_->objects[objects_[id - 1].layout].ancestors.end()))
        invalid("objeto incompatível com layout do campo");
      auto& object = objects_[id - 1];
      if (instruction.opcode == Opcode::load_field) set_register(instruction.dst, object.fields[instruction.count]);
      else {
        const auto value = register_value(instruction.src2);
        const auto &owned = module_->objects[instruction.constant].owned_fields;
        if (std::find(owned.begin(), owned.end(), instruction.count) !=
            owned.end()) {
          if (!value || value > objects_.size() || value <= id ||
              object.fields[instruction.count] != 0 ||
              objects_[value - 1].owner != 0)
            invalid("componente exclusivo já inicializado, compartilhado ou "
                    "cíclico");
          objects_[value - 1].owner = id;
        }
        object.fields[instruction.count] = value;
      }
      break;
    }
    case Opcode::make_closure: {
      Closure closure{instruction.function, {}};
      for (const auto argument : instruction.arguments) closure.captures.push_back(register_value(argument));
      closures_.push_back(std::move(closure));
      set_register(instruction.dst, closures_.size());
      break;
    }
    case Opcode::load_capture: {
      const auto id = register_value(instruction.src1);
      if (id == 0 || id > closures_.size()) invalid("ambiente de closure inválido");
      const auto& closure = closures_[id - 1];
      if (closure.function != function_id || instruction.count >= closure.captures.size())
        invalid("captura fora do ambiente");
      set_register(instruction.dst, closure.captures[instruction.count]);
      break;
    }
    case Opcode::call_closure: {
      const auto id = register_value(instruction.src1);
      if (id == 0 || id > closures_.size()) invalid("closure inválida");
      const auto target = closures_[id - 1].function;
      const auto& signature = module_->functions[target];
      if (signature.return_type != instruction.return_type ||
          signature.parameter_types.size() != instruction.parameter_types.size() + 1)
        invalid("assinatura de closure incompatível");
      for (std::size_t i = 0; i < instruction.parameter_types.size(); ++i)
        if (signature.parameter_types[i + 1] != instruction.parameter_types[i]) invalid("parâmetro de closure incompatível");
      std::vector<std::uint64_t> values{id};
      for (const auto argument : instruction.arguments) values.push_back(register_value(argument));
      set_register(instruction.dst, execute(target, values));
      break;
    }
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
          invalid("constante int inválida");
        set_register(instruction.dst, static_cast<std::uint64_t>(
                                          static_cast<std::int64_t>(value)));
      }
      break;
    }
    case Opcode::load_string:
      if (instruction.constant >= module_->constants.size())
        invalid("constante de string fora dos limites");
      set_register(instruction.dst,
                   static_cast<std::uint64_t>(instruction.constant) + 1);
      break;
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
      if (function.register_types[instruction.src1] ==
          argos::ir::ValueType::string) {
        const auto left = register_value(instruction.src1);
        const auto right = register_value(instruction.src2);
        set_register(instruction.dst,
                     string_value(left) == string_value(right));
        break;
      }
      set_register(instruction.dst,
                   function.register_types[instruction.src1] ==
                           argos::ir::ValueType::float64
                       ? double_value(instruction.src1) ==
                             double_value(instruction.src2)
                       : register_value(instruction.src1) ==
                             register_value(instruction.src2));
      break;
    case Opcode::cmp_ne:
      if (function.register_types[instruction.src1] ==
          argos::ir::ValueType::string) {
        const auto left = register_value(instruction.src1);
        const auto right = register_value(instruction.src2);
        set_register(instruction.dst,
                     string_value(left) != string_value(right));
        break;
      }
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
    case Opcode::call_virtual:
    case Opcode::call: {
      auto target = instruction.function;
      if (instruction.opcode == Opcode::call_virtual) {
        const auto receiver = register_value(instruction.arguments.front());
        if (receiver == 0 || receiver > objects_.size())
          invalid("receptor virtual inválido");
        target = module_->objects[objects_[receiver - 1].layout]
                     .methods[instruction.constant];
        if (target == UINT32_MAX)
          invalid("método abstrato sem implementação");
      }
      std::vector<std::uint64_t> call_arguments;
      for (const auto reg : instruction.arguments) call_arguments.push_back(register_value(reg));
      try {
        set_register(instruction.dst, execute(target, call_arguments));
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
    case Opcode::print_string_value: {
      const auto value = register_value(instruction.src1);
      output << string_value(value);
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
