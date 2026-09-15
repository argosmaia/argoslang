if(SHORT_FORM)
    execute_process(
        COMMAND "${ARGOSC}" "${SOURCE}" -o "${BINARY}"
        RESULT_VARIABLE compile_result
        OUTPUT_VARIABLE compile_output
        ERROR_VARIABLE compile_error
    )
else()
    execute_process(
        COMMAND "${ARGOSC}" compile "${SOURCE}" -o "${BINARY}"
        RESULT_VARIABLE compile_result
        OUTPUT_VARIABLE compile_output
        ERROR_VARIABLE compile_error
    )
endif()
if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR "Compilação falhou: ${compile_error}")
endif()
execute_process(COMMAND "${BINARY}" RESULT_VARIABLE run_result TIMEOUT 5)
if(NOT run_result EQUAL EXPECTED)
    message(FATAL_ERROR "Resultado ${run_result}; esperado ${EXPECTED}")
endif()
