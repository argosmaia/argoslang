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

  // Executes the zero-argument `main` function and returns its int result.
  // Malformed IR and unsupported instructions are reported as
  // std::runtime_error.
  [[nodiscard]] std::int32_t run() const;

private:
  [[nodiscard]] std::uint64_t execute(
      argos::ir::FunctionId function,
      const std::vector<std::uint64_t> &arguments) const;

  struct Closure {
    argos::ir::FunctionId function;
    std::vector<std::uint64_t> captures;
  };
  mutable std::vector<Closure> closures_;
  mutable std::vector<std::uint64_t> optionals_;
  mutable std::vector<std::vector<std::uint64_t>> arrays_;
  struct Object {
    std::uint32_t layout;
    std::vector<std::uint64_t> fields;
  };
  mutable std::vector<Object> objects_;
  const argos::ir::Module *module_{nullptr};
  std::ostream *output_{nullptr};
};

} // namespace argos::vm
