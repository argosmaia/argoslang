#include "argos/codegen/native/x86_64/x86_64_codegen.hpp"

#include <sstream>
#include <stdexcept>

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
    break;
  case Opcode::ret: valid_reg(instruction.src1); break;
  default: throw std::runtime_error("IR: opcode não suportado pelo backend x86-64");
  }
}
} // namespace

std::string CodeGenerator::generate(const argos::ir::Module &m) {
  std::ostringstream o;
  o << ".text\n";
  for (const auto &f : m.functions) {
    const auto name = symbol(f);
    const auto stack_bytes = static_cast<std::uint64_t>(f.slot_count + f.register_count) * 8;
    const auto aligned = (stack_bytes + 15) & ~std::uint64_t{15};
    if (f.name == "main") o << ".globl main\n";
    o << ".type " << name << ", @function\n"
      << name << ":\n  pushq %rbp\n  movq %rsp, %rbp\n";
    if (aligned != 0) o << "  subq $" << aligned << ", %rsp\n";
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
          o << "  call " << symbol(m.functions[i.function]) << "\n"
            << "  movslq %eax, %rax\n  movq %rax, " << register_address(f, i.dst) << "\n";
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
