#pragma once
#include <cstdint>
#include <variant>
namespace argos::vm { using Value=std::variant<std::monostate,std::int64_t,double,bool>; }
