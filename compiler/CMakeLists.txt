add_library(argos_frontend STATIC
    src/diagnostics/diagnostic_engine.cpp
    src/lexer/lexer.cpp
    src/lexer/token_kind.cpp
    src/source/source_manager.cpp
)

target_include_directories(argos_frontend
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(argos_frontend
    PUBLIC
        argos_project_options
    PRIVATE
        argos_project_warnings
)

add_executable(argosc
    src/driver/main.cpp
)

target_link_libraries(argosc
    PRIVATE
        argos_frontend
        argos_project_options
        argos_project_warnings
)
