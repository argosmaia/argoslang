# Keep legacy syntax only as a rejection fixture; it must never become an alias.
set(source "${CMAKE_CURRENT_BINARY_DIR}/legacy-constructor-rejection.argos")
file(WRITE "${source}" "class Legacy { public init() {} }\nfunction main() -> int { return 0; }\n")
foreach(mode IN ITEMS check run compile)
    execute_process(
        COMMAND "${ARGOSC}" "${mode}" "${source}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 65 OR NOT error MATCHES "init foi removido.*construct")
        message(FATAL_ERROR "${mode} deveria rejeitar init: ${result} ${output} ${error}")
    endif()
endforeach()
