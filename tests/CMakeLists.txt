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
