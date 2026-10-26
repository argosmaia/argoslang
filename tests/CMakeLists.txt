include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.11.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(Catch2)

add_executable(argos_tests
    diagnostics/diagnostic_tests.cpp
    ir/ir_tests.cpp
    lexer/lexer_tests.cpp
    parser/parser_tests.cpp
    repl/repl_tests.cpp
    semantics/semantic_tests.cpp
)

target_link_libraries(argos_tests
    PRIVATE
        argos_frontend
        argos_project_options
        argos_project_warnings
        Catch2::Catch2WithMain
)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)
catch_discover_tests(argos_tests)

foreach(example IN ITEMS
    minimal_literal
    minimal_arithmetic
    minimal_branch
    minimal_loop
    minimal_optional
    minimal_call
    minimal_nested
    minimal_signed
    minimal_parameters
    minimal_print_int
    minimal_print_bool
    minimal_print_string
    minimal_boolean_call
    minimal_control_flow
    minimal_real_division
)
    add_test(
        NAME "example_${example}_check"
        COMMAND $<TARGET_FILE:argosc> check
                "${PROJECT_SOURCE_DIR}/examples/${example}.argos"
    )
endforeach()
add_test(
    NAME example_minimal_modulo_by_zero_vm
    COMMAND "${CMAKE_COMMAND}"
        "-DARGOSC=$<TARGET_FILE:argosc>"
        "-DSOURCE=${PROJECT_SOURCE_DIR}/examples/minimal_modulo_by_zero.argos"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/runtime_error.cmake"
)
add_test(
    NAME example_minimal_modulo_overflow_vm
    COMMAND "${CMAKE_COMMAND}"
        "-DARGOSC=$<TARGET_FILE:argosc>"
        "-DSOURCE=${PROJECT_SOURCE_DIR}/examples/minimal_modulo_overflow.argos"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/runtime_error.cmake"
)

foreach(example_expected IN ITEMS
    "minimal_literal:42"
    "minimal_arithmetic:14"
    "minimal_branch:7"
    "minimal_loop:6"
    "minimal_call:42"
    "minimal_nested:11"
    "minimal_signed:2"
    "minimal_parameters:42"
    "minimal_print_int:0"
    "minimal_print_bool:0"
    "minimal_print_string:0"
    "minimal_boolean_call:0"
    "minimal_control_flow:8"
    "minimal_break:7"
    "minimal_real_division:0"
)
    string(REPLACE ":" ";" example_parts "${example_expected}")
    list(GET example_parts 0 example)
    list(GET example_parts 1 expected)
    set(expected_stdout "")
    if(example STREQUAL "minimal_print_int")
        set(expected_stdout "42")
    elseif(example STREQUAL "minimal_print_bool")
        set(expected_stdout "true")
    elseif(example STREQUAL "minimal_print_string")
        set(expected_stdout "hello")
    elseif(example STREQUAL "minimal_boolean_call")
        set(expected_stdout "true")
    elseif(example STREQUAL "minimal_real_division")
        set(expected_stdout "0.5\n0.5\n0.5")
    endif()
    add_test(
        NAME "example_${example}_vm"
        COMMAND "${CMAKE_COMMAND}"
            "-DARGOSC=$<TARGET_FILE:argosc>"
            "-DSOURCE=${PROJECT_SOURCE_DIR}/examples/${example}.argos"
            "-DEXPECTED=${expected}"
            "-DEXPECTED_STDOUT=${expected_stdout}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_example.cmake"
    )
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
   CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64)$")
    find_program(ARGOS_ASSEMBLY_CC cc REQUIRED)
    foreach(example_expected IN ITEMS
        "minimal_literal:42"
        "minimal_arithmetic:14"
        "minimal_branch:7"
        "minimal_loop:6"
        "minimal_call:42"
        "minimal_nested:11"
        "minimal_signed:2"
        "minimal_parameters:42"
        "minimal_print_int:0"
        "minimal_print_bool:0"
        "minimal_print_string:0"
        "minimal_boolean_call:0"
        "minimal_control_flow:8"
        "minimal_break:7"
        "minimal_real_division:0"
    )
        string(REPLACE ":" ";" example_parts "${example_expected}")
        list(GET example_parts 0 example)
        list(GET example_parts 1 expected)
        set(expected_stdout "")
        if(example STREQUAL "minimal_print_int")
            set(expected_stdout "42")
        elseif(example STREQUAL "minimal_print_bool")
            set(expected_stdout "true")
        elseif(example STREQUAL "minimal_print_string")
            set(expected_stdout "hello")
        elseif(example STREQUAL "minimal_boolean_call")
            set(expected_stdout "true")
        elseif(example STREQUAL "minimal_real_division")
            set(expected_stdout "0.5\n0.5\n0.5")
        endif()
        add_test(
            NAME "example_${example}_native"
            COMMAND "${CMAKE_COMMAND}"
                "-DARGOSC=$<TARGET_FILE:argosc>"
                "-DSOURCE=${PROJECT_SOURCE_DIR}/examples/${example}.argos"
                "-DBINARY=${CMAKE_BINARY_DIR}/bin/${example}-test"
                "-DEXPECTED=${expected}"
                "-DEXPECTED_STDOUT=${expected_stdout}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/compile_example.cmake"
        )
        add_test(
            NAME "example_${example}_assembly"
            COMMAND "${CMAKE_COMMAND}"
                "-DARGOSC=$<TARGET_FILE:argosc>"
                "-DSOURCE=${PROJECT_SOURCE_DIR}/examples/${example}.argos"
                "-DBINARY=${CMAKE_BINARY_DIR}/bin/${example}-assembly-test"
                "-DEXPECTED=${expected}"
                "-DEXPECTED_STDOUT=${expected_stdout}"
                "-DASSEMBLY=ON"
                "-DCC=${ARGOS_ASSEMBLY_CC}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/compile_example.cmake"
        )
    endforeach()
    add_test(
        NAME example_minimal_literal_short_form
        COMMAND "${CMAKE_COMMAND}"
            "-DARGOSC=$<TARGET_FILE:argosc>"
            "-DSOURCE=${PROJECT_SOURCE_DIR}/examples/minimal_literal.argos"
            "-DBINARY=${CMAKE_BINARY_DIR}/bin/minimal-literal-short-test"
            "-DEXPECTED=42"
            "-DSHORT_FORM=ON"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/compile_example.cmake"
    )
    foreach(example IN ITEMS minimal_optional null_safety)
        add_test(
            NAME "example_${example}_native_rejected"
            COMMAND "${CMAKE_COMMAND}"
                "-DARGOSC=$<TARGET_FILE:argosc>"
                "-DSOURCE=${PROJECT_SOURCE_DIR}/examples/${example}.argos"
                "-DBINARY=${CMAKE_BINARY_DIR}/bin/${example}-unsupported-test"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/compile_rejection.cmake"
        )
    endforeach()
    add_test(
        NAME example_minimal_modulo_by_zero_native
        COMMAND "${CMAKE_COMMAND}"
            "-DARGOSC=$<TARGET_FILE:argosc>"
            "-DSOURCE=${PROJECT_SOURCE_DIR}/examples/minimal_modulo_by_zero.argos"
            "-DBINARY=${CMAKE_BINARY_DIR}/bin/minimal-modulo-by-zero-test"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/runtime_error.cmake"
    )
    add_test(
        NAME example_minimal_modulo_overflow_native
        COMMAND "${CMAKE_COMMAND}"
            "-DARGOSC=$<TARGET_FILE:argosc>"
            "-DSOURCE=${PROJECT_SOURCE_DIR}/examples/minimal_modulo_overflow.argos"
            "-DBINARY=${CMAKE_BINARY_DIR}/bin/minimal-modulo-overflow-test"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/runtime_error.cmake"
    )
endif()
