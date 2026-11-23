#include "argos/ir/validator.hpp"
#include "argos/ir/instruction_traits.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace argos::ir {
namespace {

std::optional<std::string> validate_flow(const Function &function) {
  const auto size = function.blocks.size();
  std::unordered_map<LabelId, std::size_t> indices;
  for (std::size_t n = 0; n < size; ++n)
    indices.emplace(function.blocks[n].label, n);
  std::vector<std::vector<std::size_t>> successors(size), predecessors(size);
  for (std::size_t n = 0; n < size; ++n) {
    const auto &instructions = function.blocks[n].instructions;
    if (instructions.empty()) return "bloco vazio sem terminador";
    for (std::size_t p = 0; p < instructions.size(); ++p) {
      const auto &i = instructions[p];
      if ((i.opcode == Opcode::ret || i.opcode == Opcode::jmp) &&
          p + 1 != instructions.size())
        return "instrução após terminador de bloco";
      if (i.opcode == Opcode::jmp_if_false &&
          (p + 2 != instructions.size() ||
           instructions.back().opcode != Opcode::jmp))
        return "desvio condicional exige dois sucessores explícitos";
      if (i.opcode == Opcode::jmp || i.opcode == Opcode::jmp_if_false) {
        if (!indices.contains(i.label)) return "sucessor de bloco inexistente";
        const auto target = indices.at(i.label);
        successors[n].push_back(target);
        predecessors[target].push_back(n);
      }
    }
    if (instructions.back().opcode != Opcode::ret &&
        instructions.back().opcode != Opcode::jmp)
      return "bloco sem terminador";
  }
  std::vector<bool> reachable(size, false);
  std::vector<std::size_t> pending{0};
  while (!pending.empty()) {
    const auto n = pending.back(); pending.pop_back();
    if (reachable[n]) continue;
    reachable[n] = true;
    for (const auto target : successors[n]) pending.push_back(target);
  }
  const auto width = static_cast<std::size_t>(function.register_count) + function.slot_count;
  using Definitions = std::vector<bool>;
  Definitions entry(width, false);
  for (std::size_t p = 0; p < function.parameter_count; ++p)
    entry[function.register_count + p] = true;
  std::vector<Definitions> inputs(size, Definitions(width, true));
  std::vector<Definitions> outputs(size, Definitions(width, true));
  bool changed = true;
  while (changed) {
    changed = false;
    for (std::size_t n = 0; n < size; ++n) {
      if (!reachable[n]) continue;
      auto in = n == 0 ? entry : Definitions(width, true);
      if (n != 0)
        for (const auto predecessor : predecessors[n])
          if (reachable[predecessor])
            for (std::size_t bit = 0; bit < width; ++bit)
              in[bit] = in[bit] && outputs[predecessor][bit];
      auto out = in;
      for (const auto &i : function.blocks[n].instructions) {
        if (writes_register(i.opcode)) out[i.dst] = true;
        if (i.opcode == Opcode::store_var) out[function.register_count + i.slot] = true;
      }
      if (in != inputs[n] || out != outputs[n]) {
        inputs[n] = std::move(in); outputs[n] = std::move(out); changed = true;
      }
    }
  }
  for (std::size_t n = 0; n < size; ++n) {
    if (!reachable[n]) return "bloco inalcançável deve ser removido antes da validação";
    auto defined = inputs[n];
    for (const auto &i : function.blocks[n].instructions) {
      bool valid = true;
      visit_register_reads(i, [&](Register r) { valid = valid && defined[r]; });
      if (!valid) return "registrador lido antes de ser definido em todos os caminhos";
      if (i.opcode == Opcode::load_var && !defined[function.register_count + i.slot])
        return "slot lido antes de ser inicializado em todos os caminhos";
      if (writes_register(i.opcode)) defined[i.dst] = true;
      if (i.opcode == Opcode::store_var) defined[function.register_count + i.slot] = true;
    }
  }
  return std::nullopt;
}

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
      case Opcode::check_narrow:
        if (!has_register_type(instruction.src1, ValueType::int32) ||
            !has_register_type(instruction.dst, ValueType::int32) ||
            (instruction.count != 8 && instruction.count != 16 && instruction.count != 272))
          return "checagem de inteiro estreito inválida";
        break;
      case Opcode::raw_alloc:
        if (!has_register_type(instruction.src1, ValueType::int32) ||
            !has_register_type(instruction.dst, ValueType::raw_pointer))
          return "alocação manual exige tamanho int e resultado ptr";
        break;
      case Opcode::raw_free:
        if (!has_register_type(instruction.src1, ValueType::raw_pointer) ||
            !has_register_type(instruction.dst, ValueType::void_type))
          return "liberação manual exige ptr e resultado void";
        break;
      case Opcode::raw_load:
      case Opcode::raw_store:
        if (!has_register_type(instruction.src1, ValueType::raw_pointer) ||
            !has_register_type(instruction.src2, ValueType::int32))
          return "acesso manual exige ptr e deslocamento int";
        if (instruction.opcode == Opcode::raw_load) {
          if (!has_register_type(instruction.dst, ValueType::int32)) return "leitura manual exige resultado inteiro";
        } else if (instruction.arguments.size() != 1 ||
                   !has_register_type(instruction.arguments.front(), ValueType::int32) ||
                   !has_register_type(instruction.dst, ValueType::void_type))
          return "escrita manual exige valor inteiro e resultado void";
        break;
      case Opcode::check_uint8:
        if (!has_register_type(instruction.src1, ValueType::int32) ||
            !has_register_type(instruction.dst, ValueType::int32))
          return "uint_8 exige representação inteira";
        break;
      case Opcode::hash_scalar:
      case Opcode::stringify_scalar:
        if (!valid_register(instruction.src1) ||
            !has_register_type(instruction.dst,
                               instruction.opcode == Opcode::hash_scalar
                                   ? ValueType::int32
                                   : ValueType::string) ||
            (register_type(instruction.src1) != ValueType::int32 &&
             register_type(instruction.src1) != ValueType::boolean &&
             register_type(instruction.src1) != ValueType::float64 &&
             register_type(instruction.src1) != ValueType::string))
          return "operando escalar inválido para data";
        break;
      case Opcode::concat_strings:
        if (!has_register_type(instruction.src1, ValueType::string) ||
            !has_register_type(instruction.src2, ValueType::string) ||
            !has_register_type(instruction.dst, ValueType::string))
          return "concatenação exige strings";
        break;
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
            if (register_type(instruction.dst) == ValueType::boolean && value != 0 && value != 1)
              return "constante bool deve ser 0 ou 1";
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
      case Opcode::round_float:
        if (!has_register_type(instruction.dst, ValueType::float64) ||
            !has_register_type(instruction.src1, ValueType::float64))
          return "arredondamento binary32 exige representação real";
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
      case Opcode::call_virtual:
        if (instruction.constant >= module.virtual_functions.size() ||
            instruction.function !=
                module.virtual_functions[instruction.constant] ||
            instruction.arguments.empty() ||
            !has_register_type(instruction.arguments.front(),
                               ValueType::object))
          return "slot ou receptor virtual inválido";
        [[fallthrough]];
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
      default:
        return "opcode desconhecido";
      }
    }
  }
  return validate_flow(function);
}

} // namespace

std::optional<std::string> validate_module(const Module &module) {
  if (module.functions.empty())
    return "módulo sem funções";

  bool manual = false, managed = !module.objects.empty();
  for (const auto &function : module.functions) for (const auto &block : function.blocks)
    for (const auto &i : block.instructions) {
      manual = manual || i.opcode == Opcode::raw_alloc || i.opcode == Opcode::raw_free ||
                         i.opcode == Opcode::raw_load || i.opcode == Opcode::raw_store;
      managed = managed || i.opcode == Opcode::new_object || i.opcode == Opcode::make_closure ||
                           i.opcode == Opcode::make_array || i.opcode == Opcode::make_optional ||
                           i.opcode == Opcode::concat_strings || i.opcode == Opcode::stringify_scalar;
    }
  if (manual && managed) return "memória manual e arena gerenciada não podem coexistir no mesmo módulo";

  for (const auto representative : module.virtual_functions)
    if (representative >= module.functions.size())
      return "assinatura virtual inexistente";
  for (std::size_t id = 0; id < module.objects.size(); ++id) {
    const auto &layout = module.objects[id];
    if (layout.methods.size() != module.virtual_functions.size())
      return "tabela virtual incompleta";
    std::unordered_set<std::uint32_t> owned;
    for (const auto field : layout.owned_fields)
      if (!owned.insert(field).second || field >= layout.fields.size() ||
          layout.fields[field] != ValueType::object)
        return "campo de composição exclusiva inválido";
    std::unordered_set<std::uint32_t> ancestors;
    for (const auto ancestor : layout.ancestors)
      if (!ancestors.insert(ancestor).second)
        return "ancestral duplicado";
    for (const auto ancestor : layout.ancestors) {
      if (ancestor >= module.objects.size() || ancestor == id)
        return "ancestral inválido";
      const auto &base = module.objects[ancestor];
      for (const auto inherited : base.ancestors)
        if (inherited == id || !ancestors.contains(inherited))
          return "ciclo ou cadeia incompleta de ancestrais";
      for (const auto field : base.owned_fields)
        if (!owned.contains(field))
          return "ownership herdado incompleto";
      if (base.fields.size() > layout.fields.size())
        return "layout herdado incompleto";
      for (std::size_t field = 0; field < base.fields.size(); ++field)
        if (base.fields[field] != layout.fields[field])
          return "prefixo de campos herdados incompatível";
    }
    for (std::size_t slot = 0; slot < layout.methods.size(); ++slot) {
      const auto target = layout.methods[slot];
      if (target == UINT32_MAX)
        continue;
      if (target >= module.functions.size())
        return "método virtual inexistente";
      const auto &actual = module.functions[target];
      const auto &expected = module.functions[module.virtual_functions[slot]];
      if (actual.parameter_types != expected.parameter_types ||
          actual.return_type != expected.return_type)
        return "assinatura de método virtual incompatível";
    }
  }
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
      if (has_entry) return "múltiplas funções main";
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
