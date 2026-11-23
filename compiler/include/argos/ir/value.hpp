#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <mutex>
#include <stdexcept>
#include <vector>
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
  raw_pointer,
};

// Low four bits identify the payload; bits 4..15 count optional layers.
// Arrays store scalar element types in bits 16..31. The high bit marks a
// process-local interned nested element type; no fixed nesting depth is packed
// into the word. A serialized bytecode format will need its own type table.
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
namespace detail {
struct ArrayTypes {
  std::mutex mutex;
  std::vector<ValueType> elements;
};
inline ArrayTypes &array_types() { static ArrayTypes types; return types; }
}
inline ValueType array_element(ValueType type) {
  const auto word = static_cast<std::uint32_t>(type);
  if ((word & 0x80000000u) == 0)
    return static_cast<ValueType>(word >> 16);
  auto &types = detail::array_types();
  const std::lock_guard lock(types.mutex);
  const auto index = (word >> 16) & 0x7fffu;
  return index < types.elements.size() ? types.elements[index] : ValueType::void_type;
}
inline ValueType array_type(ValueType element) {
  if (payload_type(element) == ValueType::array) {
    auto &types = detail::array_types();
    const std::lock_guard lock(types.mutex);
    std::size_t index = 0;
    while (index < types.elements.size() && types.elements[index] != element) ++index;
    if (index == types.elements.size()) {
      if (index >= 32768) throw std::runtime_error("limite de tipos de array aninhado excedido");
      types.elements.push_back(element);
    }
    return static_cast<ValueType>(0x80000000u | (static_cast<std::uint32_t>(index) << 16) |
                                  static_cast<std::uint32_t>(ValueType::array));
  }
  return static_cast<ValueType>((static_cast<std::uint32_t>(element) << 16) |
                                static_cast<std::uint32_t>(ValueType::array));
}
inline bool valid_type(ValueType type, bool allow_void = false) {
  if (payload_type(type) == ValueType::array)
    return valid_type(array_element(type));
  if (static_cast<std::uint32_t>(type) >= 65536)
    return false;
  return payload_type(type) < ValueType::array ||
         payload_type(type) == ValueType::raw_pointer ||
         (allow_void && type == ValueType::void_type);
}

struct Constant {
  ConstantId id{};
  std::string value;
};
} // namespace argos::ir
