cmake_minimum_required(VERSION 3.20)

project(
    ArgosLang
    VERSION 0.1.0
    DESCRIPTION "ArgosLang compiler bootstrap"
    LANGUAGES CXX
)

option(ARGOS_BUILD_TESTS "Build ArgosLang tests" ON)
option(ARGOS_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_library(argos_project_options INTERFACE)
target_compile_features(argos_project_options INTERFACE cxx_std_23)
set_target_properties(argos_project_options PROPERTIES
    INTERFACE_CXX_EXTENSIONS OFF
)

add_library(argos_project_warnings INTERFACE)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(argos_project_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Woverloaded-virtual
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
    )
elseif(MSVC)
    target_compile_options(argos_project_warnings INTERFACE
        /W4
        /permissive-
    )
endif()

if(ARGOS_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(argos_project_options INTERFACE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(argos_project_options INTERFACE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
endif()

add_subdirectory(compiler)

if(ARGOS_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
