#pragma once
#include "argos/ir/module.hpp"

#include <cstdint>
#include <vector>

namespace argos::vm {

class VM final {
public:
  void load(const argos::ir::Module &module) { module_ = &module; }

  // Executes the zero-argument `main` function and returns its Int result.
  // Malformed IR and unsupported instructions are reported as
  // std::runtime_error.
  [[nodiscard]] std::int32_t run() const;

private:
  [[nodiscard]] std::int32_t execute(argos::ir::FunctionId function,
                                     const std::vector<std::int32_t>& arguments) const;

  const argos::ir::Module *module_{nullptr};
};

} // namespace argos::vm
