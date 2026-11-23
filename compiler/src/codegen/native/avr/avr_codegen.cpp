#include "argos/codegen/native/avr/avr_codegen.hpp"
#include "argos/ir/validator.hpp"
#include <array>
#include <charconv>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>

namespace argos::codegen::native::avr {
namespace {
using namespace argos::ir;
using Word = std::array<unsigned, 4>;
constexpr Word a{18, 19, 20, 21}, b{22, 23, 24, 25}, temporary{2, 3, 4, 5};
Word parameter(std::size_t n) {
  const auto low = 22u - static_cast<unsigned>(n) * 4;
  return {low, low + 1, low + 2, low + 3};
}
std::string symbol(const Function &f) {
  return f.name == "main" ? "argos_avr_entry"
                          : "argos_avr_fn_" + std::to_string(f.id);
}
std::string label(const Function &f, LabelId id) {
  return ".Largos_avr_" + std::to_string(f.id) + "_" + std::to_string(id);
}
bool scalar(ValueType t) {
  return t == ValueType::int32 || t == ValueType::boolean;
}
void stack_pointer(std::ostringstream &o) {
  o << "  in r0, 0x3f\n  cli\n  out 0x3e, r29\n  out 0x3d, r28\n  out 0x3f, "
       "r0\n";
}
void address(std::ostringstream &o, std::uint32_t offset) {
  const auto negative = (0u - offset) & 65535u;
  o << "  movw r30, r28\n  subi r30, " << (negative & 255u) << "\n  sbci r31, "
    << (negative >> 8) << "\n";
}
void load(std::ostringstream &o, std::uint32_t slot, const Word &regs) {
  address(o, slot * 4 + 1);
  for (std::size_t n = 0; n < 4; ++n)
    o << "  ld r" << regs[n] << ", Z" << (n == 3 ? "\n" : "+\n");
}
void store(std::ostringstream &o, std::uint32_t slot, const Word &regs) {
  address(o, slot * 4 + 1);
  for (std::size_t n = 0; n < 4; ++n)
    o << "  st Z" << (n == 3 ? "" : "+") << ", r" << regs[n] << "\n";
}
void clear(std::ostringstream &o, const Word &regs) {
  for (const auto r : regs)
    o << "  clr r" << r << "\n";
}
void negate(std::ostringstream &o, const Word &regs) {
  for (const auto r : regs)
    o << "  com r" << r << "\n";
  o << "  ldi r26, 1\n  add r" << regs[0] << ", r26\n";
  for (std::size_t n = 1; n < 4; ++n)
    o << "  adc r" << regs[n] << ", r1\n";
}
void compare(std::ostringstream &o, const Word &left, const Word &right) {
  for (std::size_t n = 0; n < 4; ++n)
    o << (n ? "  cpc r" : "  cp r") << left[n] << ", r" << right[n] << "\n";
}
void require_supported(const Module &m) {
  if (auto error = validate_module(m))
    throw std::runtime_error("IR inválida para AVR: " + *error);
  if (!m.objects.empty())
    throw std::runtime_error("AVR: perfil escalar não suporta objetos ou heap");
  for (const auto &f : m.functions) {
    if (f.return_type != ValueType::void_type && !scalar(f.return_type))
      throw std::runtime_error("AVR: retorno exige int, bool ou void");
    for (const auto t : f.register_types)
      if (!scalar(t) && t != ValueType::void_type)
        throw std::runtime_error(
            "AVR: tipo de registrador fora do perfil escalar");
    for (const auto t : f.slot_types)
      if (!scalar(t))
        throw std::runtime_error("AVR: slot exige int ou bool");
    if (!f.capture_types.empty() || f.parameter_count > 6)
      throw std::runtime_error("AVR: assinatura não suportada");
    if ((static_cast<std::uint64_t>(f.slot_count) + f.register_count) * 4 >
        2000)
      throw std::runtime_error(
          "AVR: frame excede o orçamento de SRAM do ATmega328P");
    for (const auto &block : f.blocks)
      for (const auto &i : block.instructions)
        switch (i.opcode) {
        case Opcode::nop:
        case Opcode::load_const:
        case Opcode::load_var:
        case Opcode::store_var:
        case Opcode::add:
        case Opcode::sub:
        case Opcode::mul:
        case Opcode::mod:
        case Opcode::neg:
        case Opcode::logical_not:
        case Opcode::cmp_eq:
        case Opcode::cmp_ne:
        case Opcode::cmp_lt:
        case Opcode::cmp_le:
        case Opcode::cmp_gt:
        case Opcode::cmp_ge:
        case Opcode::jmp:
        case Opcode::jmp_if_false:
        case Opcode::call:
        case Opcode::ret:
        case Opcode::check_uint8:
        case Opcode::check_narrow:
          break;
        default:
          throw std::runtime_error("AVR: opcode fora do perfil escalar: " +
                                   std::string(opcode_name(i.opcode)));
        }
  }
}
} // namespace

std::string CodeGenerator::generate(const Module &m) {
  require_supported(m);
  std::ostringstream o;
  // AVR libc's startup calls an int16 main; Argos functions use 32-bit integer
  // results in r22..r25. Keep this adapter separate from internal main calls.
  o << ".text\n.global main\n.type main, @function\nmain:\n  call "
       "argos_avr_entry\n  movw r24, r22\n  ret\n.size main, .-main\n";
  std::size_t site = 0;
  for (const auto &f : m.functions) {
    const auto frame = (f.slot_count + f.register_count) * 4;
    const auto regslot = [&](Register r) { return f.slot_count + r; };
    o << ".global " << symbol(f) << "\n.type " << symbol(f) << ", @function\n"
      << symbol(f) << ":\n  push r28\n  push r29\n";
    for (unsigned r = 2; r <= 17; ++r)
      o << "  push r" << r << "\n";
    o << "  in r28, 0x3d\n  in r29, 0x3e\n  subi r28, " << (frame & 255u)
      << "\n  sbci r29, " << ((frame >> 8) & 255u) << "\n";
    stack_pointer(o);
    for (std::size_t n = 0; n < f.parameter_count; ++n)
      store(o, static_cast<Slot>(n), parameter(n));
    for (const auto &block : f.blocks) {
      o << label(f, block.label) << ":\n";
      for (const auto &i : block.instructions) {
        const auto stem = ".Largos_avr_site_" + std::to_string(site++);
        const auto read_a = [&] { load(o, regslot(i.src1), a); };
        const auto read_b = [&] { load(o, regslot(i.src2), b); };
        const auto result = [&](const Word &regs) {
          store(o, regslot(i.dst), regs);
        };
        switch (i.opcode) {
        case Opcode::nop:
          break;
        case Opcode::load_const: {
          const auto value = static_cast<std::uint32_t>(
              std::stoll(m.constants[i.constant].value));
          for (std::size_t n = 0; n < 4; ++n)
            o << "  ldi r" << a[n] << ", " << ((value >> (n * 8)) & 255u)
              << "\n";
          result(a);
          break;
        }
        case Opcode::load_var:
          load(o, i.slot, a);
          result(a);
          break;
        case Opcode::store_var:
          read_a();
          store(o, i.slot, a);
          break;
        case Opcode::add:
        case Opcode::sub:
          read_a();
          read_b();
          for (std::size_t n = 0; n < 4; ++n)
            o << "  "
              << (i.opcode == Opcode::add ? (n ? "adc" : "add")
                                          : (n ? "sbc" : "sub"))
              << " r" << a[n] << ", r" << b[n] << "\n";
          result(a);
          break;
        case Opcode::mul:
          read_a();
          read_b();
          clear(o, temporary);
          o << "  ldi r26, 32\n"
            << stem << "_loop:\n  sbrs r18, 0\n  rjmp " << stem << "_shift\n";
          for (std::size_t n = 0; n < 4; ++n)
            o << (n ? "  adc r" : "  add r") << temporary[n] << ", r" << b[n]
              << "\n";
          o << stem
            << "_shift:\n  lsl r22\n  rol r23\n  rol r24\n  rol r25\n  lsr "
               "r21\n  ror r20\n  ror r19\n  ror r18\n  dec r26\n  brne "
            << stem << "_loop\n";
          result(temporary);
          break;
        case Opcode::mod:
          read_a();
          read_b();
          o << "  mov r0, r22\n  or r0, r23\n  or r0, r24\n  or r0, r25\n  "
               "brne "
            << stem << "_nonzero\n  jmp .Largos_avr_trap\n"
            << stem << "_nonzero:\n"
            << "  cpi r18, 0\n  cpc r19, r1\n  cpc r20, r1\n  ldi r26, 128\n  "
               "cpc r21, r26\n  brne "
            << stem << "_valid\n"
            << "  cpi r22, 255\n  ldi r26, 255\n  cpc r23, r26\n  cpc r24, "
               "r26\n  cpc r25, r26\n  brne "
            << stem << "_valid\n  jmp .Largos_avr_trap\n"
            << stem << "_valid:\n"
            << "  mov r6, r21\n  sbrs r21, 7\n  rjmp " << stem
            << "_positive_a\n";
          negate(o, a);
          o << stem << "_positive_a:\n  sbrs r25, 7\n  rjmp " << stem
            << "_positive_b\n";
          negate(o, b);
          clear(o, temporary);
          o << stem << "_positive_b:\n";
          // Clear for both signs of the divisor.
          clear(o, temporary);
          o << "  ldi r26, 32\n"
            << stem
            << "_loop:\n  lsl r18\n  rol r19\n  rol r20\n  rol r21\n  rol r2\n "
               " rol r3\n  rol r4\n  rol r5\n";
          compare(o, temporary, b);
          o << "  brlo " << stem << "_next\n";
          for (std::size_t n = 0; n < 4; ++n)
            o << (n ? "  sbc r" : "  sub r") << temporary[n] << ", r" << b[n]
              << "\n";
          o << stem << "_next:\n  dec r26\n  brne " << stem
            << "_loop\n  sbrs r6, 7\n  rjmp " << stem << "_done\n";
          negate(o, temporary);
          o << stem << "_done:\n";
          result(temporary);
          break;
        case Opcode::neg:
          read_a();
          negate(o, a);
          result(a);
          break;
        case Opcode::check_uint8:
          read_a();
          o << "  mov r0, r19\n  or r0, r20\n  or r0, r21\n  breq " << stem
            << "_valid\n  jmp .Largos_avr_trap\n"
            << stem << "_valid:\n";
          result(a);
          break;
        case Opcode::check_narrow: {
          read_a();
          const auto bits = i.count & 255u;
          const auto low = static_cast<std::uint32_t>(
              i.count & 256u ? 0 : -(1 << (bits - 1)));
          const auto high = static_cast<std::uint32_t>(
              i.count & 256u ? (1 << bits) - 1 : (1 << (bits - 1)) - 1);
          for (std::size_t n = 0; n < 4; ++n)
            o << "  ldi r" << b[n] << ", " << ((low >> (n * 8)) & 255u) << "\n";
          compare(o, a, b);
          o << "  brlt " << stem << "_error\n";
          for (std::size_t n = 0; n < 4; ++n)
            o << "  ldi r" << b[n] << ", " << ((high >> (n * 8)) & 255u)
              << "\n";
          compare(o, b, a);
          o << "  brge " << stem << "_valid\n"
            << stem << "_error:\n  jmp .Largos_avr_trap\n"
            << stem << "_valid:\n";
          result(a);
          break;
        }
        case Opcode::logical_not:
        case Opcode::cmp_eq:
        case Opcode::cmp_ne:
        case Opcode::cmp_lt:
        case Opcode::cmp_le:
        case Opcode::cmp_gt:
        case Opcode::cmp_ge: {
          read_a();
          std::string branch;
          if (i.opcode == Opcode::logical_not) {
            o << "  mov r0, r18\n  or r0, r19\n  or r0, r20\n  or r0, r21\n";
            branch = "breq";
          } else {
            read_b();
            if (i.opcode == Opcode::cmp_gt || i.opcode == Opcode::cmp_le)
              compare(o, b, a);
            else
              compare(o, a, b);
            branch =
                i.opcode == Opcode::cmp_eq   ? "breq"
                : i.opcode == Opcode::cmp_ne ? "brne"
                : (i.opcode == Opcode::cmp_lt || i.opcode == Opcode::cmp_gt)
                    ? "brlt"
                    : "brge";
          }
          o << "  " << branch << " " << stem << "_true\n";
          clear(o, a);
          o << "  rjmp " << stem << "_done\n" << stem << "_true:\n";
          clear(o, a);
          o << "  ldi r18, 1\n" << stem << "_done:\n";
          result(a);
          break;
        }
        case Opcode::jmp:
          o << "  jmp " << label(f, i.label) << "\n";
          break;
        case Opcode::jmp_if_false:
          read_a();
          o << "  mov r0, r18\n  or r0, r19\n  or r0, r20\n  or r0, r21\n  "
               "brne "
            << stem << "_true\n  jmp " << label(f, i.label) << "\n"
            << stem << "_true:\n";
          break;
        case Opcode::call:
          for (std::size_t n = 0; n < i.arguments.size(); ++n)
            load(o, regslot(i.arguments[n]), parameter(n));
          o << "  call " << symbol(m.functions[i.function]) << "\n";
          result(b);
          break;
        case Opcode::ret: {
          if (f.return_type == ValueType::void_type)
            clear(o, b);
          else
            load(o, regslot(i.src1), b);
          const auto negative = (0u - frame) & 65535u;
          o << "  subi r28, " << (negative & 255u) << "\n  sbci r29, "
            << (negative >> 8) << "\n";
          stack_pointer(o);
          for (unsigned r = 17; r >= 2; --r)
            o << "  pop r" << r << "\n";
          o << "  pop r29\n  pop r28\n  ret\n";
          break;
        }
        default:
          throw std::logic_error(
              "opcode AVR passou pela validação sem implementação");
        }
      }
    }
    o << ".size " << symbol(f) << ", .-" << symbol(f) << "\n";
  }
  o << ".Largos_avr_trap:\n  cli\n  break\n  rjmp .Largos_avr_trap\n";
  return o.str();
}
} // namespace argos::codegen::native::avr
