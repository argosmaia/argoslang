#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
namespace argos::ir {
using Register = std::uint32_t;
using Slot = std::uint32_t;
using ConstantId = std::uint32_t;
using FunctionId = std::uint32_t;
using LabelId = std::uint32_t;

enum class ValueType : std::uint32_t {
  int32,
  boolean,
  float64,
  string,
  closure,
  object,
  array,
  void_type,
};

// Low four bits identify the payload; bits 4..15 count optional layers.
// For arrays, bits 16..31 hold the (non-array) element type.
// Every present layer is a boxed word, with handle/pointer zero for absence.
constexpr bool is_optional(ValueType type) {
  return (static_cast<std::uint32_t>(type) & 65520u) != 0;
}
constexpr ValueType payload_type(ValueType type) {
  return static_cast<ValueType>(static_cast<std::uint32_t>(type) % 16);
}
constexpr ValueType optional_type(ValueType type) {
  return static_cast<ValueType>(static_cast<std::uint32_t>(type) + 16);
}
constexpr ValueType unwrap_type(ValueType type) {
  return static_cast<ValueType>(static_cast<std::uint32_t>(type) - 16);
}
constexpr ValueType with_depth(ValueType type, std::size_t depth) {
  return static_cast<ValueType>(static_cast<std::uint32_t>(type) + 16 * depth);
}
constexpr ValueType array_element(ValueType type) {
  return static_cast<ValueType>(static_cast<std::uint32_t>(type) >> 16);
}
constexpr ValueType array_type(ValueType element) {
  return static_cast<ValueType>((static_cast<std::uint32_t>(element) << 16) |
                                static_cast<std::uint32_t>(ValueType::array));
}
constexpr bool valid_type(ValueType type, bool allow_void = false) {
  if (payload_type(type) == ValueType::array)
    return array_element(type) < static_cast<ValueType>(65536) &&
           payload_type(array_element(type)) < ValueType::array;
  if (static_cast<std::uint32_t>(type) >= 65536)
    return false;
  return payload_type(type) < ValueType::array ||
         (allow_void && type == ValueType::void_type);
}

struct Constant {
  ConstantId id{};
  std::string value;
};
} // namespace argos::ir
