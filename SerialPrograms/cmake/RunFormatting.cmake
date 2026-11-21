# Target-time adapter; invoked by Formatting.cmake, not during application configure.
# cmake --build /path/to/build --target format-check
cmake_minimum_required(VERSION 3.24)

if(NOT PA_FORMAT_MODE STREQUAL "--write" AND NOT PA_FORMAT_MODE STREQUAL "--check")
    message(FATAL_ERROR "Use the format or format-check build target.")
endif()
if(PA_FORMAT_PYTHON)
    set(Python3_EXECUTABLE "${PA_FORMAT_PYTHON}")
endif()
find_package(Python3 3.8 QUIET COMPONENTS Interpreter)
if(NOT Python3_Interpreter_FOUND)
    message(FATAL_ERROR "Formatting needs Python 3.8+. Install it or configure PA_FORMAT_PYTHON=/path/to/python3.")
endif()
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${PA_FORMAT_REPO}/devtools/format/format_changed.py"
        --repo "${PA_FORMAT_REPO}" --base "${PA_FORMAT_BASE}" "${PA_FORMAT_MODE}"
    RESULT_VARIABLE format_result
)
if(NOT format_result STREQUAL "0")
    if(PA_FORMAT_MODE STREQUAL "--check" AND format_result STREQUAL "1")
        message(FATAL_ERROR "Edited-line formatting remains. Run the format target, then review and stage its changes.")
    endif()
    message(FATAL_ERROR "Incremental formatter failed (${format_result}); see its diagnostic above. Requires Uncrustify 0.83.0 and Clang; clang-format is not used.")
endif()
