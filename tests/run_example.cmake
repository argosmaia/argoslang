execute_process(
    COMMAND "${ARGOSC}" run "${SOURCE}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error
)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "Execução pela VM falhou: ${run_error}")
endif()
if(NOT run_output MATCHES "Resultado da VM: ${EXPECTED}")
    message(FATAL_ERROR "Resultado inesperado: ${run_output}; esperado ${EXPECTED}")
endif()
if(DEFINED EXPECTED_STDOUT AND NOT EXPECTED_STDOUT STREQUAL "")
    set(expected_output "${EXPECTED_STDOUT}\nResultado da VM: ${EXPECTED}\n")
    if(NOT run_output STREQUAL expected_output)
        message(FATAL_ERROR "Saída divergente: [${run_output}]; esperada [${expected_output}]")
    endif()
endif()
