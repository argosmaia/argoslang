#include "argos/codegen/ir/executable_ir_generator.hpp"

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

bool supported_type(const ast::TypeSyntax *type) {
  return type && type->nullable_depth == 0 &&
         (type->name == "int" || type->name == "bool" ||
          type->name == "Int" || type->name == "Bool");
}

bool supported_return_type(const ast::TypeSyntax *type) {
  return supported_type(type) ||
         (type && type->nullable_depth == 0 && type->name == "void");
}

bool is_boolean_expression(const ast::Expression &expression) {
  if (dynamic_cast<const ast::BooleanLiteralExpression *>(&expression)) return true;
  if (const auto *group = dynamic_cast<const ast::GroupingExpression *>(&expression))
    return is_boolean_expression(*group->expression);
  if (const auto *unary = dynamic_cast<const ast::UnaryExpression *>(&expression))
    return unary->op == ast::UnaryOperator::not_;
  if (const auto *binary = dynamic_cast<const ast::BinaryExpression *>(&expression)) {
    using Operator = ast::BinaryOperator;
    return binary->op == Operator::equal || binary->op == Operator::not_equal ||
           binary->op == Operator::less_than || binary->op == Operator::less_than_or_equal ||
           binary->op == Operator::greater_than || binary->op == Operator::greater_than_or_equal;
  }
  return false;
}

class Lowerer final {
public:
  Lowerer(argos::ir::Module &module, diagnostics::DiagnosticEngine &diagnostics,
          const ast::FunctionDeclaration &source,
          const std::unordered_map<std::string, argos::ir::FunctionId> &ids,
          const std::unordered_map<std::string, std::size_t> &arities,
          argos::ir::FunctionId id)
      : module_(module), diagnostics_(diagnostics), source_(source), ids_(ids), arities_(arities) {
    function_.id = id;
    function_.name = source.name;
    scopes_.emplace_back();
    current_ = make_block();
  }

  argos::ir::Function lower() {
    if (source_.parameters.size() > 6 || !supported_return_type(source_.return_type.get())) {
      error(source_, "somente funções com até seis parâmetros int/bool e retorno int/bool/void são compiláveis");
    } else {
      function_.parameter_count = static_cast<std::uint32_t>(source_.parameters.size());
      for (const auto& parameter : source_.parameters) {
        if (!supported_type(parameter->type.get())) error(*parameter, "parâmetro deve ser int ou bool");
        scopes_.back().insert_or_assign(parameter->name, function_.slot_count++);
      }
      block(*source_.body);
    }
    return std::move(function_);
  }

private:
  void error(const ast::Node &node, std::string message) {
    diagnostics_.error("ARGOS-C0001", std::move(message), node.span);
  }
  LabelId make_block() {
    const auto id = static_cast<LabelId>(function_.blocks.size());
    function_.blocks.push_back({id, {}});
    return id;
  }
  Register reg() { return function_.register_count++; }
  void emit(Instruction instruction) {
    function_.blocks[current_].instructions.push_back(std::move(instruction));
  }
  void jump(LabelId label) {
    Instruction instruction;
    instruction.opcode = Opcode::jmp;
    instruction.label = label;
    emit(std::move(instruction));
  }
  Register constant(std::string value) {
    const auto id = static_cast<argos::ir::ConstantId>(module_.constants.size());
    module_.constants.push_back({id, std::move(value)});
    Instruction instruction;
    instruction.opcode = Opcode::load_const;
    instruction.dst = reg();
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
    if (const auto *literal = dynamic_cast<const ast::BooleanLiteralExpression *>(&node))
      return constant(literal->value ? "1" : "0");
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
      instruction.dst = reg();
      instruction.slot = *place;
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
      instruction.dst = reg();
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
      instruction.dst = reg();
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
            instruction.opcode = is_boolean_expression(*argument)
                ? Opcode::print_bool : Opcode::print_int;
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
      instruction.dst = reg();
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
        error(node, "somente variáveis Int ou Bool são compiláveis");
        return false;
      }
      const auto place = function_.slot_count++;
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

argos::ir::Module ExecutableIrGenerator::generate(const ast::Program &program) {
  argos::ir::Module module;
  std::vector<const ast::FunctionDeclaration *> functions;
  std::unordered_map<std::string, argos::ir::FunctionId> ids;
  std::unordered_map<std::string, std::size_t> arities;
  for (const auto &item : program.items) {
    const auto *function = dynamic_cast<const ast::FunctionDeclaration *>(item.get());
    if (!function) {
      diagnostics_.error("ARGOS-C0001", "somente funções são compiláveis no nível superior", item->span);
      continue;
    }
    ids.emplace(function->name, static_cast<argos::ir::FunctionId>(functions.size()));
    arities.emplace(function->name, function->parameters.size());
    functions.push_back(function);
  }
  if (!ids.contains("main")) {
    diagnostics_.error("ARGOS-C0002", "executável requer function main() -> int ou void", program.span);
  } else {
    const auto *entry = functions[ids.at("main")];
    if (!entry->parameters.empty() || !entry->return_type ||
        (entry->return_type->name != "int" && entry->return_type->name != "Int" &&
         entry->return_type->name != "void") ||
        entry->return_type->nullable_depth != 0)
      diagnostics_.error("ARGOS-C0002", "entrada deve ser function main() -> int ou void", entry->span);
  }
  if (diagnostics_.has_errors()) return module;
  for (std::size_t id = 0; id < functions.size(); ++id) {
    Lowerer lowerer{module, diagnostics_, *functions[id], ids, arities,
                    static_cast<argos::ir::FunctionId>(id)};
    module.functions.push_back(lowerer.lower());
  }
  return module;
}

} // namespace argos::codegen::ir
