#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace argos::ir {
using Register = std::uint32_t;
using Slot = std::uint32_t;
using ConstantId = std::uint32_t;
using FunctionId = std::uint32_t;
using LabelId = std::uint32_t;

inline constexpr Register invalid_register = UINT32_MAX;
inline constexpr Slot invalid_slot = UINT32_MAX;
inline constexpr FunctionId invalid_function = UINT32_MAX;
inline constexpr LabelId invalid_label = UINT32_MAX;

enum class Opcode : std::uint8_t {
  nop,
  load_const,
  load_var,
  store_var,
  add,
  sub,
  mul,
  div,
  mod,
  cmp_eq,
  cmp_lt,
  jmp,
  jmp_if_false,
  call,
  ret,
  unwrap_check,
  c_embed,
};

struct Constant final {
  enum class Kind : std::uint8_t { integer, string, boolean, null_value };
  Kind kind;
  std::string value;
};

struct Instruction final {
  Opcode opcode{Opcode::nop};
  Register dst{invalid_register}; // Registrador de destino onde será armazenado
                                  // o valor constante
  Register src1{invalid_register};
  Register src2{invalid_register};
  Slot slot{invalid_slot};
  ConstantId constant{0};
  FunctionId function{invalid_function};
  LabelId label{invalid_label};
  std::uint32_t arg_count{0};
  std::string text;
};
struct BasicBlock final {
  LabelId label{invalid_label};
  std::vector<Instruction> instructions;
};

struct Function final {
  FunctionId id{0};
  std::string name;
  std::uint32_t parameter_count{0};
  std::uint32_t slot_count{0};
  std::uint32_t register_count{0};
  std::vector<BasicBlock> blocks;
};

struct Module final {
  std::vector<Constant> constants;
  std::vector<Function> functions;
};

[[nodiscard]] std::string_view opcode_name(Opcode opcode) noexcept;

} // namespace argos::ir