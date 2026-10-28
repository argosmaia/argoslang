#include "argos/ir/validator.hpp"

#include <charconv>
#include <cstdint>
#include <unordered_set>

namespace argos::ir {
namespace {

std::optional<std::string> validate_function(const Module &module,
                                             const Function &function) {
  if (function.parameter_count > function.slot_count)
    return "parâmetros excedem os slots disponíveis";
  if (function.parameter_types.size() != function.parameter_count ||
      function.slot_types.size() != function.slot_count ||
      function.register_types.size() != function.register_count)
    return "metadados de tipos não correspondem aos tamanhos da função";
  if (!valid_type(function.return_type, true))
    return "tipo de retorno desconhecido";
  for (const auto type : function.slot_types)
    if (!valid_type(type))
      return "slot possui tipo inválido";
  for (const auto type : function.parameter_types)
    if (!valid_type(type))
      return "parâmetro possui tipo inválido";
  for (const auto type : function.register_types)
    if (!valid_type(type, true))
      return "registrador possui tipo desconhecido";
  for (std::size_t index = 0; index < function.parameter_types.size(); ++index) {
    if (function.parameter_types[index] != function.slot_types[index])
      return "tipo do parâmetro não corresponde ao slot inicial";
  }
  if (function.blocks.empty())
    return "função sem blocos";

  std::unordered_set<LabelId> labels;
  for (const auto &block : function.blocks) {
    if (!labels.insert(block.label).second)
      return "rótulo de bloco duplicado";
  }

  const auto valid_register = [&](Register reg) {
    return reg < function.register_count;
  };
  const auto valid_slot = [&](Slot slot) { return slot < function.slot_count; };
  const auto register_type = [&](Register reg) {
    return function.register_types[reg];
  };
  const auto has_register_type = [&](Register reg, ValueType type) {
    return valid_register(reg) && register_type(reg) == type;
  };
  const auto same_register_type = [&](Register left, Register right) {
    return valid_register(left) && valid_register(right) &&
           register_type(left) == register_type(right);
  };
  const auto valid_constant = [&](ConstantId id) {
    return id < module.constants.size() && module.constants[id].id == id;
  };
  const auto valid_target = [&](LabelId id) { return labels.contains(id); };

  for (const auto &block : function.blocks) {
    for (const auto &instruction : block.instructions) {
      switch (instruction.opcode) {
      case Opcode::make_nil:
        if (!valid_register(instruction.dst) ||
            !is_optional(register_type(instruction.dst)))
          return "nil exige destino opcional";
        break;
      case Opcode::make_optional:
        if (!valid_register(instruction.dst) ||
            !valid_register(instruction.src1) ||
            !is_optional(register_type(instruction.dst)) ||
            unwrap_type(register_type(instruction.dst)) !=
                register_type(instruction.src1))
          return "presença exige destino com uma camada opcional a mais";
        break;
      case Opcode::is_nil:
        if (!has_register_type(instruction.dst, ValueType::boolean) ||
            !valid_register(instruction.src1) ||
            !is_optional(register_type(instruction.src1)))
          return "teste de ausência exige origem opcional e resultado bool";
        break;
      case Opcode::unwrap_check:
        if (!valid_register(instruction.dst) ||
            !valid_register(instruction.src1) ||
            !is_optional(register_type(instruction.src1)) ||
            unwrap_type(register_type(instruction.src1)) !=
                register_type(instruction.dst))
          return "unwrap exige origem opcional e destino desembrulhado";
        break;
      case Opcode::make_array:
        if (!valid_register(instruction.dst) ||
            is_optional(register_type(instruction.dst)) ||
            payload_type(register_type(instruction.dst)) != ValueType::array)
          return "array exige destino array";
        for (const auto r : instruction.arguments)
          if (!has_register_type(r,
                                 array_element(register_type(instruction.dst))))
            return "elemento de array incompatível";
        break;
      case Opcode::array_store:
      case Opcode::array_load:
        if (!valid_register(instruction.src1) ||
            is_optional(register_type(instruction.src1)) ||
            payload_type(register_type(instruction.src1)) != ValueType::array ||
            !has_register_type(instruction.src2, ValueType::int32))
          return "indexação exige array e índice int";
        if (instruction.opcode == Opcode::array_store) {
          if (instruction.arguments.size() != 1 ||
              !has_register_type(
                  instruction.arguments[0],
                  array_element(register_type(instruction.src1))))
            return "escrita de array incompatível";
        } else if (!has_register_type(
                       instruction.dst,
                       array_element(register_type(instruction.src1))))
          return "leitura de array incompatível";
        break;
      case Opcode::nop:
      case Opcode::print_newline:
        break;
      case Opcode::load_const:
        if (!valid_register(instruction.dst) ||
            !valid_constant(instruction.constant))
          return "constante ou registrador fora dos limites";
        if (register_type(instruction.dst) != ValueType::int32 &&
            register_type(instruction.dst) != ValueType::boolean &&
            register_type(instruction.dst) != ValueType::float64)
          return "constante exige tipo numérico ou bool";
        {
          const auto &text = module.constants[instruction.constant].value;
          if (register_type(instruction.dst) == ValueType::float64) {
            double value{};
            const auto [end, error] =
                std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size())
              return "constante double inválida";
          } else {
            std::int32_t value{};
            const auto [end, error] =
                std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc{} || end != text.data() + text.size())
              return "constante numérica inválida";
          }
        }
        break;
      case Opcode::load_string:
        if (!has_register_type(instruction.dst, ValueType::string) ||
            !valid_constant(instruction.constant))
          return "constante de string ou registrador fora dos limites";
        break;
      case Opcode::load_var:
        if (!valid_register(instruction.dst) || !valid_slot(instruction.slot))
          return "slot ou registrador fora dos limites";
        if (register_type(instruction.dst) != function.slot_types[instruction.slot])
          return "tipo do registrador não corresponde ao slot lido";
        break;
      case Opcode::store_var:
        if (!valid_register(instruction.src1) || !valid_slot(instruction.slot))
          return "slot ou registrador fora dos limites";
        if (register_type(instruction.src1) != function.slot_types[instruction.slot])
          return "tipo do valor não corresponde ao slot escrito";
        break;
      case Opcode::neg:
        if (!valid_register(instruction.dst) || !valid_register(instruction.src1))
          return "registrador fora dos limites";
        if (register_type(instruction.dst) != register_type(instruction.src1) ||
            (register_type(instruction.dst) != ValueType::int32 &&
             register_type(instruction.dst) != ValueType::float64))
          return "negação aritmética exige registradores numéricos compatíveis";
        break;
      case Opcode::int_to_double:
        if (!has_register_type(instruction.dst, ValueType::float64) ||
            !has_register_type(instruction.src1, ValueType::int32))
          return "conversão exige origem int e destino double";
        break;
      case Opcode::logical_not:
        if (!has_register_type(instruction.dst, ValueType::boolean) ||
            !has_register_type(instruction.src1, ValueType::boolean))
          return "negação lógica exige registradores bool";
        break;
      case Opcode::add:
      case Opcode::sub:
      case Opcode::mul:
      case Opcode::div:
      case Opcode::mod:
      case Opcode::cmp_eq:
      case Opcode::cmp_ne:
      case Opcode::cmp_lt:
      case Opcode::cmp_le:
      case Opcode::cmp_gt:
      case Opcode::cmp_ge:
        if (!valid_register(instruction.dst) ||
            !valid_register(instruction.src1) ||
            !valid_register(instruction.src2))
          return "registrador fora dos limites";
        if (instruction.opcode == Opcode::add ||
            instruction.opcode == Opcode::sub ||
            instruction.opcode == Opcode::mul ||
            instruction.opcode == Opcode::mod) {
          if (!same_register_type(instruction.src1, instruction.src2) ||
              register_type(instruction.dst) != register_type(instruction.src1) ||
              (register_type(instruction.dst) != ValueType::int32 &&
               register_type(instruction.dst) != ValueType::float64) ||
              (instruction.opcode == Opcode::mod &&
               register_type(instruction.dst) != ValueType::int32))
            return "operação aritmética exige registradores numéricos compatíveis";
        } else if (instruction.opcode == Opcode::div) {
          const auto numeric = [](ValueType type) {
            return type == ValueType::int32 || type == ValueType::float64;
          };
          if (!has_register_type(instruction.dst, ValueType::float64) ||
              !numeric(register_type(instruction.src1)) ||
              !numeric(register_type(instruction.src2)))
            return "divisão real exige operandos numéricos e resultado double";
        } else if (!same_register_type(instruction.src1, instruction.src2) ||
                   !has_register_type(instruction.dst, ValueType::boolean)) {
          return "comparação exige operandos compatíveis e resultado bool";
        } else if (is_optional(register_type(instruction.src1)) ||
                   register_type(instruction.src1) == ValueType::void_type ||
                   register_type(instruction.src1) == ValueType::closure) {
          return "igualdade exige payload desembrulhado comparável";
        } else if (instruction.opcode != Opcode::cmp_eq &&
                   instruction.opcode != Opcode::cmp_ne &&
                   register_type(instruction.src1) != ValueType::int32 &&
                   register_type(instruction.src1) != ValueType::float64) {
          return "comparação relacional exige operandos numéricos";
        }
        break;
      case Opcode::jmp:
        if (!valid_target(instruction.label))
          return "salto aponta para um bloco inexistente";
        break;
      case Opcode::jmp_if_false:
        if (!has_register_type(instruction.src1, ValueType::boolean) ||
            !valid_target(instruction.label))
          return "desvio condicional inválido";
        break;
      case Opcode::new_object:
        if (!has_register_type(instruction.dst, ValueType::object) || instruction.constant >= module.objects.size())
          return "alocação de objeto inválida";
        break;
      case Opcode::load_field:
      case Opcode::store_field:
        if (!has_register_type(instruction.src1, ValueType::object) || instruction.constant >= module.objects.size() ||
            instruction.count >= module.objects[instruction.constant].fields.size()) return "campo inexistente";
        if (!has_register_type(instruction.opcode == Opcode::load_field ? instruction.dst : instruction.src2,
                               module.objects[instruction.constant].fields[instruction.count])) return "tipo de campo incompatível";
        break;
      case Opcode::make_closure: {
        if (!has_register_type(instruction.dst, ValueType::closure) ||
            instruction.function >= module.functions.size()) return "closure aponta para função inexistente";
        const auto& target = module.functions[instruction.function];
        if (target.capture_types.size() != instruction.arguments.size()) return "capturas incompatíveis";
        if (target.parameter_types.empty() || target.parameter_types.front() != ValueType::closure)
          return "closure exige parâmetro de ambiente";
        for (std::size_t i = 0; i < instruction.arguments.size(); ++i)
          if (!has_register_type(instruction.arguments[i], target.capture_types[i])) return "tipo de captura incompatível";
        break;
      }
      case Opcode::load_capture:
        if (!has_register_type(instruction.src1, ValueType::closure) ||
            instruction.count >= function.capture_types.size() ||
            !has_register_type(instruction.dst, function.capture_types[instruction.count]))
          return "leitura de captura inválida";
        break;
      case Opcode::call_closure:
        if (!has_register_type(instruction.src1, ValueType::closure) ||
            !has_register_type(instruction.dst, instruction.return_type) ||
            instruction.parameter_types.size() != instruction.arguments.size() ||
            instruction.arguments.size() > 5) return "assinatura de chamada de closure inválida";
        for (std::size_t i = 0; i < instruction.arguments.size(); ++i)
          if (!has_register_type(instruction.arguments[i], instruction.parameter_types[i]) ||
              instruction.parameter_types[i] == ValueType::void_type) return "argumento de closure inválido";
        break;
      case Opcode::call:
        if (!valid_register(instruction.dst) ||
            instruction.function >= module.functions.size())
          return "chamada aponta para uma função inexistente";
        if (instruction.arguments.size() > 6)
          return "chamada excede o limite de seis argumentos";
        if (module.functions[instruction.function].parameter_types.size() !=
            module.functions[instruction.function].parameter_count)
          return "metadados de parâmetros da função chamada são inválidos";
        for (const Register argument : instruction.arguments)
          if (!valid_register(argument))
            return "argumento de chamada usa registrador inexistente";
        if (instruction.arguments.size() !=
            module.functions[instruction.function].parameter_count)
          return "quantidade de argumentos da chamada não corresponde à função";
        for (std::size_t index = 0; index < instruction.arguments.size(); ++index) {
          if (register_type(instruction.arguments[index]) !=
              module.functions[instruction.function].parameter_types[index])
            return "tipo de argumento não corresponde ao parâmetro";
        }
        if (register_type(instruction.dst) !=
            module.functions[instruction.function].return_type)
          return "tipo do resultado não corresponde ao retorno da função";
        break;
      case Opcode::print_int:
        if (!has_register_type(instruction.src1, ValueType::int32))
          return "print de int exige um registrador int";
        break;
      case Opcode::print_bool:
        if (!has_register_type(instruction.src1, ValueType::boolean))
          return "print de bool exige um registrador bool";
        break;
      case Opcode::print_double:
        if (!has_register_type(instruction.src1, ValueType::float64))
          return "print de double exige um registrador double";
        break;
      case Opcode::print_string:
        if (!valid_constant(instruction.constant))
          return "impressão usa constante inexistente";
        if (module.constants[instruction.constant].value.size() < 2 ||
            module.constants[instruction.constant].value.front() != '"' ||
            module.constants[instruction.constant].value.back() != '"')
          return "literal de string malformado";
        break;
      case Opcode::print_string_value:
        if (!has_register_type(instruction.src1, ValueType::string))
          return "print de string exige um registrador string";
        break;
      case Opcode::ret:
        if (!valid_register(instruction.src1))
          return "retorno usa registrador inexistente";
        if (function.return_type == ValueType::void_type) {
          if (register_type(instruction.src1) != ValueType::int32 &&
              register_type(instruction.src1) != ValueType::float64 &&
              register_type(instruction.src1) != ValueType::void_type)
            return "retorno void contém um valor incompatível";
        } else if (register_type(instruction.src1) != function.return_type) {
          return "tipo do valor retornado não corresponde à função";
        }
        break;
      case Opcode::c_embed:
        return "opcode ainda não pertence ao subconjunto executável";
      }
    }
  }
  return std::nullopt;
}

} // namespace

std::optional<std::string> validate_module(const Module &module) {
  if (module.functions.empty())
    return "módulo sem funções";

  const auto storage_type = [](ValueType type) { return valid_type(type); };
  for (const auto& layout : module.objects)
    for (const auto type : layout.fields)
      if (!storage_type(type)) return "campo possui tipo de armazenamento inválido";
  for (const auto& function : module.functions)
    for (const auto type : function.capture_types)
      if (!storage_type(type)) return "captura possui tipo de armazenamento inválido";
  bool has_entry = false;
  for (std::size_t index = 0; index < module.constants.size(); ++index) {
    if (module.constants[index].id != index)
      return "identificador de constante inconsistente";
  }
  for (std::size_t index = 0; index < module.functions.size(); ++index) {
    const auto &function = module.functions[index];
    if (function.id != index)
      return "identificador de função inconsistente";
    if (function.name == "main") {
      has_entry = true;
      if (function.parameter_count != 0 ||
          (function.return_type != ValueType::int32 &&
           function.return_type != ValueType::void_type))
        return "main deve ser sem parâmetros e retornar int ou void";
    }
    if (const auto error = validate_function(module, function))
      return "função " + function.name + ": " + *error;
  }
  if (!has_entry)
    return "módulo sem função main";
  return std::nullopt;
}

} // namespace argos::ir
