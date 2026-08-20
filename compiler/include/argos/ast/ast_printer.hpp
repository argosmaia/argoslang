#pragma once

#include "argos/ast/ast.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace argos::ast {

class AstPrinter final {
public:
    std::string print(const Program& program);

private:
    void write_node(const Node& node);
    void write_type(const TypeSyntax& type);
    void write_line(std::string_view text);
    void write_label(std::string_view label);

    struct IndentGuard final {
        explicit IndentGuard(AstPrinter& printer) noexcept;
        ~IndentGuard();

        IndentGuard(const IndentGuard&) = delete;
        IndentGuard& operator=(const IndentGuard&) = delete;
        IndentGuard(IndentGuard&&) = delete;
        IndentGuard& operator=(IndentGuard&&) = delete;

    private:
        AstPrinter& printer_;
    };

    std::string output_;
    std::size_t indentation_{0};
};

} // namespace argos::ast
