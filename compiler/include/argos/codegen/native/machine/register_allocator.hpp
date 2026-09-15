#pragma once
#include <cstdint>
#include <unordered_map>
namespace argos::codegen::native::machine {
using VirtualRegister = std::uint32_t;
class RegisterAllocator final {
  std::unordered_map<VirtualRegister, const char *> assignments_;

public:
  bool has(VirtualRegister v) const noexcept {
    return assignments_.contains(v);
  }
  void bind(VirtualRegister v, const char *p) { assignments_[v] = p; }
  const char *lookup(VirtualRegister v) const noexcept {
    auto i = assignments_.find(v);
    return i == assignments_.end() ? nullptr : i->second;
  }
};
} // namespace argos::codegen::native::machine
