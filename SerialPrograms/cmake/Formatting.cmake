# Incremental C++ formatting, with no application build dependency.
# cmake --build /path/to/build --target format       # apply eligible edited-line spacing
# cmake --build /path/to/build --target format-check # non-writing check
include_guard(GLOBAL)

# Add explicit targets; defer Python/tool discovery until a formatting target is run.
function(pa_add_format_targets repo_root)
    get_filename_component(format_repo "${repo_root}" ABSOLUTE)
    set(PA_FORMAT_BASE "HEAD" CACHE STRING "Git parent ref for incremental formatting (merge base with HEAD)")
    set(PA_FORMAT_PYTHON "" CACHE FILEPATH "Optional Python 3.8+ interpreter for formatting only")
    foreach(format_target IN ITEMS format format-check)
        if(format_target STREQUAL "format")
            set(format_mode --write)
        else()
            set(format_mode --check)
        endif()
        add_custom_target(${format_target}
            COMMAND "${CMAKE_COMMAND}"
                "-DPA_FORMAT_REPO=${format_repo}"
                "-DPA_FORMAT_BASE=${PA_FORMAT_BASE}"
                "-DPA_FORMAT_MODE=${format_mode}"
                "-DPA_FORMAT_PYTHON=${PA_FORMAT_PYTHON}"
                -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RunFormatting.cmake"
            COMMENT "Incremental C++ ${format_target}: Git-edited lines only"
            USES_TERMINAL
            VERBATIM
        )
    endforeach()
endfunction()
