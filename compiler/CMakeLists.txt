add_library(argos_frontend STATIC
    src/ast/ast_printer.cpp
    src/codegen/ir/ir_generator.cpp
    src/codegen/ir/executable_ir_generator.cpp
    src/diagnostics/diagnostic_engine.cpp
    src/diagnostics/diagnostic_printer.cpp
    src/lexer/lexer.cpp
    src/lexer/token_kind.cpp
    src/parser/parser.cpp
    src/repl/repl_session.cpp
    src/semantics/semantic_analyzer.cpp
    src/source/source_manager.cpp
    src/vm/vm.cpp
)

find_package(ICU REQUIRED COMPONENTS uc)
find_library(ARGOS_READLINE_LIBRARY readline REQUIRED)

target_include_directories(argos_frontend
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(argos_frontend
    PUBLIC
        argos_project_options
        ICU::uc
    PRIVATE
        argos_project_warnings
)

add_executable(argosc
    src/driver/main.cpp
    src/driver/native_compile.cpp
    src/codegen/native/x86_64/x86_64_codegen.cpp
)

target_link_libraries(argosc
    PRIVATE
        argos_frontend
        argos_project_options
        argos_project_warnings
        ${ARGOS_READLINE_LIBRARY}
)
