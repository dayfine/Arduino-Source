execute_process(COMMAND "${CMAKE_COMMAND}" -S "${FIXTURE}" -B "${OUTPUT}"
    "-DSOURCE_GROUPS_MODULE=${MODULE}" -DSCENARIO=invalid
    RESULT_VARIABLE RESULT OUTPUT_VARIABLE STDOUT ERROR_VARIABLE STDERR)
if(RESULT EQUAL 0 OR NOT "${STDOUT}${STDERR}" MATCHES "is not a prefix of file")
    message(FATAL_ERROR "Expected source_group to reject an authored file outside ROOT: ${STDOUT}${STDERR}")
endif()
