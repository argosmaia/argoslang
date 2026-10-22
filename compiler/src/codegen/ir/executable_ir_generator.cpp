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
  return type && type->nullable_depth == 0 &&
         (type->name == "int" || type->name == "bool" ||
          type->name == "float" || type->name == "double" ||
          type->name == "Int" || type->name == "Bool" ||
          type->name == "Float" || type->name == "Double");
}

bool supported_return_type(const ast::TypeSyntax *type) {
  return supported_type(type) ||
         (type && type->nullable_depth == 0 && type->name == "void");
}

class Lowerer final {
public:
  Lowerer(argos::ir::Module &module, diagnostics::DiagnosticEngine &diagnostics,
          const semantics::SemanticAnalyzer &semantics,
          const ast::FunctionDeclaration &source,
          const std::unordered_map<std::string, argos::ir::FunctionId> &ids,
          const std::unordered_map<std::string, std::size_t> &arities,
          argos::ir::FunctionId id)
      : module_(module), diagnostics_(diagnostics), semantics_(semantics),
        source_(source), ids_(ids), arities_(arities) {
    function_.id = id;
    function_.name = source.name;
    scopes_.emplace_back();
    current_ = make_block();
  }

  argos::ir::Function lower() {
    if (source_.parameters.size() > 6 || !supported_return_type(source_.return_type.get())) {
      error(source_, "somente funções com até seis parâmetros int/bool/float/double e retorno int/bool/float/double/void são compiláveis");
    } else {
      function_.return_type = syntax_type(source_.return_type.get());
      function_.parameter_count = static_cast<std::uint32_t>(source_.parameters.size());
      for (const auto& parameter : source_.parameters) {
        if (!supported_type(parameter->type.get())) error(*parameter, "parâmetro deve ser int, bool, float ou double");
        const auto type = syntax_type(parameter->type.get());
        function_.parameter_types.push_back(type);
        scopes_.back().insert_or_assign(parameter->name, add_slot(type));
      }
      block(*source_.body);
    }
    return std::move(function_);
  }

private:
  void error(const ast::Node &node, std::string message) {
    diagnostics_.emit(diagnostics::catalog::compiler::unsupported_lowering,
                      node.span, std::move(message));
  }
  LabelId make_block() {
    const auto id = static_cast<LabelId>(function_.blocks.size());
    function_.blocks.push_back({id, {}});
    return id;
  }
  static ValueType syntax_type(const ast::TypeSyntax *type) {
    if (type && (type->name == "bool" || type->name == "Bool"))
      return ValueType::boolean;
    if (type && (type->name == "float" || type->name == "Float" ||
                 type->name == "double" || type->name == "Double"))
      return ValueType::float64;
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
    if (const auto *literal = dynamic_cast<const ast::IntegerLiteralExpression *>(&node)) {
      std::string digits;
      for (char c : literal->spelling) if (c != '_') digits.push_back(c);
      std::int32_t value{};
      const auto [end, status] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
      if (status != std::errc{} || end != digits.data() + digits.size()) {
        error(node, "somente literais decimais Int são compiláveis");
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
      const auto *target = dynamic_cast<const ast::IdentifierExpression *>(assignment->target.get());
      if (!target || assignment->op != ast::AssignmentOperator::assign) {
        error(node, "somente atribuição simples a variável local é compilável");
        return std::nullopt;
      }
      const auto place = slot(target->name);
      const auto value = expression(*assignment->value);
      if (!place || !value) return std::nullopt;
      Instruction instruction;
      instruction.opcode = Opcode::store_var;
      instruction.slot = *place;
      instruction.src1 = *value;
      emit(std::move(instruction));
      return value;
    }
    if (const auto *call = dynamic_cast<const ast::CallExpression *>(&node)) {
      const auto *callee = dynamic_cast<const ast::IdentifierExpression *>(call->callee.get());
      if (callee && callee->name == "print") {
        for (const auto &argument : call->arguments) {
          Instruction instruction;
          if (const auto *string = dynamic_cast<const ast::StringLiteralExpression *>(argument.get())) {
            const auto id = static_cast<argos::ir::ConstantId>(module_.constants.size());
            module_.constants.push_back({id, string->spelling});
            instruction.opcode = Opcode::print_string;
            instruction.constant = id;
          } else {
            const auto value = expression(*argument);
            if (!value) return std::nullopt;
            const auto type = function_.register_types[*value];
            instruction.opcode =
                type == ValueType::boolean
                    ? Opcode::print_bool
                    : type == ValueType::float64 ? Opcode::print_double
                                                 : Opcode::print_int;
            instruction.src1 = *value;
          }
          emit(std::move(instruction));
        }
        Instruction newline;
        newline.opcode = Opcode::print_newline;
        emit(std::move(newline));
        return constant("0");
      }
      if (!callee || !ids_.contains(callee->name) || call->arguments.size() > 6 ||
          call->arguments.size() != arities_.at(callee->name)) {
        error(node, "somente chamadas locais com até seis argumentos são compiláveis");
        return std::nullopt;
      }
      Instruction instruction;
      instruction.opcode = Opcode::call;
      const auto type = expression_type(node);
      if (!type) return std::nullopt;
      instruction.dst = reg(*type);
      instruction.function = ids_.at(callee->name);
      for (std::size_t index = 0; index < call->arguments.size(); ++index) {
        if (!call->argument_labels[index].empty()) { error(node, "argumentos nomeados ainda não são compiláveis"); return std::nullopt; }
        const auto value = expression(*call->arguments[index]);
        if (!value) return std::nullopt;
        instruction.arguments.push_back(*value);
      }
      const auto result = instruction.dst;
      emit(std::move(instruction));
      return result;
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
      scopes_.back().insert_or_assign(variable->name, place);
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
      return false;
    }
    if (const auto *nested = dynamic_cast<const ast::BlockStatement *>(&node)) return block(*nested);
    if (const auto *statement = dynamic_cast<const ast::ExpressionStatement *>(&node)) {
      expression(*statement->expression);
      return false;
    }
    if (const auto *statement = dynamic_cast<const ast::ReturnStatement *>(&node)) {
      if (!statement->expression) {
        if (source_.return_type->name != "void") {
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
  const ast::FunctionDeclaration &source_;
  const std::unordered_map<std::string, argos::ir::FunctionId> &ids_;
  const std::unordered_map<std::string, std::size_t> &arities_;
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
  std::unordered_map<std::string, argos::ir::FunctionId> ids;
  std::unordered_map<std::string, std::size_t> arities;
  for (const auto &item : program.items) {
    const auto *function = dynamic_cast<const ast::FunctionDeclaration *>(item.get());
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
        (entry->return_type->name != "int" && entry->return_type->name != "Int" &&
         entry->return_type->name != "void") ||
        entry->return_type->nullable_depth != 0)
      diagnostics_.emit(diagnostics::catalog::compiler::invalid_entry_point,
                        entry->span, "entrada deve ser function main() -> int ou void");
  }
  if (diagnostics_.has_errors()) return module;
  for (std::size_t id = 0; id < functions.size(); ++id) {
    Lowerer lowerer{module, diagnostics_, semantics_, *functions[id], ids, arities,
                    static_cast<argos::ir::FunctionId>(id)};
    module.functions.push_back(lowerer.lower());
  }
  return module;
}

} // namespace argos::codegen::ir
