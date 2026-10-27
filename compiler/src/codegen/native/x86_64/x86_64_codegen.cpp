#include "argos/codegen/native/x86_64/x86_64_codegen.hpp"
#include "argos/ir/validator.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <set>

namespace argos::codegen::native::x86_64 {

namespace {
constexpr std::string_view division_error_message =
    "argosc: erro de execução: módulo por zero ou resultado fora do intervalo\n";

std::string label(argos::ir::FunctionId function, argos::ir::LabelId block) {
  return ".Largos_" + std::to_string(function) + "_" + std::to_string(block);
}

std::string slot_address(argos::ir::Slot slot) {
  return "-" + std::to_string((static_cast<std::uint64_t>(slot) + 1) * 8) + "(%rbp)";
}

std::string register_address(const argos::ir::Function &function,
                             argos::ir::Register reg) {
  return slot_address(function.slot_count + reg);
}

std::string symbol(const argos::ir::Function &function) {
  return function.name == "main" ? "main"
                                 : ".Largos_function_" + std::to_string(function.id);
}

bool pointer_type(argos::ir::ValueType type) {
  return type == argos::ir::ValueType::string || type == argos::ir::ValueType::closure || type == argos::ir::ValueType::object;
}

std::uint64_t closure_signature(const std::vector<argos::ir::ValueType>& parameters,
                                argos::ir::ValueType returned, bool environment) {
  std::uint64_t value = static_cast<std::uint64_t>(returned) + 1;
  unsigned shift = 4;
  for (std::size_t i = environment ? 1 : 0; i < parameters.size(); ++i, shift += 4)
    value |= (static_cast<std::uint64_t>(parameters[i]) + 1) << shift;
  return value;
}

void validate(const argos::ir::Module &module, const argos::ir::Function &function,
              const argos::ir::Instruction &instruction) {
  using argos::ir::Opcode;
  const auto valid_reg = [&](argos::ir::Register reg) {
    if (reg >= function.register_count) throw std::runtime_error("IR: registrador inválido");
  };
  const auto valid_slot = [&] {
    if (instruction.slot >= function.slot_count) throw std::runtime_error("IR: slot inválido");
  };
  switch (instruction.opcode) {
  case Opcode::new_object:
  case Opcode::load_field:
  case Opcode::store_field:
  case Opcode::make_closure:
  case Opcode::load_capture:
  case Opcode::call_closure:
    // Full structural validation is performed by validate_module.
    break;
  case Opcode::load_const:
    valid_reg(instruction.dst);
    if (instruction.constant >= module.constants.size()) throw std::runtime_error("IR: constante inválida");
    break;
  case Opcode::load_string:
    valid_reg(instruction.dst);
    if (instruction.constant >= module.constants.size()) throw std::runtime_error("IR: constante inválida");
    break;
  case Opcode::load_var: valid_reg(instruction.dst); valid_slot(); break;
  case Opcode::store_var: valid_reg(instruction.src1); valid_slot(); break;
  case Opcode::neg:
  case Opcode::logical_not:
  case Opcode::int_to_double:
    valid_reg(instruction.dst); valid_reg(instruction.src1); break;
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
    valid_reg(instruction.dst); valid_reg(instruction.src1); valid_reg(instruction.src2); break;
  case Opcode::jmp:
    if (std::none_of(function.blocks.begin(), function.blocks.end(),
                     [&](const auto &block) { return block.label == instruction.label; })) throw std::runtime_error("IR: salto inválido");
    break;
  case Opcode::jmp_if_false:
    valid_reg(instruction.src1);
    if (std::none_of(function.blocks.begin(), function.blocks.end(),
                     [&](const auto &block) { return block.label == instruction.label; })) throw std::runtime_error("IR: salto inválido");
    break;
  case Opcode::call:
    valid_reg(instruction.dst);
    if (instruction.function >= module.functions.size()) throw std::runtime_error("IR: função inválida");
    if (instruction.arguments.size() > 6) throw std::runtime_error("IR: argumentos demais");
    for (const auto reg : instruction.arguments) valid_reg(reg);
    break;
  case Opcode::print_int: valid_reg(instruction.src1); break;
  case Opcode::print_bool:
  case Opcode::print_double: valid_reg(instruction.src1); break;
  case Opcode::print_string:
    if (instruction.constant >= module.constants.size()) throw std::runtime_error("IR: constante inválida");
    break;
  case Opcode::print_string_value: valid_reg(instruction.src1); break;
  case Opcode::print_newline: break;
  case Opcode::ret: valid_reg(instruction.src1); break;
  default: throw std::runtime_error("IR: opcode não suportado pelo backend x86-64");
  }
}
} // namespace

std::string CodeGenerator::generate(const argos::ir::Module &m) {
  if (const auto error = argos::ir::validate_module(m))
    throw std::runtime_error("IR inválida: " + *error);
  for (const auto &function : m.functions)
    if (function.parameter_count > 6)
      throw std::runtime_error("IR: backend x86-64 suporta até seis parâmetros");
  std::ostringstream o;
  std::set<argos::ir::ConstantId> string_constants;
  std::set<argos::ir::ConstantId> literal_string_constants;
  std::set<argos::ir::ConstantId> double_constants;
  bool prints_integer = false;
  bool prints_boolean = false;
  bool prints_double = false;
  bool uses_modulo = false;
  bool uses_closures = !m.objects.empty();
  for (const auto& function : m.functions)
    for (const auto& block : function.blocks)
      for (const auto& instruction : block.instructions)
        if (instruction.opcode == argos::ir::Opcode::load_capture ||
            instruction.opcode == argos::ir::Opcode::new_object ||
            instruction.opcode == argos::ir::Opcode::make_closure ||
            instruction.opcode == argos::ir::Opcode::call_closure) uses_closures = true;
  for (const auto &function : m.functions)
    for (const auto &block : function.blocks)
      for (const auto &instruction : block.instructions)
        if (instruction.opcode == argos::ir::Opcode::print_string)
          literal_string_constants.insert(instruction.constant);
        else if (instruction.opcode == argos::ir::Opcode::load_string)
          string_constants.insert(instruction.constant);
        else if (instruction.opcode == argos::ir::Opcode::print_int)
          prints_integer = true;
        else if (instruction.opcode == argos::ir::Opcode::print_bool)
          prints_boolean = true;
        else if (instruction.opcode == argos::ir::Opcode::print_double)
          prints_double = true;
        else if (instruction.opcode == argos::ir::Opcode::mod)
          uses_modulo = true;
        else if (instruction.opcode == argos::ir::Opcode::load_const &&
                 function.register_types[instruction.dst] ==
                     argos::ir::ValueType::float64)
          double_constants.insert(instruction.constant);
  const bool needs_print_format =
      prints_integer || prints_boolean || prints_double ||
      !literal_string_constants.empty() || !string_constants.empty();
  if (needs_print_format || uses_modulo || uses_closures || !double_constants.empty()) {
    o << ".section .rodata\n";
    if (prints_integer)
      o << ".Largos_print_int_format:\n  .string \"%d\"\n";
    if (prints_double)
      o << ".Largos_print_double_format:\n  .string \"%.17g\"\n";
    if (prints_boolean || !literal_string_constants.empty())
      o << ".Largos_print_string_format:\n  .string \"%s\"\n";
    if (prints_boolean)
      o << ".Largos_true:\n  .string \"true\"\n"
        << ".Largos_false:\n  .string \"false\"\n";
    for (const auto constant : string_constants) {
      const auto &text = m.constants[constant].value;
      o << ".p2align 3\n.Largos_string_" << constant << ":\n  .quad "
        << text.size() << "\n  .byte ";
      if (text.empty()) {
        o << "0\n";
      } else {
        for (std::size_t index = 0; index < text.size(); ++index) {
          if (index != 0) o << ", ";
          o << static_cast<unsigned int>(
              static_cast<unsigned char>(text[index]));
        }
        o << "\n";
      }
    }
    for (const auto constant : literal_string_constants)
      o << ".Largos_print_literal_" << constant << ":\n  .string "
        << m.constants[constant].value << "\n";
    for (const auto constant : double_constants) {
      double value{};
      const auto &text = m.constants[constant].value;
      const auto [end, error] =
          std::from_chars(text.data(), text.data() + text.size(), value);
      if (error != std::errc{} || end != text.data() + text.size())
        throw std::runtime_error("IR: constante double inválida");
      o << ".Largos_double_" << constant << ":\n  .quad 0x" << std::hex
        << std::bit_cast<std::uint64_t>(value) << std::dec << "\n";
    }
    if (uses_modulo)
      o << ".Largos_division_error:\n"
        << "  .string \"" << division_error_message.substr(0, division_error_message.size() - 1)
        << "\\n\"\n";
  }
  o << ".text\n";
  if (uses_closures) {
    o << ".bss\n.p2align 3\n.Largos_heap_head:\n  .quad 0\n.text\n"
      << ".Largos_alloc:\n  pushq %rbp\n  movq %rsp, %rbp\n"
      << "  addq $8, %rdi\n  call malloc@PLT\n  testq %rax, %rax\n  je .Largos_closure_error\n"
      << "  movq .Largos_heap_head(%rip), %rcx\n  movq %rcx, (%rax)\n"
      << "  movq %rax, .Largos_heap_head(%rip)\n  addq $8, %rax\n  leave\n  ret\n"
      << ".Largos_release:\n  pushq %rbp\n  movq %rsp, %rbp\n"
      << ".Largos_release_loop:\n  movq .Largos_heap_head(%rip), %rdi\n"
      << "  testq %rdi, %rdi\n  je .Largos_release_done\n"
      << "  movq (%rdi), %rax\n  movq %rax, .Largos_heap_head(%rip)\n"
      << "  call free@PLT\n  jmp .Largos_release_loop\n"
      << ".Largos_release_done:\n  leave\n  ret\n"
      << ".Largos_closure_error:\n  movl $70, %edi\n  call exit@PLT\n  ud2\n";
  }

  for (const auto &f : m.functions) {
    const auto name = symbol(f);
    const auto stack_bytes = static_cast<std::uint64_t>(f.slot_count + f.register_count) * 8;
    const auto aligned = (stack_bytes + 15) & ~std::uint64_t{15};
    if (f.name == "main") o << ".globl main\n";
    o << ".type " << name << ", @function\n"
      << name << ":\n  pushq %rbp\n  movq %rsp, %rbp\n";
    if (aligned != 0) o << "  subq $" << aligned << ", %rsp\n";
    if (uses_closures && f.name == "main")
      o << "  leaq .Largos_release(%rip), %rdi\n  call atexit@PLT\n";
    static constexpr const char *integer_argument_registers[] = {
        "%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"};
    static constexpr const char *pointer_argument_registers[] = {
        "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
    static constexpr const char *double_argument_registers[] = {
        "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5"};
    std::size_t integer_parameter = 0;
    std::size_t double_parameter = 0;
    for (std::uint32_t index = 0; index < f.parameter_count; ++index) {
      if (f.parameter_types[index] == argos::ir::ValueType::float64) {
        o << "  movq " << double_argument_registers[double_parameter++]
          << ", %rax\n  movq %rax, " << slot_address(index) << "\n";
      } else if (pointer_type(f.parameter_types[index])) {
        o << "  movq " << pointer_argument_registers[integer_parameter++]
          << ", %rax\n  movq %rax, " << slot_address(index) << "\n";
      } else {
        o << "  movslq " << integer_argument_registers[integer_parameter++]
          << ", %rax\n  movq %rax, " << slot_address(index) << "\n";
      }
    }
    std::size_t division_site = 0;
    std::size_t string_comparison_site = 0;
    for (const auto &b : f.blocks) {
      o << label(f.id, b.label) << ":\n";
      for (const auto &i : b.instructions) {
        validate(m, f, i);
        using argos::ir::Opcode;
        switch (i.opcode) {
        case Opcode::new_object:
          o << "  movq $" << (8 + m.objects[i.constant].fields.size() * 8) << ", %rdi\n"
            << "  call .Largos_alloc\n  movq $" << i.constant << ", (%rax)\n"
            << "  movq %rax, " << register_address(f, i.dst) << "\n";
          break;
        case Opcode::load_field:
        case Opcode::store_field:
          o << "  movq " << register_address(f, i.src1) << ", %rax\n"
            << "  testq %rax, %rax\n  je .Largos_closure_error\n"
            << "  cmpq $" << i.constant << ", (%rax)\n  jne .Largos_closure_error\n";
          if (i.opcode == Opcode::load_field)
            o << "  movq " << 8 + static_cast<std::uint64_t>(i.count) * 8 << "(%rax), %rax\n"
              << "  movq %rax, " << register_address(f, i.dst) << "\n";
          else o << "  movq " << register_address(f, i.src2) << ", %rcx\n"
                 << "  movq %rcx, " << 8 + static_cast<std::uint64_t>(i.count) * 8 << "(%rax)\n";
          break;
        case Opcode::make_closure: {
          const auto& target = m.functions[i.function];
          o << "  movq $" << (24 + i.arguments.size() * 8) << ", %rdi\n  call .Largos_alloc\n"
            << "  leaq " << symbol(target) << "(%rip), %rcx\n  movq %rcx, (%rax)\n"
            << "  movabsq $" << closure_signature(target.parameter_types, target.return_type, true)
            << ", %rcx\n  movq %rcx, 8(%rax)\n  movq $" << i.function << ", 16(%rax)\n";
          for (std::size_t index = 0; index < i.arguments.size(); ++index)
            o << "  movq " << register_address(f, i.arguments[index]) << ", %rcx\n"
              << "  movq %rcx, " << 24 + index * 8 << "(%rax)\n";
          o << "  movq %rax, " << register_address(f, i.dst) << "\n";
          break;
        }
        case Opcode::load_capture:
          o << "  movq " << register_address(f, i.src1) << ", %rax\n"
            << "  testq %rax, %rax\n  je .Largos_closure_error\n"
            << "  cmpq $" << f.id << ", 16(%rax)\n  jne .Largos_closure_error\n"
            << "  movq " << 24 + static_cast<std::uint64_t>(i.count) * 8 << "(%rax), %rax\n"
            << "  movq %rax, " << register_address(f, i.dst) << "\n";
          break;
        case Opcode::call_closure: {
          o << "  movq " << register_address(f, i.src1) << ", %r11\n"
            << "  testq %r11, %r11\n  je .Largos_closure_error\n"
            << "  movabsq $" << closure_signature(i.parameter_types, i.return_type, false)
            << ", %rax\n  cmpq %rax, 8(%r11)\n  jne .Largos_closure_error\n"
            << "  movq %r11, %rdi\n";
          std::size_t integer_argument = 1, double_argument = 0;
          for (std::size_t index = 0; index < i.arguments.size(); ++index) {
            if (i.parameter_types[index] == argos::ir::ValueType::float64)
              o << "  movq " << register_address(f, i.arguments[index]) << ", "
                << double_argument_registers[double_argument++] << "\n";
            else if (pointer_type(i.parameter_types[index]))
              o << "  movq " << register_address(f, i.arguments[index]) << ", "
                << pointer_argument_registers[integer_argument++] << "\n";
            else o << "  movl " << register_address(f, i.arguments[index]) << ", "
                   << integer_argument_registers[integer_argument++] << "\n";
          }
          o << "  call *(%r11)\n";
          if (i.return_type == argos::ir::ValueType::float64) o << "  movq %xmm0, %rax\n";
          else if (!pointer_type(i.return_type)) o << "  movslq %eax, %rax\n";
          o << "  movq %rax, " << register_address(f, i.dst) << "\n";
          break;
        }
        case Opcode::load_const:
          if (f.register_types[i.dst] == argos::ir::ValueType::float64) {
            o << "  movsd .Largos_double_" << i.constant
              << "(%rip), %xmm0\n  movq %xmm0, %rax\n  movq %rax, "
              << register_address(f, i.dst) << "\n";
          } else {
            o << "  movl $" << m.constants[i.constant].value << ", %eax\n"
              << "  movslq %eax, %rax\n  movq %rax, "
              << register_address(f, i.dst) << "\n";
          }
          break;
        case Opcode::load_string:
          o << "  leaq .Largos_string_" << i.constant
            << "(%rip), %rax\n  movq %rax, "
            << register_address(f, i.dst) << "\n";
          break;
        case Opcode::load_var:
          o << "  movq " << slot_address(i.slot) << ", %rax\n"
            << "  movq %rax, " << register_address(f, i.dst) << "\n";
          break;
        case Opcode::store_var:
          o << "  movq " << register_address(f, i.src1) << ", %rax\n"
            << "  movq %rax, " << slot_address(i.slot) << "\n";
          break;
        case Opcode::neg:
        case Opcode::logical_not:
          if (i.opcode == Opcode::neg &&
              f.register_types[i.dst] == argos::ir::ValueType::float64) {
            o << "  movq " << register_address(f, i.src1)
              << ", %rax\n  btcq $63, %rax\n  movq %rax, "
              << register_address(f, i.dst) << "\n";
          } else {
            o << "  movl " << register_address(f, i.src1) << ", %eax\n";
            if (i.opcode == Opcode::neg)
              o << "  negl %eax\n  movslq %eax, %rax\n";
            else
              o << "  testl %eax, %eax\n  sete %al\n  movzbl %al, %eax\n";
            o << "  movq %rax, " << register_address(f, i.dst) << "\n";
          }
          break;
        case Opcode::int_to_double:
          o << "  cvtsi2sdq " << register_address(f, i.src1)
            << ", %xmm0\n  movq %xmm0, %rax\n  movq %rax, "
            << register_address(f, i.dst) << "\n";
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
          if (f.register_types[i.src1] == argos::ir::ValueType::object) {
            o << "  movq " << register_address(f, i.src1) << ", %rax\n"
              << "  cmpq " << register_address(f, i.src2) << ", %rax\n"
              << (i.opcode == Opcode::cmp_eq ? "  sete %al\n" : "  setne %al\n")
              << "  movzbl %al, %eax\n  movq %rax, " << register_address(f, i.dst) << "\n";
          } else if (f.register_types[i.src1] == argos::ir::ValueType::string &&
              (i.opcode == Opcode::cmp_eq || i.opcode == Opcode::cmp_ne)) {
            const auto unequal_label =
                ".Largos_" + std::to_string(f.id) + "_string_unequal_" +
                std::to_string(string_comparison_site);
            const auto done_label =
                ".Largos_" + std::to_string(f.id) + "_string_compare_done_" +
                std::to_string(string_comparison_site++);
            o << "  movq " << register_address(f, i.src1) << ", %rdi\n"
              << "  movq " << register_address(f, i.src2) << ", %rsi\n"
              << "  movq (%rdi), %rdx\n"
              << "  cmpq (%rsi), %rdx\n  jne " << unequal_label << "\n"
              << "  leaq 8(%rdi), %rdi\n  leaq 8(%rsi), %rsi\n"
              << "  call memcmp@PLT\n  testl %eax, %eax\n  "
              << (i.opcode == Opcode::cmp_eq ? "sete" : "setne")
              << " %al\n  jmp " << done_label << "\n"
              << unequal_label << ":\n  movl $"
              << (i.opcode == Opcode::cmp_eq ? "0" : "1") << ", %eax\n"
              << done_label << ":\n  movzbl %al, %eax\n  movq %rax, "
              << register_address(f, i.dst) << "\n";
          } else if (i.opcode == Opcode::div ||
              f.register_types[i.src1] == argos::ir::ValueType::float64 ||
              f.register_types[i.dst] == argos::ir::ValueType::float64) {
            const auto emit_double_operand = [&](argos::ir::Register reg,
                                                  const char *xmm) {
              if (f.register_types[reg] == argos::ir::ValueType::float64)
                o << "  movq " << register_address(f, reg) << ", %rax\n"
                  << "  movq %rax, " << xmm << "\n";
              else
                o << "  cvtsi2sdq " << register_address(f, reg) << ", "
                  << xmm << "\n";
            };
            emit_double_operand(i.src1, "%xmm0");
            emit_double_operand(i.src2, "%xmm1");
            switch (i.opcode) {
            case Opcode::add: o << "  addsd %xmm1, %xmm0\n"; break;
            case Opcode::sub: o << "  subsd %xmm1, %xmm0\n"; break;
            case Opcode::mul: o << "  mulsd %xmm1, %xmm0\n"; break;
            case Opcode::div: o << "  divsd %xmm1, %xmm0\n"; break;
            default: break;
            }
            if (i.opcode == Opcode::cmp_eq || i.opcode == Opcode::cmp_ne ||
                i.opcode == Opcode::cmp_lt || i.opcode == Opcode::cmp_le ||
                i.opcode == Opcode::cmp_gt || i.opcode == Opcode::cmp_ge) {
              o << "  ucomisd %xmm1, %xmm0\n";
              switch (i.opcode) {
              case Opcode::cmp_eq:
                o << "  sete %al\n  setnp %dl\n  andb %dl, %al\n";
                break;
              case Opcode::cmp_ne:
                o << "  setne %al\n  setp %dl\n  orb %dl, %al\n";
                break;
              case Opcode::cmp_lt:
                o << "  setb %al\n  setnp %dl\n  andb %dl, %al\n";
                break;
              case Opcode::cmp_le:
                o << "  setbe %al\n  setnp %dl\n  andb %dl, %al\n";
                break;
              case Opcode::cmp_gt: o << "  seta %al\n"; break;
              case Opcode::cmp_ge: o << "  setae %al\n"; break;
              default: break;
              }
              o << "  movzbl %al, %eax\n  movq %rax, "
                << register_address(f, i.dst) << "\n";
            } else {
              o << "  movq %xmm0, %rax\n  movq %rax, "
                << register_address(f, i.dst) << "\n";
            }
          } else {
            o << "  movl " << register_address(f, i.src1) << ", %eax\n"
              << "  movl " << register_address(f, i.src2) << ", %ecx\n";
            switch (i.opcode) {
            case Opcode::add: o << "  addl %ecx, %eax\n"; break;
            case Opcode::sub: o << "  subl %ecx, %eax\n"; break;
            case Opcode::mul: o << "  imull %ecx, %eax\n"; break;
            case Opcode::mod: {
              const auto ready_label =
                  ".Largos_" + std::to_string(f.id) + "_division_ready_" +
                  std::to_string(division_site++);
              o << "  testl %ecx, %ecx\n"
                << "  je .Largos_" << f.id << "_division_error\n"
                << "  cmpl $-1, %ecx\n"
                << "  jne " << ready_label << "\n"
                << "  cmpl $-2147483648, %eax\n"
                << "  je .Largos_" << f.id << "_division_error\n"
                << ready_label << ":\n"
                << "  cltd\n  idivl %ecx\n";
              o << "  movl %edx, %eax\n";
              break;
            }
            default:
              o << "  cmpl %ecx, %eax\n";
              switch (i.opcode) {
              case Opcode::cmp_eq: o << "  sete %al\n"; break;
              case Opcode::cmp_ne: o << "  setne %al\n"; break;
              case Opcode::cmp_lt: o << "  setl %al\n"; break;
              case Opcode::cmp_le: o << "  setle %al\n"; break;
              case Opcode::cmp_gt: o << "  setg %al\n"; break;
              case Opcode::cmp_ge: o << "  setge %al\n"; break;
              default: break;
              }
              o << "  movzbl %al, %eax\n";
              break;
            }
            if (i.opcode == Opcode::add || i.opcode == Opcode::sub ||
                i.opcode == Opcode::mul || i.opcode == Opcode::mod)
              o << "  movslq %eax, %rax\n";
            o << "  movq %rax, " << register_address(f, i.dst) << "\n";
          }
          break;
        case Opcode::jmp: o << "  jmp " << label(f.id, i.label) << "\n"; break;
        case Opcode::jmp_if_false:
          o << "  movl " << register_address(f, i.src1) << ", %eax\n"
            << "  testl %eax, %eax\n  je " << label(f.id, i.label) << "\n";
          break;
        case Opcode::call:
        {
          const auto &callee = m.functions[i.function];
          std::size_t integer_argument = 0;
          std::size_t double_argument = 0;
          for (std::size_t index = 0; index < i.arguments.size(); ++index) {
            if (callee.parameter_types[index] == argos::ir::ValueType::float64) {
              o << "  movq " << register_address(f, i.arguments[index])
                << ", %rax\n  movq %rax, "
                << double_argument_registers[double_argument++] << "\n";
            } else if (pointer_type(callee.parameter_types[index])) {
              o << "  movq " << register_address(f, i.arguments[index])
                << ", " << pointer_argument_registers[integer_argument++]
                << "\n";
            } else {
              o << "  movl " << register_address(f, i.arguments[index])
                << ", " << integer_argument_registers[integer_argument++]
                << "\n";
            }
          }
          o << "  call " << symbol(callee) << "\n";
          if (f.register_types[i.dst] == argos::ir::ValueType::float64)
            o << "  movq %xmm0, %rax\n  movq %rax, "
              << register_address(f, i.dst) << "\n";
          else if (pointer_type(f.register_types[i.dst]))
            o << "  movq %rax, " << register_address(f, i.dst) << "\n";
          else
            o << "  movslq %eax, %rax\n  movq %rax, "
              << register_address(f, i.dst) << "\n";
          break;
        }
        case Opcode::print_int:
          o << "  movl " << register_address(f, i.src1) << ", %esi\n"
            << "  leaq .Largos_print_int_format(%rip), %rdi\n"
            << "  xorl %eax, %eax\n  call printf@PLT\n";
          break;
        case Opcode::print_bool:
          o << "  movl " << register_address(f, i.src1) << ", %eax\n"
            << "  leaq .Largos_false(%rip), %rsi\n"
            << "  testl %eax, %eax\n"
            << "  leaq .Largos_true(%rip), %rax\n"
            << "  cmovne %rax, %rsi\n"
            << "  leaq .Largos_print_string_format(%rip), %rdi\n"
            << "  xorl %eax, %eax\n  call printf@PLT\n";
          break;
        case Opcode::print_double:
          o << "  movq " << register_address(f, i.src1)
            << ", %xmm0\n  leaq .Largos_print_double_format(%rip), %rdi\n"
            << "  movl $1, %eax\n  call printf@PLT\n";
          break;
        case Opcode::print_string:
          o << "  leaq .Largos_print_literal_" << i.constant << "(%rip), %rsi\n"
            << "  leaq .Largos_print_string_format(%rip), %rdi\n"
            << "  xorl %eax, %eax\n  call printf@PLT\n";
          break;
        case Opcode::print_string_value:
          o << "  movq " << register_address(f, i.src1) << ", %rax\n"
            << "  movq (%rax), %rdx\n"
            << "  leaq 8(%rax), %rdi\n"
            << "  movl $1, %esi\n"
            << "  movq stdout@GOTPCREL(%rip), %rcx\n"
            << "  movq (%rcx), %rcx\n"
            << "  call fwrite@PLT\n";
          break;
        case Opcode::print_newline:
          o << "  movl $10, %edi\n  call putchar@PLT\n";
          break;
        case Opcode::ret:
          if (f.return_type == argos::ir::ValueType::float64)
            o << "  movq " << register_address(f, i.src1)
              << ", %xmm0\n";
          else if (pointer_type(f.return_type))
            o << "  movq " << register_address(f, i.src1) << ", %rax\n";
          else
            o << "  movl " << register_address(f, i.src1) << ", %eax\n";
          o << "  jmp .Largos_" << f.id << "_return\n";
          break;
        default: break;
        }
      }
    }
    const bool function_uses_modulo =
        std::any_of(f.blocks.begin(), f.blocks.end(), [](const auto &block) {
          return std::any_of(block.instructions.begin(), block.instructions.end(),
                             [](const auto &instruction) {
                               return instruction.opcode == argos::ir::Opcode::mod;
                             });
        });
    if (function_uses_modulo) {
      o << ".Largos_" << f.id << "_division_error:\n"
        << "  movl $2, %edi\n"
        << "  leaq .Largos_division_error(%rip), %rsi\n"
        << "  movl $" << division_error_message.size() << ", %edx\n"
        << "  call write@PLT\n"
        << "  movl $70, %edi\n"
        << "  call exit@PLT\n"
        << "  ud2\n";
    }
    o << "  xorl %eax, %eax\n.Largos_" << f.id << "_return:\n  leave\n  ret\n";
    o << ".size " << name << ", .-" << name << "\n";
  }
  o << ".section .note.GNU-stack,\"\",@progbits\n";
  return o.str();
}
} // namespace argos::codegen::native::x86_64
