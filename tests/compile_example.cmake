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
execute_process(
    COMMAND "${BINARY}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error
    TIMEOUT 5
)
if(NOT run_result EQUAL EXPECTED)
    message(FATAL_ERROR "Resultado ${run_result}; esperado ${EXPECTED}; ${run_error}")
endif()
if(DEFINED EXPECTED_STDOUT AND NOT EXPECTED_STDOUT STREQUAL "")
    set(expected_output "${EXPECTED_STDOUT}\n")
    if(NOT run_output STREQUAL expected_output)
        message(FATAL_ERROR "Saída divergente: [${run_output}]; esperada [${expected_output}]")
    endif()
endif()
