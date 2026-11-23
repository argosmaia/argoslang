foreach(body IN ITEMS
    "let p = raw_alloc(2)\nraw_free(p)\nraw_free(p)"
    "let p = raw_alloc(2)\nraw_free(p)\nlet value = raw_load(p, 0)"
    "let p = raw_alloc(2)\nraw_store(p, 2, 1)"
    "let p = raw_alloc(2)\nlet value = raw_load(p, -1)"
    "let p = raw_alloc(0)"
    "let mut value: sbyte = 127\nvalue++"
    "let mut value: short = -32768\nvalue--"
    "let mut value: ushort = 65535\nvalue++")
    string(SHA256 id "${body}")
    set(source "${BINARY}-${id}.argos")
    set(binary "${BINARY}-${id}")
    file(WRITE "${source}" "function main() -> int { unsafe {\n${body}\n} return 0 }")
    execute_process(COMMAND "${ARGOSC}" run "${source}" RESULT_VARIABLE status ERROR_VARIABLE error)
    if(NOT status EQUAL 70)
        message(FATAL_ERROR "Manual memory VM expected trap, got ${status}: ${error}")
    endif()
    if(NATIVE)
        execute_process(COMMAND "${ARGOSC}" compile "${source}" -o "${binary}" RESULT_VARIABLE status ERROR_VARIABLE error)
        if(NOT status EQUAL 0)
            message(FATAL_ERROR "Manual memory compilation failed: ${error}")
        endif()
        execute_process(COMMAND "${binary}" RESULT_VARIABLE status ERROR_VARIABLE error TIMEOUT 5)
        if(NOT status EQUAL 70)
            message(FATAL_ERROR "Manual memory native expected trap, got ${status}: ${error}")
        endif()
    endif()
endforeach()
