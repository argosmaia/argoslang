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

execute_process(
    COMMAND "${ARGOSC}" run "${SOURCE}"
    RESULT_VARIABLE vm_result
    ERROR_VARIABLE vm_error
)
if(NOT vm_result EQUAL 65 OR NOT vm_error MATCHES "ARGOS-C0001")
    message(FATAL_ERROR "Era esperada rejeição executável na VM: ${vm_result} ${vm_error}")
endif()
