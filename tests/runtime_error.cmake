execute_process(
    COMMAND "${ARGOSC}" run "${SOURCE}"
    RESULT_VARIABLE run_result
    ERROR_VARIABLE run_error
)
if(NOT run_result EQUAL 70 OR
   NOT run_error MATCHES "módulo por zero ou resultado fora do intervalo")
    message(FATAL_ERROR "Erro de VM inesperado: status=${run_result}; ${run_error}")
endif()

if(DEFINED BINARY)
    execute_process(
        COMMAND "${ARGOSC}" compile "${SOURCE}" -o "${BINARY}"
        RESULT_VARIABLE compile_result
        ERROR_VARIABLE compile_error
    )
    if(NOT compile_result EQUAL 0)
        message(FATAL_ERROR "Compilação falhou: ${compile_error}")
    endif()
    execute_process(
        COMMAND "${BINARY}"
        RESULT_VARIABLE native_result
        ERROR_VARIABLE native_error
        TIMEOUT 5
    )
    if(NOT native_result EQUAL 70 OR
       NOT native_error MATCHES "módulo por zero ou resultado fora do intervalo")
        message(FATAL_ERROR "Erro nativo inesperado: status=${native_result}; ${native_error}")
    endif()
endif()
