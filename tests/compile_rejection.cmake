execute_process(
    COMMAND "${ARGOSC}" "${SOURCE}" -o "${BINARY}"
    RESULT_VARIABLE compile_result
    ERROR_VARIABLE compile_error
)
if(NOT compile_result EQUAL 65 OR NOT compile_error MATCHES "ARGOS-C0001")
    message(FATAL_ERROR "Era esperado erro de backend: ${compile_result} ${compile_error}")
endif()
if(EXISTS "${BINARY}")
    message(FATAL_ERROR "Arquivo não suportado gerou executável")
endif()
