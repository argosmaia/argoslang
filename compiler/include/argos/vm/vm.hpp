#pragma once
#include "argos/ir/module.hpp"
namespace argos::vm { class VM final { const argos::ir::Module* module_{nullptr}; public: void load(const argos::ir::Module& m){module_=&m;} void run(); }; }
