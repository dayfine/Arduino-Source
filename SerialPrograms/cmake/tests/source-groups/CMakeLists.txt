# Configure-only regression tests; no Qt, compiler or hardware is needed.
# cmake -S SerialPrograms/cmake/tests/source-groups -B /external/path/source-group-tests
# ctest --test-dir /external/path/source-group-tests -C Debug --output-on-failure
cmake_minimum_required(VERSION 3.24)
project(SourceGroupTests NONE)
enable_testing()
get_filename_component(MODULE "../../SourceGroups.cmake" ABSOLUTE)
foreach(CASE IN ITEMS mixed all_generated authored empty)
    add_test(NAME ${CASE} COMMAND "${CMAKE_COMMAND}"
        -S "${CMAKE_CURRENT_SOURCE_DIR}/fixture"
        -B "${CMAKE_CURRENT_BINARY_DIR}/${CASE} output"
        "-DSOURCE_GROUPS_MODULE=${MODULE}" "-DSCENARIO=${CASE}")
endforeach()
add_test(NAME reject_external_authored COMMAND "${CMAKE_COMMAND}"
    "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/fixture"
    "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/invalid output"
    "-DMODULE=${MODULE}" -P "${CMAKE_CURRENT_SOURCE_DIR}/reject-external.cmake")
