#include "argos/codegen/ir/executable_ir_generator.hpp"
#include "argos/diagnostics/diagnostic_catalog.hpp"

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace argos::codegen::ir {
namespace {

using argos::ir::Instruction;
using argos::ir::Opcode;
using argos::ir::Register;
using argos::ir::Slot;
using argos::ir::LabelId;
using argos::ir::ValueType;

bool supported_type(const ast::TypeSyntax *type) {
  if (type && type->result_type) {
    if (type->nullable_depth != 0) return false;
    for (const auto& parameter : type->parameter_types)
      if (!supported_type(parameter.get())) return false;
    return type->result_type->name == "void" || supported_type(type->result_type.get());
  }
  return type && type->nullable_depth == 0 &&
         (type->name == "int" || type->name == "bool" ||
          type->name == "float" || type->name == "double" ||
          type->name == "string");
}

std::optional<std::string> decode_string_literal(std::string_view spelling) {
  const bool multiline = spelling.starts_with("\"\"\"");
  const std::size_t delimiter_size = multiline ? 3 : 1;
  if (spelling.size() < delimiter_size * 2) return std::nullopt;
  spelling.remove_prefix(delimiter_size);
  spelling.remove_suffix(delimiter_size);

  std::string result;
  const auto append_code_point = [&](std::uint32_t value) {
    if (value <= 0x7f) {
      result.push_back(static_cast<char>(value));
    } else if (value <= 0x7ff) {
      result.push_back(static_cast<char>(0xc0 | (value >> 6)));
      result.push_back(static_cast<char>(0x80 | (value & 0x3f)));
    } else if (value <= 0xffff) {
      result.push_back(static_cast<char>(0xe0 | (value >> 12)));
      result.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
      result.push_back(static_cast<char>(0x80 | (value & 0x3f)));
    } else {
      result.push_back(static_cast<char>(0xf0 | (value >> 18)));
      result.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
      result.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
      result.push_back(static_cast<char>(0x80 | (value & 0x3f)));
    }
  };

  for (std::size_t index = 0; index < spelling.size(); ++index) {
    const char character = spelling[index];
    if (character != '\\') {
      result.push_back(character);
      continue;
    }
    if (++index >= spelling.size()) return std::nullopt;
    switch (spelling[index]) {
    case '0': result.push_back('\0'); break;
    case 't': result.push_back('\t'); break;
    case 'n': result.push_back('\n'); break;
    case 'r': result.push_back('\r'); break;
    case '\\': result.push_back('\\'); break;
    case '"': result.push_back('"'); break;
    case 'u': {
      if (index + 1 >= spelling.size() || spelling[index + 1] != '{')
        return std::nullopt;
      const auto digits_begin = index + 2;
      const auto digits_end = spelling.find('}', digits_begin);
      if (digits_end == std::string_view::npos || digits_end == digits_begin)
        return std::nullopt;
      std::uint32_t value{};
      const auto [end, error] = std::from_chars(
          spelling.data() + digits_begin, spelling.data() + digits_end, value,
          16);
      if (error != std::errc{} || end != spelling.data() + digits_end ||
          value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
        return std::nullopt;
      append_code_point(value);
      index = digits_end;
      break;
    }
    default:
      return std::nullopt;
    }
  }
  return result;
}

struct ClassLayout {
  const ast::ClassDeclaration* declaration{};
  std::uint32_t id{};
  std::vector<const ast::VariableDeclaration*> fields;
};
using ClassLayouts = std::unordered_map<std::string, ClassLayout>;

struct WorkItem {
  const ast::FunctionDeclaration* function{};
  const ast::LambdaExpression* lambda{};
  std::optional<argos::ir::FunctionId> adapter;
  const ast::ClassDeclaration* owner{};
};

class Lowerer final {
public:
  Lowerer(argos::ir::Module &module, diagnostics::DiagnosticEngine &diagnostics,
          const semantics::SemanticAnalyzer &semantics,
          const ast::FunctionDeclaration &source,
          const std::unordered_map<std::string, argos::ir::FunctionId> &ids,
          const std::unordered_map<std::string, std::size_t> &arities,
          argos::ir::FunctionId id, std::vector<WorkItem>& work, const ClassLayouts& layouts,
          const ast::ClassDeclaration* owner = nullptr)
      : module_(module), diagnostics_(diagnostics), semantics_(semantics),
        source_(source), parameters_(source.parameters), return_type_(source.return_type.get()),
        body_(source.body.get()), ids_(ids), arities_(arities), work_(work), layouts_(layouts), owner_(owner) {
    function_.id = id;
    function_.name = owner ? owner->name + "." + source.name : source.name;
    scopes_.emplace_back();
    current_ = make_block();
  }

  Lowerer(argos::ir::Module& module, diagnostics::DiagnosticEngine& diagnostics,
          const semantics::SemanticAnalyzer& semantics, const ast::LambdaExpression& source,
          const std::unordered_map<std::string, argos::ir::FunctionId>& ids,
          const std::unordered_map<std::string, std::size_t>& arities,
          argos::ir::FunctionId id, std::vector<WorkItem>& work, const ClassLayouts& layouts)
      : module_(module), diagnostics_(diagnostics), semantics_(semantics), source_(source),
        parameters_(source.parameters), return_type_(source.return_type.get()),
        body_(source.body.get()), lambda_(&source), ids_(ids), arities_(arities), work_(work), layouts_(layouts) {
    function_.id = id;
    function_.name = "<lambda_" + std::to_string(id) + ">";
    scopes_.emplace_back();
    current_ = make_block();
  }

  argos::ir::Function lower() {
    if (parameters_.size() + ((lambda_ || owner_) ? 1 : 0) > 6) {
      error(source_, "função suporta até seis parâmetros; lambda reserva um para o ambiente");
      return std::move(function_);
    }
    if (lambda_) {
      const auto type = semantics_.type_of(*lambda_);
      const auto returned = semantic_type(type.signature.back());
      if (!returned) { error(source_, "retorno da lambda sem representação executável"); return std::move(function_); }
      function_.return_type = *returned;
      function_.parameter_types.push_back(ValueType::closure);
      add_slot(ValueType::closure);
    } else {
      if (return_type_ && !supported_return_type(return_type_)) {
        error(source_, "tipo de retorno ainda não é compilável");
        return std::move(function_);
      }
      function_.return_type = return_type_ ? syntax_type(return_type_) : ValueType::void_type;
    }
    if (owner_) {
      function_.parameter_types.push_back(ValueType::object);
      scopes_.back().insert_or_assign("this", add_slot(ValueType::object));
    }
    function_.parameter_count = static_cast<std::uint32_t>(parameters_.size() + ((lambda_ || owner_) ? 1 : 0));
    for (const auto& parameter : parameters_) {
      if (!supported_type(parameter->type.get())) error(*parameter, "tipo de parâmetro ainda não é compilável");
      const auto type = syntax_type(parameter->type.get());
      function_.parameter_types.push_back(type);
      scopes_.back().insert_or_assign(parameter->name, add_slot(type));
    }
    if (lambda_) {
      Instruction environment;
      environment.opcode = Opcode::load_var;
      environment.slot = 0;
      environment.dst = reg(ValueType::closure);
      const auto env = environment.dst;
      emit(environment);
      for (const auto& capture : semantics_.captures_of(*lambda_)) {
        const auto type = semantic_type(capture.type);
        if (!type) { error(source_, "tipo de captura ainda não é compilável"); continue; }
        Instruction load;
        load.opcode = Opcode::load_capture;
        load.src1 = env;
        load.dst = reg(*type);
        load.count = static_cast<std::uint32_t>(function_.capture_types.size());
        function_.capture_types.push_back(*type);
        emit(load);
        const auto place = add_slot(*type);
        scopes_.back().insert_or_assign(capture.name, place);
        Instruction store;
        store.opcode = Opcode::store_var;
        store.src1 = load.dst;
        store.slot = place;
        emit(store);
      }
    }
    if (owner_ && static_cast<const ast::FunctionDeclaration&>(source_).name == "construct") {
      const auto& layout = layouts_.at(owner_->name);
      for (std::size_t i = 0; i < layout.fields.size(); ++i) {
        if (!layout.fields[i]->initializer) continue;
        const auto value = expression(*layout.fields[i]->initializer);
        if (value) store_field(layout, static_cast<std::uint32_t>(i), this_value(), *value);
      }
    }
    if (lambda_ && lambda_->expression_body) {
      const auto value = expression(*lambda_->expression_body);
      if (value) {
        Instruction ret;
        ret.opcode = Opcode::ret;
        ret.src1 = *value;
        emit(ret);
      }
    } else if (body_) {
      if (!block(*body_) && function_.return_type == ValueType::void_type) {
        Instruction ret;
        ret.opcode = Opcode::ret;
        ret.src1 = constant("0");
        emit(ret);
      }
    } else error(source_, "função sem corpo não pode ser executada");
    return std::move(function_);
  }

private:
  bool supported_type(const ast::TypeSyntax* type) const {
    return ::argos::codegen::ir::supported_type(type) ||
           (type && type->nullable_depth == 0 && layouts_.contains(type->name));
  }
  bool supported_return_type(const ast::TypeSyntax* type) const {
    return supported_type(type) || (type && type->nullable_depth == 0 && type->name == "void");
  }
  Register this_value() {
    Instruction load;
    load.opcode = Opcode::load_var;
    load.slot = *slot("this");
    load.dst = reg(ValueType::object);
    emit(load);
    return load.dst;
  }
  std::optional<std::uint32_t> field_index(const ClassLayout& layout, const std::string& name) const {
    for (std::size_t i = 0; i < layout.fields.size(); ++i)
      if (layout.fields[i]->name == name) return static_cast<std::uint32_t>(i);
    return std::nullopt;
  }
  Register load_field(const ClassLayout& layout, std::uint32_t index, Register base) {
    Instruction load;
    load.opcode = Opcode::load_field;
    load.constant = layout.id;
    load.count = index;
    load.src1 = base;
    load.dst = reg(module_.objects[layout.id].fields[index]);
    emit(load);
    return load.dst;
  }
  void store_field(const ClassLayout& layout, std::uint32_t index, Register base, Register value) {
    Instruction store;
    store.opcode = Opcode::store_field;
    store.constant = layout.id;
    store.count = index;
    store.src1 = base;
    store.src2 = value;
    emit(store);
  }
  Register accessor_call(argos::ir::FunctionId id, Register receiver,
                         std::optional<Register> value = std::nullopt) {
    Instruction call;
    call.opcode = Opcode::call;
    call.function = id;
    call.dst = reg(value ? ValueType::void_type : syntax_type(work_[id].function->return_type.get()));
    call.arguments = {receiver};
    if (value) call.arguments.push_back(*value);
    emit(call);
    return call.dst;
  }
  std::optional<Register> direct_call(const ast::Node& node, argos::ir::FunctionId id,
      const std::vector<ast::ExpressionPtr>& arguments, const std::vector<std::string>& labels,
      ValueType returned, std::optional<Register> receiver = std::nullopt) {
    const auto* declaration = work_[id].function;
    if (!declaration || arguments.size() + (receiver ? 1 : 0) > 6) {
      error(node, "chamada excede limite de seis argumentos incluindo this"); return std::nullopt;
    }
    Instruction call;
    call.opcode = Opcode::call;
    call.function = id;
    call.dst = reg(returned);
    call.arguments.resize(arguments.size() + (receiver ? 1 : 0));
    if (receiver) call.arguments[0] = *receiver;
    // Evaluate in source order before arranging registers by parameter label.
    for (std::size_t i = 0; i < arguments.size(); ++i) {
      const auto value = expression(*arguments[i]);
      if (!value) return std::nullopt;
      std::size_t position = i;
      if (i < labels.size() && !labels[i].empty())
        for (std::size_t j = 0; j < declaration->parameters.size(); ++j)
          if (declaration->parameters[j]->external_label == labels[i]) { position = j; break; }
      if (position + (receiver ? 1 : 0) >= call.arguments.size()) {
        error(node, "rótulo não corresponde ao parâmetro"); return std::nullopt;
      }
      call.arguments[position + (receiver ? 1 : 0)] = *value;
    }
    emit(call);
    return call.dst;
  }
  void error(const ast::Node &node, std::string message) {
    diagnostics_.emit(diagnostics::catalog::compiler::unsupported_lowering,
                      node.span, std::move(message));
  }
  LabelId make_block() {
    const auto id = static_cast<LabelId>(function_.blocks.size());
    function_.blocks.push_back({id, {}});
    return id;
  }
  ValueType syntax_type(const ast::TypeSyntax *type) {
    if (type && type->result_type) return ValueType::closure;
    if (type && layouts_.contains(type->name)) return ValueType::object;
    if (type && type->name == "bool")
      return ValueType::boolean;
    if (type && (type->name == "float" || type->name == "double"))
      return ValueType::float64;
    if (type && type->name == "string")
      return ValueType::string;
    if (type && type->name == "void") return ValueType::void_type;
    return ValueType::int32;
  }
  static std::optional<ValueType> semantic_type(const semantics::Type &type) {
    if (type.nullable_depth != 0) return std::nullopt;
    if (type.kind == semantics::TypeKind::int_type) return ValueType::int32;
    if (type.kind == semantics::TypeKind::bool_type) return ValueType::boolean;
    if (type.kind == semantics::TypeKind::float_type ||
        type.kind == semantics::TypeKind::double_type)
      return ValueType::float64;
    if (type.kind == semantics::TypeKind::function_type) return ValueType::closure;
    if (type.kind == semantics::TypeKind::nominal_type) return ValueType::object;
    if (type.kind == semantics::TypeKind::string_type) return ValueType::string;
    if (type.kind == semantics::TypeKind::void_type) return ValueType::void_type;
    return std::nullopt;
  }
  std::optional<ValueType> expression_type(const ast::Expression &node) {
    const auto type = semantic_type(semantics_.type_of(node));
    if (!type) error(node, "o tipo desta expressão ainda não pode ser representado pela IR executável");
    return type;
  }
  Slot add_slot(ValueType type) {
    const auto result = function_.slot_count++;
    function_.slot_types.push_back(type);
    return result;
  }
  Register reg(ValueType type = ValueType::int32) {
    const auto result = function_.register_count++;
    function_.register_types.push_back(type);
    return result;
  }
  void emit(Instruction instruction) {
    function_.blocks[current_].instructions.push_back(std::move(instruction));
  }
  void jump(LabelId label) {
    Instruction instruction;
    instruction.opcode = Opcode::jmp;
    instruction.label = label;
    emit(std::move(instruction));
  }
  Register constant(std::string value, ValueType type = ValueType::int32) {
    const auto id = static_cast<argos::ir::ConstantId>(module_.constants.size());
    module_.constants.push_back({id, std::move(value)});
    Instruction instruction;
    instruction.opcode = Opcode::load_const;
    instruction.dst = reg(type);
    instruction.constant = id;
    const auto result = instruction.dst;
    emit(std::move(instruction));
    return result;
  }
  std::optional<Slot> slot(const std::string &name) const {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
      const auto found = scope->find(name);
      if (found != scope->end()) return found->second;
    }
    return std::nullopt;
  }

  std::optional<Register> expression(const ast::Expression &node) {
    auto result = expression_impl(node);
    const auto coercion = semantics_.coercion_of(node);
    if (!result || !coercion) return result;
    const auto target = semantic_type(*coercion);
    if (!target) {
      error(node, "a conversão numérica desta expressão ainda não é compilável");
      return std::nullopt;
    }
    if (function_.register_types[*result] == *target) return result;
    if (*target != ValueType::float64 ||
        function_.register_types[*result] != ValueType::int32) {
      error(node, "a conversão numérica desta expressão ainda não é compilável");
      return std::nullopt;
    }
    Instruction conversion;
    conversion.opcode = Opcode::int_to_double;
    conversion.dst = reg(ValueType::float64);
    conversion.src1 = *result;
    const auto converted = conversion.dst;
    emit(std::move(conversion));
    return converted;
  }

  std::optional<Register> expression_impl(const ast::Expression &node) {
    if (const auto* creation = dynamic_cast<const ast::NewExpression*>(&node)) {
      const auto found = layouts_.find(creation->type->name);
      if (found == layouts_.end()) { error(node, "classe ainda não possui layout executável"); return std::nullopt; }
      Instruction allocation;
      allocation.opcode = Opcode::new_object;
      allocation.constant = found->second.id;
      allocation.dst = reg(ValueType::object);
      emit(allocation);
      direct_call(node, ids_.at(creation->type->name + ".construct"), creation->call->arguments,
                  creation->call->argument_labels, ValueType::void_type, allocation.dst);
      return allocation.dst;
    }
    if (const auto* access = dynamic_cast<const ast::MemberAccessExpression*>(&node)) {
      const auto type = semantics_.type_of(*access->base);
      const auto found = layouts_.find(type.nominal_name);
      if (found == layouts_.end()) { error(node, "membro sem layout executável"); return std::nullopt; }
      const auto index = field_index(found->second, access->member);
      const auto getter = ids_.find(type.nominal_name + ".$getter." + access->member);
      if (!index && getter == ids_.end()) {
        error(node, "referência ligada a método ainda não é compilável; use chamada direta"); return std::nullopt;
      }
      const auto base = expression(*access->base);
      if (!base) return std::nullopt;
      return index ? load_field(found->second, *index, *base) : accessor_call(getter->second, *base);
    }
    if (const auto* lambda = dynamic_cast<const ast::LambdaExpression*>(&node)) {
      Instruction instruction;
      instruction.opcode = Opcode::make_closure;
      instruction.function = static_cast<argos::ir::FunctionId>(work_.size());
      work_.push_back({nullptr, lambda, std::nullopt});
      for (const auto& capture : semantics_.captures_of(*lambda)) {
        const auto place = slot(capture.name);
        if (!place) { error(node, "captura deve referir-se a binding local"); return std::nullopt; }
        Instruction load;
        load.opcode = Opcode::load_var;
        load.slot = *place;
        load.dst = reg(function_.slot_types[*place]);
        emit(load);
        instruction.arguments.push_back(load.dst);
      }
      instruction.dst = reg(ValueType::closure);
      const auto result = instruction.dst;
      emit(instruction);
      return result;
    }
    if (const auto *literal =
            dynamic_cast<const ast::StringLiteralExpression *>(&node)) {
      const auto decoded = decode_string_literal(literal->spelling);
      if (!decoded) {
        error(node, "literal de string inválido para a execução");
        return std::nullopt;
      }
      const auto id = static_cast<argos::ir::ConstantId>(module_.constants.size());
      module_.constants.push_back({id, *decoded});
      Instruction instruction;
      instruction.opcode = Opcode::load_string;
      instruction.dst = reg(ValueType::string);
      instruction.constant = id;
      const auto result = instruction.dst;
      emit(std::move(instruction));
      return result;
    }
    if (const auto *literal = dynamic_cast<const ast::IntegerLiteralExpression *>(&node)) {
      std::string digits;
      for (char c : literal->spelling) if (c != '_') digits.push_back(c);
      std::int32_t value{};
      const auto [end, status] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
      if (status != std::errc{} || end != digits.data() + digits.size()) {
        error(node, "somente literais decimais int são compiláveis");
        return std::nullopt;
      }
      return constant(std::to_string(value));
    }
    if (const auto *literal = dynamic_cast<const ast::FloatLiteralExpression *>(&node)) {
      std::string text;
      for (const char character : literal->spelling)
        if (character != '_') text.push_back(character);
      if (!text.empty() && (text.back() == 'f' || text.back() == 'F'))
        text.pop_back();
      double value{};
      const auto [end, status] =
          std::from_chars(text.data(), text.data() + text.size(), value);
      if (status != std::errc{} || end != text.data() + text.size()) {
        error(node, "literal double inválido");
        return std::nullopt;
      }
      return constant(text, ValueType::float64);
    }
    if (const auto *literal = dynamic_cast<const ast::BooleanLiteralExpression *>(&node))
      return constant(literal->value ? "1" : "0", ValueType::boolean);
    if (const auto *group = dynamic_cast<const ast::GroupingExpression *>(&node))
      return expression(*group->expression);
    if (const auto *identifier = dynamic_cast<const ast::IdentifierExpression *>(&node)) {
      const auto place = slot(identifier->name);
      if (!place && owner_) {
        const auto& layout = layouts_.at(owner_->name);
        if (const auto index = field_index(layout, identifier->name)) return load_field(layout, *index, this_value());
        if (const auto getter = ids_.find(owner_->name + ".$getter." + identifier->name); getter != ids_.end())
          return accessor_call(getter->second, this_value());
      }
      if (!place) {
        const auto binding = semantics_.symbol_id_of(node);
        const auto* symbol = binding ? semantics_.symbol_by_id(*binding) : nullptr;
        if (symbol && symbol->kind == semantics::Symbol::Kind::function && !symbol->owner.empty()) {
          error(node, "referência ligada a método ainda não é compilável; use chamada direta");
          return std::nullopt;
        }
      }
      if (!place && ids_.contains(identifier->name)) {
        Instruction instruction;
        instruction.opcode = Opcode::make_closure;
        instruction.function = static_cast<argos::ir::FunctionId>(work_.size());
        work_.push_back({nullptr, nullptr, ids_.at(identifier->name)});
        instruction.dst = reg(ValueType::closure);
        const auto result = instruction.dst;
        emit(instruction);
        return result;
      }
      if (!place) {
        error(node, "somente variáveis locais são compiláveis");
        return std::nullopt;
      }
      Instruction instruction;
      instruction.opcode = Opcode::load_var;
      instruction.slot = *place;
      instruction.dst = reg(function_.slot_types[*place]);
      const auto result = instruction.dst;
      emit(std::move(instruction));
      return result;
    }
    if (const auto *unary = dynamic_cast<const ast::UnaryExpression *>(&node)) {
      const auto operand = expression(*unary->operand);
      if (!operand) return std::nullopt;
      if (unary->op == ast::UnaryOperator::plus) return operand;
      Instruction instruction;
      instruction.opcode = unary->op == ast::UnaryOperator::minus ? Opcode::neg : Opcode::logical_not;
      const auto type = expression_type(node);
      if (!type) return std::nullopt;
      instruction.dst = reg(*type);
      instruction.src1 = *operand;
      const auto result = instruction.dst;
      emit(std::move(instruction));
      return result;
    }
    if (const auto *binary = dynamic_cast<const ast::BinaryExpression *>(&node)) {
      if (binary->op == ast::BinaryOperator::logical_and ||
          binary->op == ast::BinaryOperator::logical_or) {
        const auto left = expression(*binary->left);
        if (!left) return std::nullopt;

        const auto result_slot = add_slot(ValueType::boolean);
        const auto right_label = make_block();
        const bool conjunction =
            binary->op == ast::BinaryOperator::logical_and;
        const auto short_label = conjunction ? make_block() : LabelId{};
        const auto end_label = make_block();
        Instruction branch;
        branch.opcode = Opcode::jmp_if_false;
        branch.src1 = *left;
        branch.label = conjunction ? short_label : right_label;
        emit(std::move(branch));
        if (!conjunction) {
          const auto truth = constant("1", ValueType::boolean);
          Instruction store;
          store.opcode = Opcode::store_var;
          store.slot = result_slot;
          store.src1 = truth;
          emit(std::move(store));
          jump(end_label);
        } else {
          jump(right_label);
          current_ = short_label;
          const auto false_value = constant("0", ValueType::boolean);
          Instruction short_store;
          short_store.opcode = Opcode::store_var;
          short_store.slot = result_slot;
          short_store.src1 = false_value;
          emit(std::move(short_store));
          jump(end_label);
        }

        current_ = right_label;
        const auto right = expression(*binary->right);
        if (!right) return std::nullopt;
        Instruction right_store;
        right_store.opcode = Opcode::store_var;
        right_store.slot = result_slot;
        right_store.src1 = *right;
        emit(std::move(right_store));
        jump(end_label);

        current_ = end_label;
        Instruction load;
        load.opcode = Opcode::load_var;
        load.dst = reg(ValueType::boolean);
        load.slot = result_slot;
        const auto result = load.dst;
        emit(std::move(load));
        return result;
      }
      Opcode opcode;
      switch (binary->op) {
      case ast::BinaryOperator::add: opcode = Opcode::add; break;
      case ast::BinaryOperator::subtract: opcode = Opcode::sub; break;
      case ast::BinaryOperator::multiply: opcode = Opcode::mul; break;
      case ast::BinaryOperator::divide: opcode = Opcode::div; break;
      case ast::BinaryOperator::modulo: opcode = Opcode::mod; break;
      case ast::BinaryOperator::equal: opcode = Opcode::cmp_eq; break;
      case ast::BinaryOperator::not_equal: opcode = Opcode::cmp_ne; break;
      case ast::BinaryOperator::less_than: opcode = Opcode::cmp_lt; break;
      case ast::BinaryOperator::less_than_or_equal: opcode = Opcode::cmp_le; break;
      case ast::BinaryOperator::greater_than: opcode = Opcode::cmp_gt; break;
      case ast::BinaryOperator::greater_than_or_equal: opcode = Opcode::cmp_ge; break;
      default:
        error(node, "operador ainda não é compilável");
        return std::nullopt;
      }
      const auto left = expression(*binary->left);
      const auto right = expression(*binary->right);
      if (!left || !right) return std::nullopt;
      Instruction instruction;
      instruction.opcode = opcode;
      const auto type = expression_type(node);
      if (!type) return std::nullopt;
      instruction.dst = reg(*type);
      instruction.src1 = *left;
      instruction.src2 = *right;
      const auto result = instruction.dst;
      emit(std::move(instruction));
      return result;
    }
    if (const auto *assignment = dynamic_cast<const ast::AssignmentExpression *>(&node)) {
      std::optional<Slot> local;
      const ClassLayout* layout = nullptr;
      std::optional<std::uint32_t> index;
      std::optional<Register> receiver;
      std::string member;
      std::string class_name;
      if (const auto* access = dynamic_cast<const ast::MemberAccessExpression*>(assignment->target.get())) {
        class_name = semantics_.type_of(*access->base).nominal_name;
        const auto found = layouts_.find(class_name);
        if (found != layouts_.end()) {
          layout = &found->second;
          member = access->member;
          receiver = expression(*access->base);
        }
      } else if (const auto* identifier = dynamic_cast<const ast::IdentifierExpression*>(assignment->target.get())) {
        local = slot(identifier->name);
        if (!local && owner_) {
          class_name = owner_->name;
          layout = &layouts_.at(class_name);
          member = identifier->name;
          receiver = this_value();
        }
      }
      if (layout) index = field_index(*layout, member);
      const auto getter = ids_.find(class_name + ".$getter." + member);
      const auto setter = ids_.find(class_name + ".$setter." + member);
      if (!local && (!receiver || !layout || (!index && setter == ids_.end()))) {
        error(node, "atribuição exige variável, campo mutável ou propriedade com setter"); return std::nullopt;
      }
      Opcode operation = Opcode::nop;
      switch (assignment->op) {
      case ast::AssignmentOperator::assign: break;
      case ast::AssignmentOperator::add_assign: operation = Opcode::add; break;
      case ast::AssignmentOperator::subtract_assign: operation = Opcode::sub; break;
      case ast::AssignmentOperator::multiply_assign: operation = Opcode::mul; break;
      case ast::AssignmentOperator::divide_assign: operation = Opcode::div; break;
      case ast::AssignmentOperator::modulo_assign: operation = Opcode::mod; break;
      default: error(node, "operador de atribuição ainda não é compilável"); return std::nullopt;
      }
      std::optional<Register> previous;
      if (operation != Opcode::nop) {
        if (local) {
          Instruction load;
          load.opcode = Opcode::load_var;
          load.slot = *local;
          load.dst = reg(function_.slot_types[*local]);
          emit(load);
          previous = load.dst;
        } else if (index) previous = load_field(*layout, *index, *receiver);
        else if (getter != ids_.end()) previous = accessor_call(getter->second, *receiver);
        else { error(node, "atribuição composta exige getter"); return std::nullopt; }
      }
      auto value = expression(*assignment->value);
      if (!value) return std::nullopt;
      if (previous) {
        Instruction arithmetic;
        arithmetic.opcode = operation;
        arithmetic.src1 = *previous;
        arithmetic.src2 = *value;
        arithmetic.dst = reg(function_.register_types[*previous]);
        emit(arithmetic);
        value = arithmetic.dst;
      }
      if (local) {
        Instruction store;
        store.opcode = Opcode::store_var;
        store.slot = *local;
        store.src1 = *value;
        emit(store);
      } else if (index) store_field(*layout, *index, *receiver, *value);
      else accessor_call(setter->second, *receiver, *value);
      return value;
    }
    if (const auto *call = dynamic_cast<const ast::CallExpression *>(&node)) {
      const auto *callee = dynamic_cast<const ast::IdentifierExpression *>(call->callee.get());
      if (callee && callee->name == "print") {
        for (const auto &argument : call->arguments) {
          Instruction instruction;
          const auto value = expression(*argument);
          if (!value) return std::nullopt;
          const auto type = function_.register_types[*value];
          if (type == ValueType::closure || type == ValueType::object || type == ValueType::void_type) {
            error(*argument, "print exige int, bool, float, double ou string"); return std::nullopt;
          }
          instruction.opcode =
              type == ValueType::boolean
                  ? Opcode::print_bool
                  : type == ValueType::float64
                        ? Opcode::print_double
                        : type == ValueType::string ? Opcode::print_string_value
                                                    : Opcode::print_int;
          instruction.src1 = *value;
          emit(std::move(instruction));
        }
        Instruction newline;
        newline.opcode = Opcode::print_newline;
        emit(std::move(newline));
        return constant("0");
      }
      if (const auto* access = dynamic_cast<const ast::MemberAccessExpression*>(call->callee.get())) {
        const auto base_type = semantics_.type_of(*access->base);
        const auto name = base_type.nominal_name + "." + access->member;
        if (ids_.contains(name)) {
          const auto receiver = expression(*access->base);
          const auto returned = expression_type(node);
          if (!receiver || !returned) return std::nullopt;
          return direct_call(node, ids_.at(name), call->arguments, call->argument_labels, *returned, receiver);
        }
      }
      if (callee && owner_ && !slot(callee->name)) {
        const auto name = owner_->name + "." + callee->name;
        if (ids_.contains(name)) {
          const auto returned = expression_type(node);
          if (!returned) return std::nullopt;
          return direct_call(node, ids_.at(name), call->arguments, call->argument_labels, *returned, this_value());
        }
      }
      const auto callee_type = semantics_.type_of(*call->callee);
      const auto binding = semantics_.symbol_id_of(*call->callee);
      const auto* bound = binding ? semantics_.symbol_by_id(*binding) : nullptr;
      if (!callee || slot(callee->name) || !ids_.contains(callee->name) ||
          (bound && bound->kind != semantics::Symbol::Kind::function)) {
        if (callee_type.kind != semantics::TypeKind::function_type || callee_type.signature.empty() || call->arguments.size() > 5) {
          error(node, "chamada indireta exige função com até cinco argumentos"); return std::nullopt;
        }
        const auto closure = expression(*call->callee);
        if (!closure) return std::nullopt;
        Instruction instruction;
        instruction.opcode = Opcode::call_closure;
        instruction.src1 = *closure;
        const auto result_type = expression_type(node);
        if (!result_type) return std::nullopt;
        instruction.return_type = *result_type;
        instruction.dst = reg(*result_type);
        for (const auto& argument : call->arguments) {
          const auto value = expression(*argument);
          if (!value) return std::nullopt;
          instruction.arguments.push_back(*value);
          instruction.parameter_types.push_back(function_.register_types[*value]);
        }
        const auto result = instruction.dst;
        emit(instruction);
        return result;
      }
      if (!callee || !ids_.contains(callee->name) || call->arguments.size() > 6 ||
          call->arguments.size() != arities_.at(callee->name)) {
        error(node, "somente chamadas locais com até seis argumentos são compiláveis");
        return std::nullopt;
      }
      const auto returned = expression_type(node);
      if (!returned) return std::nullopt;
      return direct_call(node, ids_.at(callee->name), call->arguments, call->argument_labels, *returned);
    }
    error(node, "expressão ainda não é compilável");
    return std::nullopt;
  }

  bool block(const ast::BlockStatement &node) {
    scopes_.emplace_back();
    bool terminated = false;
    for (const auto &item : node.items) if (!terminated) terminated = item_lower(*item);
    scopes_.pop_back();
    return terminated;
  }

  bool item_lower(const ast::BlockItem &node) {
    if (const auto *variable = dynamic_cast<const ast::VariableDeclaration *>(&node)) {
      if (variable->type && !supported_type(variable->type.get())) {
        error(node, "somente variáveis int, bool, float ou double são compiláveis");
        return false;
      }
      const auto type = variable->type
                            ? std::optional<ValueType>{syntax_type(variable->type.get())}
                            : (variable->initializer
                                   ? expression_type(*variable->initializer)
                                   : std::nullopt);
      if (!type) {
        error(node, "a variável precisa ter tipo int, bool, float ou double para ser compilada");
        return false;
      }
      const auto place = add_slot(*type);
      if (variable->initializer) {
        const auto value = expression(*variable->initializer);
        if (value) {
          Instruction instruction;
          instruction.opcode = Opcode::store_var;
          instruction.slot = place;
          instruction.src1 = *value;
          emit(std::move(instruction));
        }
      }
      scopes_.back().insert_or_assign(variable->name, place);
      return false;
    }
    if (const auto *nested = dynamic_cast<const ast::BlockStatement *>(&node)) return block(*nested);
    if (const auto *statement = dynamic_cast<const ast::ExpressionStatement *>(&node)) {
      expression(*statement->expression);
      return false;
    }
    if (const auto *statement = dynamic_cast<const ast::ReturnStatement *>(&node)) {
      if (!statement->expression) {
        if (function_.return_type != ValueType::void_type) {
          error(node, "retorno sem valor só é permitido em funções void");
          return false;
        }
        Instruction instruction;
        instruction.opcode = Opcode::ret;
        instruction.src1 = constant("0");
        emit(std::move(instruction));
        return true;
      }
      const auto value = expression(*statement->expression);
      if (!value) return false;
      if (function_.register_types[*value] != function_.return_type) {
        error(node, "o tipo do valor retornado não corresponde ao tipo declarado pela função");
        return false;
      }
      Instruction instruction;
      instruction.opcode = Opcode::ret;
      instruction.src1 = *value;
      emit(std::move(instruction));
      return true;
    }
    if (const auto *statement = dynamic_cast<const ast::IfStatement *>(&node)) {
      const auto condition = expression(*statement->condition);
      if (!condition) return false;
      const auto then_label = make_block();
      const auto else_label = make_block();
      const auto end_label = make_block();
      Instruction branch;
      branch.opcode = Opcode::jmp_if_false;
      branch.src1 = *condition;
      branch.label = else_label;
      emit(std::move(branch));
      jump(then_label);
      current_ = then_label;
      const bool then_returns = block(*statement->then_branch);
      if (!then_returns) jump(end_label);
      current_ = else_label;
      bool else_returns = false;
      if (statement->else_branch) else_returns = item_lower(*statement->else_branch);
      if (!else_returns) jump(end_label);
      current_ = end_label;
      return then_returns && statement->else_branch && else_returns;
    }
    if (const auto *statement = dynamic_cast<const ast::WhileStatement *>(&node)) {
      const auto test_label = make_block();
      const auto body_label = make_block();
      const auto end_label = make_block();
      jump(test_label);
      current_ = test_label;
      const auto condition = expression(*statement->condition);
      if (condition) {
        Instruction branch;
        branch.opcode = Opcode::jmp_if_false;
        branch.src1 = *condition;
        branch.label = end_label;
        emit(std::move(branch));
        jump(body_label);
      }
      current_ = body_label;
      break_targets_.push_back(end_label);
      continue_targets_.push_back(test_label);
      if (!block(*statement->body)) jump(test_label);
      continue_targets_.pop_back();
      break_targets_.pop_back();
      current_ = end_label;
      return false;
    }
    if (dynamic_cast<const ast::BreakStatement *>(&node)) {
      if (break_targets_.empty()) { error(node, "'break' fora de laço"); return false; }
      jump(break_targets_.back());
      return true;
    }
    if (dynamic_cast<const ast::ContinueStatement *>(&node)) {
      if (continue_targets_.empty()) { error(node, "'continue' fora de laço"); return false; }
      jump(continue_targets_.back());
      return true;
    }
    error(node, "instrução ou declaração ainda não é compilável");
    return false;
  }

  argos::ir::Module &module_;
  diagnostics::DiagnosticEngine &diagnostics_;
  const semantics::SemanticAnalyzer &semantics_;
  const ast::Node &source_;
  const std::vector<ast::ParameterPtr>& parameters_;
  const ast::TypeSyntax* return_type_;
  const ast::BlockStatement* body_;
  const ast::LambdaExpression* lambda_{nullptr};
  const std::unordered_map<std::string, argos::ir::FunctionId> &ids_;
  const std::unordered_map<std::string, std::size_t> &arities_;
  std::vector<WorkItem>& work_;
  const ClassLayouts& layouts_;
  const ast::ClassDeclaration* owner_{nullptr};
  argos::ir::Function function_;
  LabelId current_{};
  std::vector<std::unordered_map<std::string, Slot>> scopes_;
  std::vector<LabelId> break_targets_;
  std::vector<LabelId> continue_targets_;
};

} // namespace

argos::ir::Module IrGenerator::generate(const ast::Program &program) {
  argos::ir::Module module;
  std::vector<const ast::FunctionDeclaration *> functions;
  ClassLayouts layouts;
  std::vector<std::unique_ptr<ast::FunctionDeclaration>> implicit_constructors;
  std::unordered_map<const ast::FunctionDeclaration*, const ast::ClassDeclaration*> owners;
  for (const auto& item : program.items) {
    const auto* declaration = dynamic_cast<const ast::ClassDeclaration*>(item.get());
    if (!declaration) continue;
    if (declaration->is_interface) continue;
    if (!declaration->bases.empty() || declaration->modifiers.abstract_) {
      diagnostics_.emit(diagnostics::catalog::compiler::unsupported_lowering, declaration->span,
                        "herança e despacho polimórfico possuem análise estática, mas ainda não têm lowering executável");
      continue;
    }
    ClassLayout layout{declaration, static_cast<std::uint32_t>(module.objects.size()), {}};
    argos::ir::ObjectLayout object;
    object.name = declaration->name;
    for (const auto& member : declaration->members)
      if (const auto* field = dynamic_cast<const ast::VariableDeclaration*>(member.get()); field && !field->is_property)
        layout.fields.push_back(field);
    layouts.emplace(declaration->name, layout);
    module.objects.push_back(object);
  }
  for (const auto& [name, layout] : layouts) {
    for (const auto* field : layout.fields) {
      const auto* type = field->type.get();
      ValueType value = ValueType::int32;
      if (type && type->nullable_depth == 0 && type->name == "bool") value = ValueType::boolean;
      else if (type && type->nullable_depth == 0 && (type->name == "double" || type->name == "float")) value = ValueType::float64;
      else if (type && type->nullable_depth == 0 && type->name == "string") value = ValueType::string;
      else if (type && type->nullable_depth == 0 && type->result_type) value = ValueType::closure;
      else if (type && type->nullable_depth == 0 && layouts.contains(type->name)) value = ValueType::object;
      else if (!type || type->nullable_depth || type->name != "int")
        diagnostics_.emit(diagnostics::catalog::compiler::unsupported_lowering, field->span, "tipo de campo ainda não é executável");
      module.objects[layout.id].fields.push_back(value);
    }
  }
  std::unordered_map<std::string, argos::ir::FunctionId> ids;
  std::unordered_map<std::string, std::size_t> arities;
  for (const auto &item : program.items) {
    const auto *function = dynamic_cast<const ast::FunctionDeclaration *>(item.get());
    if (const auto* declaration = dynamic_cast<const ast::ClassDeclaration*>(item.get())) {
      if (declaration->is_interface || !layouts.contains(declaration->name)) continue;
      bool has_constructor = false;
      for (const auto& member : declaration->members) {
        if (const auto* property = dynamic_cast<const ast::VariableDeclaration*>(member.get());
            property && property->is_property) {
          for (const auto* accessor : {property->getter.get(), property->setter.get()}) {
            if (!accessor) continue;
            ids.emplace(declaration->name + "." + accessor->name, static_cast<argos::ir::FunctionId>(functions.size()));
            arities.emplace(declaration->name + "." + accessor->name, accessor->parameters.size());
            functions.push_back(accessor);
            owners.emplace(accessor, declaration);
          }
          continue;
        }
        const auto* method = dynamic_cast<const ast::FunctionDeclaration*>(member.get());
        if (!method) continue;
        has_constructor = has_constructor || method->name == "construct";
        ids.emplace(declaration->name + "." + method->name, static_cast<argos::ir::FunctionId>(functions.size()));
        arities.emplace(declaration->name + "." + method->name, method->parameters.size());
        functions.push_back(method);
        owners.emplace(method, declaration);
      }
      if (!has_constructor) {
        auto constructor = std::make_unique<ast::FunctionDeclaration>("construct", std::vector<ast::ParameterPtr>{},
            nullptr, std::make_unique<ast::BlockStatement>(std::vector<ast::BlockItemPtr>{}, declaration->span), declaration->span);
        ids.emplace(declaration->name + ".construct", static_cast<argos::ir::FunctionId>(functions.size()));
        arities.emplace(declaration->name + ".construct", 0);
        functions.push_back(constructor.get());
        owners.emplace(constructor.get(), declaration);
        implicit_constructors.push_back(std::move(constructor));
      }
      continue;
    }
    if (!function) {
      diagnostics_.emit(diagnostics::catalog::compiler::unsupported_lowering,
                        item->span, "somente funções são compiláveis no nível superior");
      continue;
    }
    ids.emplace(function->name, static_cast<argos::ir::FunctionId>(functions.size()));
    arities.emplace(function->name, function->parameters.size());
    functions.push_back(function);
  }
  if (!ids.contains("main")) {
    diagnostics_.emit(diagnostics::catalog::compiler::invalid_entry_point,
                      program.span, "executável requer function main() -> int ou void");
  } else {
    const auto *entry = functions[ids.at("main")];
    if (!entry->parameters.empty() || !entry->return_type ||
        (entry->return_type->name != "int" &&
         entry->return_type->name != "void") ||
        entry->return_type->nullable_depth != 0)
      diagnostics_.emit(diagnostics::catalog::compiler::invalid_entry_point,
                        entry->span, "entrada deve ser function main() -> int ou void");
  }
  if (diagnostics_.has_errors()) return module;
  std::vector<WorkItem> work;
  for (const auto* function : functions)
    work.push_back({function, nullptr, std::nullopt, owners.contains(function) ? owners.at(function) : nullptr});
  for (std::size_t id = 0; id < work.size(); ++id) {
    const auto item = work[id];
    if (item.function) {
      Lowerer lowerer{module, diagnostics_, semantics_, *item.function, ids, arities,
                     static_cast<argos::ir::FunctionId>(id), work, layouts, item.owner};
      module.functions.push_back(lowerer.lower());
    } else if (item.lambda) {
      Lowerer lowerer{module, diagnostics_, semantics_, *item.lambda, ids, arities,
                     static_cast<argos::ir::FunctionId>(id), work, layouts};
      module.functions.push_back(lowerer.lower());
    } else {
      const auto target = module.functions[*item.adapter];
      argos::ir::Function adapter;
      adapter.id = static_cast<argos::ir::FunctionId>(id);
      adapter.name = "<adapter_" + std::to_string(id) + ">";
      adapter.return_type = target.return_type;
      adapter.parameter_types = {ValueType::closure};
      adapter.parameter_types.insert(adapter.parameter_types.end(), target.parameter_types.begin(), target.parameter_types.end());
      adapter.parameter_count = static_cast<std::uint32_t>(adapter.parameter_types.size());
      if (adapter.parameter_count > 6)
        diagnostics_.emit(diagnostics::catalog::compiler::unsupported_lowering, program.span,
                          "função usada como valor suporta até cinco parâmetros");
      adapter.slot_types = adapter.parameter_types;
      adapter.slot_count = adapter.parameter_count;
      argos::ir::BasicBlock block{0, {}};
      Instruction call;
      call.opcode = Opcode::call;
      call.function = *item.adapter;
      for (std::size_t i = 0; i < target.parameter_types.size(); ++i) {
        Instruction load;
        load.opcode = Opcode::load_var;
        load.slot = static_cast<Slot>(i + 1);
        load.dst = adapter.register_count++;
        adapter.register_types.push_back(target.parameter_types[i]);
        block.instructions.push_back(load);
        call.arguments.push_back(load.dst);
      }
      call.dst = adapter.register_count++;
      adapter.register_types.push_back(target.return_type);
      block.instructions.push_back(call);
      Instruction ret;
      ret.opcode = Opcode::ret;
      ret.src1 = call.dst;
      block.instructions.push_back(ret);
      adapter.blocks.push_back(block);
      module.functions.push_back(adapter);
    }
  }
  return module;
}

} // namespace argos::codegen::ir
