#pragma once

#include "argos/source/source_span.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace argos::ast {
    enum class NodeKind : std::uint8_t {
        program,
        type_syntax,
        parameter,
        
        variable_declaration,
        function_declaration,

        block_statement,
        expression_statement,
        if_statement,
        return_statement,

        binary_expression,
        unary_expression,
        literal_expression,
        identifier_expression,
        call_expression,
        member_access_expression,
        array_expression,
        assignment_expression,
        conditional_expression,
        error_expression,
        integer_literal_expression,
        string_literal_expression,
        boolean_literal_expression,
        null_literal_expression,
        grouping_expression,
        postfix_expression,
    };

    enum class Mutability : std::uint8_t {
        immutable,
        mutable_,
    };
    
    enum class UnaryOperator : std::uint8_t {
        plus,
        minus,
        not_,
    };

    enum class BinaryOperator : std::uint8_t {
        add,
        subtract,
        multiply,
        divide,
        modulo,
        equal,
        not_equal,
        less_than,
        less_than_or_equal,
        greater_than,
        greater_than_or_equal,
        logical_and,
        logical_or,
        lifted_equal,
        null_coalescing,
    };
}