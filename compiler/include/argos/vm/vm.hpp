#pragma once
#include "argos/ir/module.hpp"

#include <cstdint>
#include <iosfwd>
#include <vector>

namespace argos::vm {

class VM final {
public:
  void load(const argos::ir::Module &module) { module_ = &module; }
  void set_output(std::ostream &output) noexcept { output_ = &output; }

  // Executes the zero-argument `main` function and returns its Int result.
  // Malformed IR and unsupported instructions are reported as
  // std::runtime_error.
  [[nodiscard]] std::int32_t run() const;

private:
  [[nodiscard]] std::uint64_t execute(
      argos::ir::FunctionId function,
      const std::vector<std::uint64_t> &arguments) const;

  const argos::ir::Module *module_{nullptr};
  std::ostream *output_{nullptr};
};

} // namespace argos::vm
