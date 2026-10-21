include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.11.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(Catch2)

add_executable(argos_tests
    lexer/lexer_tests.cpp
    parser/parser_tests.cpp
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
)
    add_test(
        NAME "example_${example}_check"
        COMMAND $<TARGET_FILE:argosc> check
                "${PROJECT_SOURCE_DIR}/examples/${example}.argos"
    )
endforeach()

foreach(example_expected IN ITEMS
    "minimal_literal:42"
    "minimal_arithmetic:14"
    "minimal_branch:7"
    "minimal_loop:6"
    "minimal_call:42"
    "minimal_nested:11"
    "minimal_signed:2"
    "minimal_parameters:42"
)
    string(REPLACE ":" ";" example_parts "${example_expected}")
    list(GET example_parts 0 example)
    list(GET example_parts 1 expected)
    add_test(
        NAME "example_${example}_vm"
        COMMAND "${CMAKE_COMMAND}"
            "-DARGOSC=$<TARGET_FILE:argosc>"
            "-DSOURCE=${PROJECT_SOURCE_DIR}/examples/${example}.argos"
            "-DEXPECTED=${expected}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_example.cmake"
    )
endforeach()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
   CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64)$")
    foreach(example_expected IN ITEMS
        "minimal_literal:42"
        "minimal_arithmetic:14"
        "minimal_branch:7"
        "minimal_loop:6"
        "minimal_call:42"
        "minimal_nested:11"
        "minimal_signed:2"
        "minimal_parameters:42"
    )
        string(REPLACE ":" ";" example_parts "${example_expected}")
        list(GET example_parts 0 example)
        list(GET example_parts 1 expected)
        add_test(
            NAME "example_${example}_native"
            COMMAND "${CMAKE_COMMAND}"
                "-DARGOSC=$<TARGET_FILE:argosc>"
                "-DSOURCE=${PROJECT_SOURCE_DIR}/examples/${example}.argos"
                "-DBINARY=${CMAKE_BINARY_DIR}/bin/${example}-test"
                "-DEXPECTED=${expected}"
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
endif()
