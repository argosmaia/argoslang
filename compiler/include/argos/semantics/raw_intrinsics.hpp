#pragma once
#include <string_view>
namespace argos::semantics {
inline bool is_raw_intrinsic(std::string_view name) {
  return name == "raw_alloc" || name == "raw_free" || name == "raw_load" ||
         name == "raw_store";
}
} // namespace argos::semantics
