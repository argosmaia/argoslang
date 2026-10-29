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
    minimal_null_safety
    minimal_optional_arrays
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
    minimal_string_values
    minimal_short_circuit
    minimal_closures
    minimal_objects
    minimal_properties
    minimal_object_closure
    minimal_frontend_inheritance
    minimal_polymorphism
    minimal_data
    minimal_sealed_access
    minimal_composition
    minimal_records
    minimal_frontend_bound_method
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
    "minimal_optional:9"
    "minimal_null_safety:0"
    "minimal_optional_arrays:0"
    "minimal_frontend_inheritance:42"
    "minimal_polymorphism:0"
    "minimal_data:0"
    "minimal_sealed_access:0"
    "minimal_composition:0"
    "minimal_records:0"
    "null_safety:0"
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
    "minimal_string_values:0"
    "minimal_short_circuit:0"
    "minimal_closures:0"
    "minimal_objects:0"
    "minimal_properties:0"
    "minimal_object_closure:0"
)
    string(REPLACE ":" ";" example_parts "${example_expected}")
    list(GET example_parts 0 example)
    list(GET example_parts 1 expected)
    set(expected_stdout "")
    if(example STREQUAL "minimal_polymorphism")
        set(expected_stdout "base\nderived\n191919\n202036\n34\n0\neffect\n37\ndefault\n2\n2.5\n79\n12\n3420")
    elseif(example STREQUAL "minimal_composition")
        set(expected_stdout "571\nRio de Janeiromusica\nGPS ativo\nRadio ativo\n8\n9\n11")
    elseif(example STREQUAL "minimal_records")
        set(expected_stdout "Person(name=Ada, age=255, active=true, rating=1.5)\nPerson(name=Ada, age=255, active=true, rating=1.5)\ntruetrue\nAda\nEnvelope(value=Person(name=Ada, age=255, active=true, rating=1.5))truetrue\nEnvelope(value=nil)")
    elseif(example STREQUAL "minimal_sealed_access")
        set(expected_stdout "Ada1001\nAlan2")
    elseif(example STREQUAL "minimal_data")
        set(expected_stdout "truefalsefalse\ntruefalse\ntrue\n8")
    elseif(example STREQUAL "minimal_optional_arrays")
        set(expected_stdout "0\neffect\n9\neffect\n2\n4\n8\n53\n7\ntruehello\n1\ntruetrue\ntruetruetrue\nreceiver\noffset\neffect\n2\n8")
    elseif(example STREQUAL "minimal_null_safety")
        set(expected_stdout "fallback\n9\n0truefalse\n3\n7\n0\nfalsetrue\nfalse\n4\nempty\ntrue\nfalse\n1.5\n832\n10\n8\n22\n99\nindex\n22\n33\n4\n6")
    elseif(example STREQUAL "minimal_closures")
        set(expected_stdout "13\n7")
    elseif(example STREQUAL "minimal_object_closure")
        set(expected_stdout "2\n5\ncaptured\n12\n8\n10\n1\n2.5\nretained\ndone")
    elseif(example STREQUAL "minimal_properties")
        set(expected_stdout "target\nget\nrhs\nset\nget\n5\nget\nset\nget\n2.5\nupdated\ntrue\n5\n6\n4")
    elseif(example STREQUAL "minimal_objects")
        set(expected_stdout "15\n15\na\ntrue")
    elseif(example STREQUAL "minimal_print_int")
        set(expected_stdout "42")
    elseif(example STREQUAL "minimal_print_bool")
        set(expected_stdout "true")
    elseif(example STREQUAL "minimal_print_string")
        set(expected_stdout "hello")
    elseif(example STREQUAL "minimal_boolean_call")
        set(expected_stdout "true")
    elseif(example STREQUAL "minimal_real_division")
        set(expected_stdout "0.5\n0.5\n0.5")
    elseif(example STREQUAL "minimal_string_values")
        set(expected_stdout "olá\nsame\nnul-safe\nline one\nline two")
    elseif(example STREQUAL "minimal_short_circuit")
        set(expected_stdout "nested short-circuit\nshort-circuit")
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
        "minimal_optional:9"
        "minimal_null_safety:0"
        "minimal_optional_arrays:0"
        "minimal_frontend_inheritance:42"
        "minimal_polymorphism:0"
        "minimal_data:0"
        "minimal_sealed_access:0"
        "minimal_composition:0"
        "minimal_records:0"
        "null_safety:0"
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
        "minimal_string_values:0"
        "minimal_short_circuit:0"
        "minimal_closures:0"
        "minimal_objects:0"
        "minimal_properties:0"
        "minimal_object_closure:0"
    )
        string(REPLACE ":" ";" example_parts "${example_expected}")
        list(GET example_parts 0 example)
        list(GET example_parts 1 expected)
        set(expected_stdout "")
        if(example STREQUAL "minimal_polymorphism")
            set(expected_stdout "base\nderived\n191919\n202036\n34\n0\neffect\n37\ndefault\n2\n2.5\n79\n12\n3420")
        elseif(example STREQUAL "minimal_composition")
            set(expected_stdout "571\nRio de Janeiromusica\nGPS ativo\nRadio ativo\n8\n9\n11")
        elseif(example STREQUAL "minimal_records")
            set(expected_stdout "Person(name=Ada, age=255, active=true, rating=1.5)\nPerson(name=Ada, age=255, active=true, rating=1.5)\ntruetrue\nAda\nEnvelope(value=Person(name=Ada, age=255, active=true, rating=1.5))truetrue\nEnvelope(value=nil)")
        elseif(example STREQUAL "minimal_sealed_access")
            set(expected_stdout "Ada1001\nAlan2")
        elseif(example STREQUAL "minimal_data")
            set(expected_stdout "truefalsefalse\ntruefalse\ntrue\n8")
        elseif(example STREQUAL "minimal_optional_arrays")
            set(expected_stdout "0\neffect\n9\neffect\n2\n4\n8\n53\n7\ntruehello\n1\ntruetrue\ntruetruetrue\nreceiver\noffset\neffect\n2\n8")
        elseif(example STREQUAL "minimal_null_safety")
            set(expected_stdout "fallback\n9\n0truefalse\n3\n7\n0\nfalsetrue\nfalse\n4\nempty\ntrue\nfalse\n1.5\n832\n10\n8\n22\n99\nindex\n22\n33\n4\n6")
        elseif(example STREQUAL "minimal_closures")
            set(expected_stdout "13\n7")
        elseif(example STREQUAL "minimal_properties")
            set(expected_stdout "target\nget\nrhs\nset\nget\n5\nget\nset\nget\n2.5\nupdated\ntrue\n5\n6\n4")
        elseif(example STREQUAL "minimal_objects")
            set(expected_stdout "15\n15\na\ntrue")
        elseif(example STREQUAL "minimal_object_closure")
            set(expected_stdout "2\n5\ncaptured\n12\n8\n10\n1\n2.5\nretained\ndone")
        elseif(example STREQUAL "minimal_print_int")
            set(expected_stdout "42")
        elseif(example STREQUAL "minimal_print_bool")
            set(expected_stdout "true")
        elseif(example STREQUAL "minimal_print_string")
            set(expected_stdout "hello")
        elseif(example STREQUAL "minimal_boolean_call")
            set(expected_stdout "true")
        elseif(example STREQUAL "minimal_real_division")
            set(expected_stdout "0.5\n0.5\n0.5")
        elseif(example STREQUAL "minimal_string_values")
            set(expected_stdout "olá\nsame\nnul-safe\nline one\nline two")
        elseif(example STREQUAL "minimal_short_circuit")
            set(expected_stdout "nested short-circuit\nshort-circuit")
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
    foreach(example IN ITEMS minimal_frontend_bound_method)
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

add_test(
    NAME legacy_constructor_rejected_by_all_commands
    COMMAND "${CMAKE_COMMAND}"
        "-DARGOSC=$<TARGET_FILE:argosc>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/legacy_constructor_rejection.cmake"
)

foreach(example_error IN ITEMS "minimal_nil_unwrap:tentativa de desembrulhar nil" "minimal_array_bounds:índice fora dos limites")
    string(REPLACE ":" ";" parts "${example_error}")
    list(GET parts 0 example)
    list(GET parts 1 error)
    add_test(NAME "${example}_runtime_safety"
        COMMAND "${CMAKE_COMMAND}"
            "-DARGOSC=$<TARGET_FILE:argosc>"
            "-DSOURCE=${PROJECT_SOURCE_DIR}/examples/${example}.argos"
            "-DBINARY=${CMAKE_BINARY_DIR}/bin/${example}-safety"
            "-DEXPECTED_ERROR=${error}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/optional_runtime_error.cmake")
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64)$")
    add_test(NAME multi_source_package_isolation
        COMMAND "${CMAKE_COMMAND}" "-DARGOSC=$<TARGET_FILE:argosc>" "-DPROJECT_ROOT=${PROJECT_SOURCE_DIR}" "-DBINARY=${CMAKE_BINARY_DIR}/bin/multi-package-test" -P "${CMAKE_CURRENT_SOURCE_DIR}/multi_source.cmake")
    add_test(NAME exclusive_components_destroyed_once
        COMMAND "${CMAKE_COMMAND}" "-DARGOSC=$<TARGET_FILE:argosc>" "-DCC=${ARGOS_ASSEMBLY_CC}" "-DSOURCE=${PROJECT_SOURCE_DIR}/examples/minimal_composition.argos" "-DBINARY=${CMAKE_BINARY_DIR}/bin/exclusive-components-test" "-DWRAPPER=${CMAKE_CURRENT_SOURCE_DIR}/owned_components.c" -P "${CMAKE_CURRENT_SOURCE_DIR}/owned_components.cmake")
endif()
