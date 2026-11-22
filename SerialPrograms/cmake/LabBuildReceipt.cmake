# Called after linking a lab executable; never invokes that executable.
set(receipt "${BINARY}.lab-build.json")
if(PROFILE STREQUAL "")
    file(REMOVE "${receipt}")
    return()
endif()
if(NOT PROFILE MATCHES "^[A-Za-z0-9_][A-Za-z0-9_-]*$")
    message(FATAL_ERROR "Invalid lab profile")
endif()
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
    WORKING_DIRECTORY "${SOURCE}" OUTPUT_VARIABLE revision
    OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=normal
    WORKING_DIRECTORY "${SOURCE}" OUTPUT_VARIABLE changes COMMAND_ERROR_IS_FATAL ANY)
set(clean false)
if(changes STREQUAL "")
    set(clean true)
endif()
file(SHA256 "${BINARY}" digest)
file(WRITE "${receipt}" "{\"schema\":1,\"profile\":\"${PROFILE}\",\"source_revision\":\"${revision}\",\"source_clean\":${clean},\"binary_sha256\":\"${digest}\"}\n")
