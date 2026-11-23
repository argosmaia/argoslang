execute_process(COMMAND "${ARGOSC}" compile "${SOURCE}" --target=avr -O0 -S -o "${BINARY}.s"
    RESULT_VARIABLE result ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "AVR codegen: ${error}")
endif()
execute_process(COMMAND "${LLVM_MC}" --triple=avr --mcpu=atmega328p --filetype=obj "${BINARY}.s" -o "${BINARY}.o"
    RESULT_VARIABLE result ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "AVR assembly: ${error}")
endif()
file(SIZE "${BINARY}.o" object_size)
if(object_size LESS 100)
    message(FATAL_ERROR "AVR object unexpectedly empty")
endif()
