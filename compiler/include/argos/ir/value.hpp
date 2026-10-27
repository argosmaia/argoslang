#pragma once
#include <cstdint>
#include <string>
namespace argos::ir {
using Register = std::uint32_t;
using Slot = std::uint32_t;
using ConstantId = std::uint32_t;
using FunctionId = std::uint32_t;
using LabelId = std::uint32_t;

enum class ValueType : std::uint8_t {
  int32,
  boolean,
  float64,
  string,
  closure,
  object,
  void_type,
};

struct Constant {
  ConstantId id{};
  std::string value;
};
} // namespace argos::ir
