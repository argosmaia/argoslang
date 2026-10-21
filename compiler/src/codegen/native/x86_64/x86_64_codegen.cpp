#include "argos/codegen/native/x86_64/x86_64_codegen.hpp"

#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace argos::codegen::native::x86_64 {

namespace {
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
  case Opcode::logical_not: valid_reg(instruction.dst); valid_reg(instruction.src1); break;
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
  case Opcode::print_bool: valid_reg(instruction.src1); break;
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
  std::ostringstream o;
  std::unordered_set<argos::ir::ConstantId> string_constants;
  bool prints_boolean = false;
  for (const auto &function : m.functions)
    for (const auto &block : function.blocks)
      for (const auto &instruction : block.instructions)
        if (instruction.opcode == argos::ir::Opcode::print_string)
          string_constants.insert(instruction.constant);
        else if (instruction.opcode == argos::ir::Opcode::print_bool)
          prints_boolean = true;
  if (!string_constants.empty() || prints_boolean) {
    o << ".section .rodata\n"
      << ".Largos_print_int_format:\n  .string \"%d\"\n"
      << ".Largos_print_string_format:\n  .string \"%s\"\n";
    if (prints_boolean)
      o << ".Largos_true:\n  .string \"true\"\n"
        << ".Largos_false:\n  .string \"false\"\n";
    for (const auto constant : string_constants)
      o << ".Largos_string_" << constant << ":\n  .string "
        << m.constants[constant].value << "\n";
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
    static constexpr const char* argument_registers[] = {"%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"};
    for (std::uint32_t index = 0; index < f.parameter_count; ++index)
      o << "  movl " << argument_registers[index] << ", %eax\n  movslq %eax, %rax\n  movq %rax, " << slot_address(index) << "\n";
    for (const auto &b : f.blocks) {
      o << label(f.id, b.label) << ":\n";
      for (const auto &i : b.instructions) {
        validate(m, f, i);
        using argos::ir::Opcode;
        switch (i.opcode) {
        case Opcode::load_const:
          o << "  movl $" << m.constants[i.constant].value << ", %eax\n"
            << "  movslq %eax, %rax\n  movq %rax, " << register_address(f, i.dst) << "\n";
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
          o << "  movl " << register_address(f, i.src1) << ", %eax\n";
          if (i.opcode == Opcode::neg) o << "  negl %eax\n  movslq %eax, %rax\n";
          else o << "  testl %eax, %eax\n  sete %al\n  movzbl %al, %eax\n";
          o << "  movq %rax, " << register_address(f, i.dst) << "\n";
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
          o << "  movl " << register_address(f, i.src1) << ", %eax\n"
            << "  movl " << register_address(f, i.src2) << ", %ecx\n";
          switch (i.opcode) {
          case Opcode::add: o << "  addl %ecx, %eax\n"; break;
          case Opcode::sub: o << "  subl %ecx, %eax\n"; break;
          case Opcode::mul: o << "  imull %ecx, %eax\n"; break;
          case Opcode::div:
          case Opcode::mod:
            o << "  cltd\n  idivl %ecx\n";
            if (i.opcode == Opcode::mod) o << "  movl %edx, %eax\n";
            break;
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
              i.opcode == Opcode::mul || i.opcode == Opcode::div || i.opcode == Opcode::mod)
            o << "  movslq %eax, %rax\n";
          o << "  movq %rax, " << register_address(f, i.dst) << "\n";
          break;
        case Opcode::jmp: o << "  jmp " << label(f.id, i.label) << "\n"; break;
        case Opcode::jmp_if_false:
          o << "  movl " << register_address(f, i.src1) << ", %eax\n"
            << "  testl %eax, %eax\n  je " << label(f.id, i.label) << "\n";
          break;
        case Opcode::call:
          for (std::size_t index = 0; index < i.arguments.size(); ++index)
            o << "  movl " << register_address(f, i.arguments[index]) << ", " << argument_registers[index] << "\n";
          o << "  call " << symbol(m.functions[i.function]) << "\n"
            << "  movslq %eax, %rax\n  movq %rax, " << register_address(f, i.dst) << "\n";
          break;
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
        case Opcode::print_string:
          o << "  leaq .Largos_string_" << i.constant << "(%rip), %rsi\n"
            << "  leaq .Largos_print_string_format(%rip), %rdi\n"
            << "  xorl %eax, %eax\n  call printf@PLT\n";
          break;
        case Opcode::print_newline:
          o << "  movl $10, %edi\n  call putchar@PLT\n";
          break;
        case Opcode::ret:
          o << "  movl " << register_address(f, i.src1) << ", %eax\n"
            << "  jmp .Largos_" << f.id << "_return\n";
          break;
        default: break;
        }
      }
    }
    o << "  movl $0, %eax\n.Largos_" << f.id << "_return:\n  leave\n  ret\n";
    o << ".size " << name << ", .-" << name << "\n";
  }
  o << ".section .note.GNU-stack,\"\",@progbits\n";
  return o.str();
}
} // namespace argos::codegen::native::x86_64
