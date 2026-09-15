#pragma once
#include <cstddef>
namespace argos::vm::runtime { inline void* allocate(std::size_t n){return ::operator new(n);} inline void deallocate(void* p) noexcept{::operator delete(p);} }
