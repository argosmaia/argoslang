#include "argos/codegen/native/x86_64/x86_64_codegen.hpp"
#include "argos/ir/validator.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

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
  case Opcode::load_const:
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
    if (instruction.label >= function.blocks.size()) throw std::runtime_error("IR: salto inválido");
    break;
  case Opcode::jmp_if_false:
    valid_reg(instruction.src1);
    if (instruction.label >= function.blocks.size()) throw std::runtime_error("IR: salto inválido");
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
  case Opcode::print_newline: break;
  case Opcode::ret: valid_reg(instruction.src1); break;
  default: throw std::runtime_error("IR: opcode não suportado pelo backend x86-64");
  }
}
} // namespace

std::string CodeGenerator::generate(const argos::ir::Module &m) {
  if (const auto error = argos::ir::validate_module(m))
    throw std::runtime_error("IR inválida: " + *error);
  std::ostringstream o;
  std::unordered_set<argos::ir::ConstantId> string_constants;
  std::unordered_set<argos::ir::ConstantId> double_constants;
  bool prints_integer = false;
  bool prints_boolean = false;
  bool prints_double = false;
  bool uses_modulo = false;
  for (const auto &function : m.functions)
    for (const auto &block : function.blocks)
      for (const auto &instruction : block.instructions)
        if (instruction.opcode == argos::ir::Opcode::print_string)
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
      prints_integer || prints_boolean || prints_double || !string_constants.empty();
  if (needs_print_format || uses_modulo || !double_constants.empty()) {
    o << ".section .rodata\n";
    if (prints_integer)
      o << ".Largos_print_int_format:\n  .string \"%d\"\n";
    if (prints_double)
      o << ".Largos_print_double_format:\n  .string \"%.17g\"\n";
    if (prints_boolean || !string_constants.empty())
      o << ".Largos_print_string_format:\n  .string \"%s\"\n";
    if (prints_boolean)
      o << ".Largos_true:\n  .string \"true\"\n"
        << ".Largos_false:\n  .string \"false\"\n";
    for (const auto constant : string_constants)
      o << ".Largos_string_" << constant << ":\n  .string "
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
  for (const auto &f : m.functions) {
    const auto name = symbol(f);
    const auto stack_bytes = static_cast<std::uint64_t>(f.slot_count + f.register_count) * 8;
    const auto aligned = (stack_bytes + 15) & ~std::uint64_t{15};
    if (f.name == "main") o << ".globl main\n";
    o << ".type " << name << ", @function\n"
      << name << ":\n  pushq %rbp\n  movq %rsp, %rbp\n";
    if (aligned != 0) o << "  subq $" << aligned << ", %rsp\n";
    static constexpr const char *integer_argument_registers[] = {
        "%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"};
    static constexpr const char *double_argument_registers[] = {
        "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5"};
    std::size_t integer_parameter = 0;
    std::size_t double_parameter = 0;
    for (std::uint32_t index = 0; index < f.parameter_count; ++index) {
      if (f.parameter_types[index] == argos::ir::ValueType::float64) {
        o << "  movq " << double_argument_registers[double_parameter++]
          << ", %rax\n  movq %rax, " << slot_address(index) << "\n";
      } else {
        o << "  movslq " << integer_argument_registers[integer_parameter++]
          << ", %rax\n  movq %rax, " << slot_address(index) << "\n";
      }
    }
    std::size_t division_site = 0;
    for (const auto &b : f.blocks) {
      o << label(f.id, b.label) << ":\n";
      for (const auto &i : b.instructions) {
        validate(m, f, i);
        using argos::ir::Opcode;
        switch (i.opcode) {
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
          if (i.opcode == Opcode::div ||
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
          o << "  leaq .Largos_string_" << i.constant << "(%rip), %rsi\n"
            << "  leaq .Largos_print_string_format(%rip), %rdi\n"
            << "  xorl %eax, %eax\n  call printf@PLT\n";
          break;
        case Opcode::print_newline:
          o << "  movl $10, %edi\n  call putchar@PLT\n";
          break;
        case Opcode::ret:
          if (f.return_type == argos::ir::ValueType::float64)
            o << "  movq " << register_address(f, i.src1)
              << ", %xmm0\n";
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
